# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Template-combines an HTML and a JSON file and merges them into an output
file as a data: url."""

import argparse
import base64
import os
import string
import sys


def main():
  argument_parser = argparse.ArgumentParser()
  argument_parser.add_argument('output_file', help='The file to write to')
  argument_parser.add_argument('html_template', help='The HTML template file')
  argument_parser.add_argument('javascript_file', help='The JS file')
  argument_parser.add_argument('output_template',
                               help='The output HTML template file')
  args = argument_parser.parse_args()

  # Slurp the input files.
  js_file_contents = open(args.javascript_file, 'r').read();
  html_template = string.Template(open(args.html_template, 'r').read());
  output_template = string.Template(open(args.output_template, 'r').read());

  # Stamp the javacript contents into the HTML template.
  html_doc = html_template.substitute({'javascript_file': js_file_contents});

  # Construct the data: URL that contains the combined doc.
  data_url = "data:text/html;base64,%s" % base64.b64encode(
      html_doc.encode()).decode()

  # And finally stamp the the data URL into the output template.
  output = output_template.safe_substitute({'data_url': data_url})

  current_contents = ''
  if os.path.isfile(args.output_file):
    with open(args.output_file, 'r') as current_file:
      current_contents = current_file.read()

  if current_contents != output:
    with open(args.output_file, 'w') as output_file:
      output_file.write(output)
  return 0


if __name__ == '__main__':
  sys.exit(main())
