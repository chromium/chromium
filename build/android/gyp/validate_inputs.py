#!/usr/bin/env python3
#
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Ensures inputs exist and writes a stamp file."""

import argparse
import pathlib
import sys


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--stamp', help='Path to touch on success.')
  parser.add_argument('inputs', nargs='+', help='Files to check.')

  args = parser.parse_args()

  for path in args.inputs:
    path_obj = pathlib.Path(path)
    if not path_obj.is_file():
      if not path_obj.exists():
        sys.stderr.write(f'File not found: {path}\n')
      else:
        sys.stderr.write(f'Not a file: {path}\n')
      sys.exit(1)

  if args.stamp:
    pathlib.Path(args.stamp).touch()


if __name__ == '__main__':
  main()
