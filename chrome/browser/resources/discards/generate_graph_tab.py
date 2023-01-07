# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Template-combines an HTML and a JSON file and merges them into an output
file as a data: url."""

import argparse
import base64
import os
import string
import sys


def strip_js_exports(js_contents):
  """The input JS may use imports for TS compilation, but these should be used
  for types only and therefore should be stripped by TS compiler.
  TS compiler also generates an "export" statement - remove this. """
  lines = js_contents.splitlines();
  for line in lines:
    assert not line.startswith('import ')

  def not_an_export(line):
    return not line.startswith('export ');
  return '\n'.join(filter(not_an_export, lines))


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

  # Note: Don't turn this into a template. It is a .html.js file output from the
  # TS compiler. TS compiler complains about not finding values if it sees
  # "${foo}" template syntax, so the input html file needs to use a different
  # pattern (see DATA_URL_PLACEHOLDER below).
  output_template = open(args.output_template, 'r').read();

  # Stamp the javacript contents into the HTML template.
  html_doc = html_template.substitute(
      {'javascript_file': strip_js_exports(js_file_contents)})

  # Construct the data: URL that contains the combined doc.
  data_url = "data:text/html;base64,%s" % base64.b64encode(
      html_doc.encode()).decode()

  # And finally stamp the the data URL into the output template.
  DATA_URL_PLACEHOLDER = '{__data_url__}';
  output = output_template.replace(DATA_URL_PLACEHOLDER, data_url);

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
