#!/usr/bin/env python

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Generates test_messages.js from an extension message json file.'''

import gzip
import optparse
import sys


def Die(message):
  '''Prints an error message and exit the program.'''
  print >> sys.stderr, message
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
    return open(filename)
  with _OpenFile(in_file_name) as in_file:
    json = in_file.read().strip()
  with open(options.output_file, 'w') as out_file:
    out_file.write(_JS_TEMPLATE % {'in_file': in_file_name, 'json': json})


if __name__ == '__main__':
  main()
