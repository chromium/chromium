#!/usr/bin/env python

# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import io
import optparse
import sys
from zipfile import ZipFile

# Unzips archive created with `zip -r ../pumpkin_files.zip *`.
# Archive structure:
#   tagger_wasm_main.wasm
#   tagger_wasm_main.js
#   js_pumpkin_tagger-bin.js
#   en_us/action_config.binarypb
#   en_us/pumpkin_config.binarypb

def UnzipPumpkinFiles(filename, output_dir):
  with ZipFile(filename, 'r') as zf:
    zf.extractall(path=output_dir);

def main():
  parser = optparse.OptionParser(description=__doc__)
  parser.usage = '%prog [options] <zip-file_path>'

  parser.add_option(
      '--output-dir',
      action='store',
      metavar='OUTPUT_DIR',
      help='Output directory for extracted files.')
  options, args = parser.parse_args()
  if len(args) < 1 or not options.output_dir:
      print(
          'Expected --output-dir and the input filename to unzip.',
          file=sys.stderr)
      print(str(args))
      sys.exit(1)

  UnzipPumpkinFiles(args[0], options.output_dir)

if __name__ == '__main__':
  main()