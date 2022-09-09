#!/usr/bin/env python3

# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Generates test_messages.js from an extension message json file.'''

import gzip
import io
import optparse
import sys

logfp = open('/tmp/pylog.txt', 'a+')
logfp.write('generate_test_messages.py ' + sys.version + '\n')
logfp.close()


def Die(message):
  '''Prints an error message and exit the program.'''
  print(message, file=sys.stderr)
  sys.exit(1)


# Tempalte for the test_messages.js.
_JS_TEMPLATE = '''// GENERATED FROM %(in_file)s

goog.provide('TestMessages');

TestMessages = %(json)s;
'''


def main():
  parser = optparse.OptionParser(description=__doc__)
  parser.add_option(
      '-o',
      '--output_file',
      action='store',
      metavar='SPEC',
      help=('Where to output the generated deps file.'))
  options, args = parser.parse_args()
  if options.output_file is None:
    Die('Output file not specified')
  if len(args) != 1:
    Die('Exactly one input file must be specified')
  in_file_name = args[0]
  def _OpenFile(filename):
    if filename.endswith('.gz'):
      return gzip.open(filename)
    return open(filename, 'rb')
  with _OpenFile(in_file_name) as in_file:
    json = in_file.read().decode('utf-8').strip()
  with io.open(options.output_file, 'w', encoding='utf-8') as out_file:
    out_file.write(_JS_TEMPLATE % {'in_file': in_file_name, 'json': json})


if __name__ == '__main__':
  main()
