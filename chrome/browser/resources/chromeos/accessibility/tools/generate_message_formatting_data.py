#!/usr/bin/env python

# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Generates JavaScript modules containg various plural rules needed to
internationalize messages.
'''

import optparse
import os
import sys
import xml.etree.ElementTree as ElementTree

from pathlib import Path

def TranslateFile(path, new_path, constname):
  xml_tree = ElementTree.parse(path)
  root = xml_tree.getroot().find('plurals')

  content = '''// Copyright 2023 The Chromium Authors
// Use of this source file is governed by a BSD-style license that can be
// found in the LICENSE file.

// ============ THIS IS A GENERATED FILE ============

export const ''' + constname
  content += ''' = {};

'''

  for ruleset in root.findall('pluralRules'):
    for locale in ruleset.get('locales').split():
      content += constname + '[' + locale + '] = [];\n'
      for rule in ruleset.findall('pluralRule'):
        content += constname + '[' + locale + '][' + rule.get('count') + '] = '
        content += '\'' + rule.text + '\';\n'
      content += '\n'

  with open(new_path, 'w') as output:
    output.write(content)

def main():
  parser = optparse.OptionParser(description=__doc__)
  parser.add_option(
      '-i',
      '--input_file',
      action='append',
      default=[],
      dest='inputs',
      metavar='SPEC',
      help=('The input XML files to read'))
  parser.add_option(
    '-o',
    '--output_dir',
    action='store',
    metavar='SPEC',
    help=('Where the output JS file will be written'))

  options, args = parser.parse_args()

  for path in options.inputs:
    base = Path(path).stem
    constname = base.upper() + "_RULES"
    new_filename = base + '_data.js'
    new_path = os.path.join(options.output_dir, new_filename)

    TranslateFile(path, new_path, constname);

if __name__ == '__main__':
  main()
