#!/usr/bin/env python

# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Generates phonetic_dictionaries.js'''

import gzip
import json
import optparse
import os
import sys


HEADER = '''export const PhoneticDictionaries = {};

PhoneticDictionaries.phoneticMap_ = {
'''

CONTENT_TEMPLATE = '''"%(locale)s": %(data)s,
'''

FOOTER = '''};
'''

def quit(message):
  '''Prints an error message and exit the program.'''
  sys.stderr.write(message + '\n')
  sys.exit(1)

def open_file(filename):
    if filename.endswith('.gz'):
        return gzip.open(filename)
    return open(filename)

def main():
    # Parse input.
    parser = optparse.OptionParser(description=__doc__)
    parser.add_option(
      '-o',
      '--output_file',
      action='store',
      metavar='SPEC',
      help=('Where to output the generated file.'))
    options, args = parser.parse_args()
    if options.output_file is None:
        quit('Output file not specified')
    if len(args) != 1:
        quit('Exactly one input directory must be specified')
    dir_name = args[0]
    out_file = options.output_file
    output = HEADER

    # Extract phonetic dictionaries from all compressed message files and write
    # them to a .js file.
    for locale in os.listdir(dir_name):
        locale_dir = os.path.join(dir_name, locale)

        if not os.path.isdir(locale_dir):
            continue

        files = os.listdir(locale_dir)
        if not len(files) == 1:
            continue

        file = files[0]
        file_path = os.path.join(locale_dir,file)
        with open_file(file_path) as in_file:
            contents = json.loads(in_file.read().strip())
            try:
                test = json.loads(contents['CHROMEVOX_PHONETIC_MAP']['message'])
                if sys.version_info >= (3,0):
                    data = contents['CHROMEVOX_PHONETIC_MAP']['message']
                else:
                    # Need to encode utf8 if running python 2.
                    data = (contents['CHROMEVOX_PHONETIC_MAP']['message']
                    .encode('utf-8'))
                locale = locale.replace('_', '-').lower()
                output += CONTENT_TEMPLATE % {'locale': locale, 'data': data}
            except ValueError as e:
                continue

    # Write to file.
    with open(out_file, 'w') as dest_file:
        dest_file.write(output + FOOTER)


if __name__ == '__main__':
    main()
