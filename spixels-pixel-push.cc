// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
//  PixelPusher protocol implementation for LED matrix
//
//  Copyright (C) 2013 Henner Zeller <h.zeller@acm.org>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "led-strip.h"
#include "multi-spi.h"

#include "pp-server.h"
#include "pp-server/lib/universal-discovery-protocol.h"

static const int kMaxUDPPacketSize = 65507;  // largest practical w/ IPv4 header
static const int kDefaultUDPPacketSize = 1460;


// APA102

// Make faster or slower depending on how well the data lines work.
// This was set at 4.  This limited frame rate to about 12FPS
// with 16 outputs and 480 LEDs/output.  Too slow!
// 12 MHz seems to work fine.
// 16MHz causes APA102s past about 200 in the sequence to start freaking out.
static const int kAPA102ClockMhz = 12;

class APA102SpixelsDevice : public pp::OutputDevice {
public:
  APA102SpixelsDevice(int num_strips, int strip_len)
    : num_strips_(num_strips),
      strip_len_(strip_len),
      strips_(new spixels::LEDStrip* [ num_strips_ ]),
      spi_(spixels::CreateDirectMultiSPI(kAPA102ClockMhz))
  {
    for (int strip = 0; strip < num_strips_; ++strip)
    {
      strips_[strip] = spixels::CreateAPA102Strip(spi_,
      												spixels::MultiSPI::SPIPinForConnector(strip + 1),
      												strip_len_);
    }
  }

  ~APA102SpixelsDevice()
  {
    for (int strip = 0; strip < num_strips_; ++strip) delete strips_[strip];
    delete [] strips_;
    delete spi_;
  }

  virtual int num_strips() const { return num_strips_; }
  virtual int num_pixel_per_strip() const { return strip_len_; }

	virtual void HandlePusherCommand(const char *buf, size_t size)
	{
		if (size >= 1)
		{
		 	switch (buf[0])
		 	{
		 	case PPPusherCommandGlobalBrightness :
		 		if (size >= 3)
		 		{
		 			// 0x0FFFF is full brightness for PP
		 			// 0x10000 if full brightness for spixels
		 			// we could scale but it's probably just as accurate to just add 1
		 			// APA102 will throw away the 11 LSbits anyway
			  		uint32_t	const brightnessScale16 = (uint32_t)*(uint16_t*)(buf + 1) + 1;

//		 			fprintf(stderr, "PPPusherCommandGlobalBrightness received - 0x%04X\n", brightnessScale16);
		 			for (int strip = 0; strip < num_strips_; strip++)
		 			{
		 				strips_[strip]->SetBrightnessScale(brightnessScale16 / 65536.0f);
		 			}
		 		}
		 		break;
		 	case PPPusherCommandStripBrightness :
			  	if (size >= 4)
			  	{
			  		uint32_t		const strip = buf[1];
			  		
			  		if (strip < (uint32_t)num_strips_)
			  		{
			 			// 0x0FFFF is full brightness for PP
			 			// 0x10000 if full brightness for spixels
			 			// we could scale but it's probably just as accurate to just add 1
			 			// APA102 will throw away the 11 LSbits anyway
				  		uint32_t	const brightnessScale16 = (uint32_t)*(uint16_t*)(buf + 2) + 1;
				  		
//			 			fprintf(stderr, "PPPusherCommandStripBrightness received - 0x%04X for strip %u\n",
//			 					brightnessScale16, strip);
			  			strips_[strip]->SetBrightnessScale(brightnessScale16 / 65536.0f);
			  		}
			  	}
			  	break;
			default :
			 	fprintf(stderr, "HandlePusherCommand() - unknown command:%u\n", buf[0]);
				break;
		 	}
		}
	}
  
	virtual void SetPixel(uint32_t strip, uint32_t pixel, const ::pp::PixelColor &col)
	{
		if (strip < (uint32_t)num_strips_)
		{
			strips_[strip]->SetPixel8(pixel, col.red, col.green, col.blue);
		}
	}

	virtual void FlushFrame()
	{
		spi_->SendBuffers();
	}

private:
  const int num_strips_;
  const int strip_len_;
  spixels::LEDStrip **strips_;
  spixels::MultiSPI *spi_;
};



// LPD6803 - not yet tested with hardware

static const int kLPD6308ClockMhz = 4;

class LPD6803SpixelsDevice : public pp::OutputDevice {
public:
  LPD6803SpixelsDevice(int num_strips, int strip_len)
    : num_strips_(num_strips),
      strip_len_(strip_len),
      strips_(new spixels::LEDStrip* [ num_strips_ ]),
      spi_(spixels::CreateDirectMultiSPI(kLPD6308ClockMhz))
  {
    for (int strip = 0; strip < num_strips_; ++strip)
    {
      strips_[strip] = spixels::CreateLPD6803Strip(spi_,
      												spixels::MultiSPI::SPIPinForConnector(strip + 1),
      												strip_len_);
    }
  }

  ~LPD6803SpixelsDevice()
  {
    for (int strip = 0; strip < num_strips_; ++strip) delete strips_[strip];
    delete [] strips_;
    delete spi_;
  }

  virtual int num_strips() const { return num_strips_; }
  virtual int num_pixel_per_strip() const { return strip_len_; }

	virtual void SetPixel(uint32_t strip, uint32_t pixel, const ::pp::PixelColor &col)
	{
		if (strip < (uint32_t)num_strips_)
		{
			strips_[strip]->SetPixel8(pixel, col.red, col.green, col.blue);
		}
	}

	virtual void FlushFrame()
	{
		spi_->SendBuffers();
	}

private:
  const int num_strips_;
  const int strip_len_;
  spixels::LEDStrip **strips_;
  spixels::MultiSPI *spi_;
};



static int usage(const char *progname) {
  fprintf(stderr, "usage: %s <options>\n", progname);
  fprintf(stderr, "Options:\n"
          "\t-S <strips>   : Number of connected LED strips (default: 16)\n"
          "\t-L <len>      : Length of LED strips (default: 480)\n"
          "\t-i <iface>    : network interface, such as eth0, wlan0. "
          "Default eth0\n"
          "\t-G <group>    : PixelPusher group (default: 0)\n"
          "\t-C <controller> : PixelPusher controller (default: 0)\n"
          "\t-a <artnet-universe,artnet-channel>: if used with artnet. Default 0,0\n"
          "\t-u <udp-size> : Max UDP data/packet (default %d)\n"
          "\t                Best use the maximum that works with your network (up to %d).\n",
          kDefaultUDPPacketSize, kMaxUDPPacketSize);

  return 1;
}

int main(int argc, char *argv[]) {
  pp::PPOptions pp_options;
  pp_options.artnet_universe = -1;
  pp_options.artnet_channel = -1;
  pp_options.network_interface = "eth0";
  int num_strips = 8;
  int strip_len = 480;

  int opt;
  while ((opt = getopt(argc, argv, "S:L:i:u:a:G:C:")) != -1) {
    switch (opt) {
    case 'S':
      num_strips = atoi(optarg);
      break;
    case 'L':
      strip_len = atoi(optarg);
      break;
    case 'i':
      pp_options.network_interface = strdup(optarg);
      break;
    case 'u':
      pp_options.udp_packet_size = atoi(optarg);
      break;
    case 'G':
      pp_options.group = atoi(optarg);
      break;
    case 'C':
      pp_options.controller = atoi(optarg);
      break;
    case 'a':
      if (2 != sscanf(optarg, "%d,%d",
                      &pp_options.artnet_universe, &pp_options.artnet_channel)) {
        fprintf(stderr, "Artnet parameters must be <universe>,<channel>\n");
        return 1;
      }
      break;
    default:
      return usage(argv[0]);
    }
  }

  // Some parameter checks.
  if (getuid() != 0) {
    fprintf(stderr, "Must run as root to be able to access /dev/mem\n"
            "Prepend 'sudo' to the command:\n\tsudo %s ...\n", argv[0]);
    return 1;
  }

	APA102SpixelsDevice pixel_strips(num_strips, strip_len);
//	LPD6803SpixelsDevice pixel_strips(num_strips, strip_len);
  
  if (!pp::StartPixelPusherServer(pp_options, &pixel_strips)) {
    return 1;
  }

  for(;;) sleep(INT_MAX);
  pp::ShutdownPixelPusherServer();

  return 0;
}
