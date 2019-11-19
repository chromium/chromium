#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Extracts an LLD partition from an ELF file."""

import argparse
import subprocess
import sys


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument(
      '--partition',
      help='Name of partition if not the main partition',
      metavar='PART')
  parser.add_argument(
      '--objcopy',
      required=True,
      help='Path to llvm-objcopy binary',
      metavar='FILE')
  parser.add_argument(
      '--unstripped-output',
      required=True,
      help='Unstripped output file',
      metavar='FILE')
  parser.add_argument(
      '--stripped-output',
      required=True,
      help='Stripped output file',
      metavar='FILE')
  parser.add_argument('input', help='Input file')
  args = parser.parse_args()

  objcopy_args = [args.objcopy]
  if args.partition:
    objcopy_args += ['--extract-partition', args.partition]
  else:
    objcopy_args += ['--extract-main-partition']
  objcopy_args += [args.input, args.unstripped_output]
  subprocess.check_call(objcopy_args)

  objcopy_args = [
      args.objcopy, '--strip-all', args.unstripped_output, args.stripped_output
  ]
  subprocess.check_call(objcopy_args)


if __name__ == '__main__':
  sys.exit(main())
