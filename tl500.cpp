/*
* libusb example to communicate with the TL-500
* Copyright (C) 2009 Kornelius Rohmeyer <kornl@gmx.de>
* based on the libusb example program to list devices on the bus
* Copyright (C) 2007 Daniel Drake <dsd@gentoo.org>
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <libusb.h>
#include <ctype.h>
#include <math.h>
#include <getopt.h>


uint16_t VENDOR = 1105; /* 0451 */
uint16_t PRODUCT = 12817; /* 3211 */

libusb_device_handle** find_tl500(libusb_device **devs) {
    libusb_device *dev;
    libusb_device_handle **handle;
    int i = 0;

    printf("Trying to find Arexx logging system.\n");
    while ((dev = devs[i++]) != NULL) {
        struct libusb_device_descriptor desc;
        int r = libusb_get_device_descriptor(dev, &desc);
        if (r < 0) {
            fprintf(stderr, "failed to get device descriptor");
            return NULL;
        }

        printf("%04x:%04x (bus %d, device %d)\n",
            desc.idVendor, desc.idProduct,
            libusb_get_bus_number(dev), libusb_get_device_address(dev));

        if (desc.idVendor == VENDOR && desc.idProduct == PRODUCT) {
            printf("Found Arexx TL-500.\n");
            int usb_open = libusb_open(dev, handle);
            if (usb_open==0) {
              printf("libusb_open successful.\n");
            	return handle;
            }
            fprintf(stderr, "libusb_open failed. Error code %d.\n", usb_open);
        }
    }

    return NULL;
}

static char* get_date() {
	time_t timer=time(NULL);
	return asctime(localtime(&timer));
}

static int get_sensor(unsigned char data[64]) {
	return (data[3])*(256)+(data[2]);
}

static int get_value(unsigned char data[64]) {
	int value = 0;
	for (int i=4; i<=5; i++) {
		value += data[i]*pow(256, 5-i);
	}
	return value;
}

static int get_time(unsigned char data[64]) {
	int value = 0;
	for (int i=9; i>=6; i--) {
		value += data[i]*pow(256, i-6);
	}
	return value;
}

/**
 * We assume that all TSN-TH70E sensors have ids bigger than 10000.
 * If the id is then odd we have a humidity sensor.
 * Has anyone more information about this?
 */
static double get_measurement(unsigned char data[64]) {
	double value = get_value(data);
	int sensor = get_sensor(data);
	if (sensor < 10000) {
		return value * 0.0078;
	} else {
		if (sensor%2==0) {
	       return -39.58 + value * 0.01;
		} else {
		   return 0.6 + value * 0.03328;
		}
	}
}

/**
 * We assume that all TSN-TH70E sensors have ids bigger than 10000.
 * If the id is then odd we have a humidity sensor.
 * Has anyone more information about this?
 */
const char* get_unit(unsigned char data[64]) {
	int sensor = get_sensor(data);
	if (sensor > 10000 && sensor%2!=0) {
		return "%RH";
	} else {
		return "°C";
	}
}

enum output_type
  {
    output_verbose,			/* default */
    output_csv,				/* -v 1 */
    output_raw				/* -v 2 */
  };

static enum output_type output_type;

static void print_data(unsigned char data[64]) {
	switch (output_type) {
	case output_verbose:
		printf("Received data %s ", get_date());
		for(int j=0;j<64;j++) {
			printf("%02x ",data[j]);
		}
		printf("\n");
		printf("From sensor %d we get a raw value %d. We guess this means %3.2f %s. Time: %d\n", get_sensor(data),
				get_value(data), get_measurement(data), get_unit(data), get_time(data));
		break;
	case output_csv:
		printf("%d, %d, %3.2f, %s, %d, %s, ", get_sensor(data),
				get_value(data), get_measurement(data), get_unit(data), get_time(data), get_date());
		for(int j=0;j<64;j++) {
			printf("%02x",data[j]);
		}
		printf("\n");
		break;
	case output_raw:
		for(int j=0;j<64;j++) {
			printf("%02x",data[j]);
		}
		printf("\n");
		break;
	}
}

int main(int argc, char **argv) {
	time_t seconds = time (NULL);
    printf ("%ld seconds since January 1, 1970", seconds);

	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
				{"help", 0, 0, 'h'},
				{"format", 1, 0, 'f'}
		};

		int c = getopt_long (argc, argv, "hvf:",
				long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'h':
			printf ("Usage: tl500 [OPTION]... \n");
			printf("\
Communicates with the Arexx TL 500.\n\
\n\
Mandatory arguments to long options are mandatory for short options too.\n\
\
  -f, --format=STYLE  Output format:\n\
          0 (default): Data: 00 0a 72 22 0c ...\n\
            From sensor 8818 we get a raw value 3095.\n\
            We guess this means 24.14 °C.\n\
          1 (csv): 8818,3095,24.14, °C, 000a72220c...\n\
          2 (raw): 000a72220c...\n");
			return 0;

		case 'f':
			switch (*optarg) {
				case '1':
					output_type = output_csv;
					break;
				case '2':
					output_type = output_raw;
					break;
			}
			break;

		default:
			printf ("Warning: Unknown option 0%o ??\n", c);
		}
	}

    libusb_device **devs;
    int r;
    ssize_t cnt;

    r = libusb_init(NULL);
    if (r < 0)
        return r;

    cnt = libusb_get_device_list(NULL, &devs);
    if (cnt < 0)
        return (int) cnt;

    libusb_device_handle **handle = find_tl500(devs);

    if (handle==NULL) {
        fprintf(stderr, "No logging system found.\n");
        libusb_free_device_list(devs, 1);
        libusb_exit(NULL);
        return -1;
    }

    unsigned char ENDPOINT_DOWN = 0x1;
    unsigned char ENDPOINT_UP = 0x81;
    int actual_length;
    unsigned char dataUp[64];
    unsigned char dataDown[64];
    for(int j=0;j<64;j++) { dataDown[j] = 0; }
    dataDown[0] = 4;
    libusb_bulk_transfer(handle[0], ENDPOINT_DOWN, dataDown, sizeof(dataDown), &actual_length, 0);
    dataDown[0] = 3;
    for(int i=0; i<10000; i++) {
        r = libusb_bulk_transfer(handle[0], ENDPOINT_DOWN, dataDown,
                                 sizeof(dataDown), &actual_length, 0);
        r = libusb_bulk_transfer(handle[0], ENDPOINT_UP, dataUp,
                                 sizeof(dataUp), &actual_length, 1000);
        if (r == 0 && actual_length == sizeof(dataUp)) {
            if (dataUp[0]!=0 || dataUp[1]!=0) {
            	print_data(dataUp);
            }
            sleep(1);
        } else {
            printf("Something went wrong (r == %i, actual_length == %i , sizeof(data) == %lu ).\n",
                   r, actual_length, sizeof(dataUp));
        }
    }

    libusb_free_device_list(devs, 1);

    libusb_exit(NULL);

    return 0;
}
