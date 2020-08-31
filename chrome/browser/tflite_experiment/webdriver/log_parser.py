# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import sys
import json
import csv


class LogParser():
    """The histogram log parser.

    This class is the tool that is used to convert histogram logs
    of the experiments to presentable formats like CSV.

    Attributes:
        _flags: A Namespace object from the call to parse_flags()
    """

    def __init__(self):
        self._flags = parse_flags()

    # Generate CSV file from recorded histograms.
    def generate_csv(self):
        if not self._flags.log_file or not self._flags.csv_file:
            return

        # Adds entries based on URL.
        url_list = {}
        with open(self._flags.log_file, 'r') as f:
            for line in f:
                # Check if this line is a valid JSON histogram log.
                if line.find('url') == -1 or line.find('name') == -1 or \
                  line.find('value') == -1 or line.find('tflite') == -1:
                    continue

                # Find starting point of the JSON substring
                # in this line.
                start = line.find('INFO')
                data_string = line[start + 6:len(line)]
                data_string = data_string.replace("\'", "\"")
                data_string = data_string.rstrip("\r")
                data_string = data_string.rstrip("\n")
                data = json.loads(data_string)

                url = data['url']
                url = url.rstrip('\n')
                url = url.rstrip('\r')
                if url not in url_list.keys():
                    url_list[url] = {}

                name = data['name'] + ',' + str(data['tflite'])
                url_list[url][name] = data['value']

        with open(self._flags.csv_file, 'w') as csvfile:
            csv_writer = csv.writer(csvfile, delimiter=',')
            csv_header = ['url']

            # Write CSV file header.
            for key, val in url_list.iteritems():
                for key1, val1 in val.iteritems():
                    csv_header.append(key1)
                break
            csv_writer.writerow(csv_header)

            # Sort based on URL.
            sorted(url_list.items())

            # Write data row.
            for key, val in url_list.iteritems():
                url = [key][0]
                csv_data = [url]
                for key1 in csv_header[1:]:
                    if key1 in val.keys():
                        csv_data.append(val[key1])
                    else:
                        csv_data.append(0)
                csv_writer.writerow(csv_data)


def parse_flags():
    """Parses the given command line arguments.
  
    Returns:
        A new Namespace object with class properties for each argument added below.
        See pydoc for argparse.
    """
    parser = argparse.ArgumentParser()
    parser.add_argument('--log_file',
                        type=str,
                        help='Path to the input log file.')
    parser.add_argument('--csv_file',
                        type=str,
                        help='Path to the outut CSV file.')
    return parser.parse_args(sys.argv[1:])


if __name__ == '__main__':
    parser = LogParser()
    parser.generate_csv()
