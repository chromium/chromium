#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import pathlib
import sys

_REPO_ROOT = pathlib.Path(__file__).resolve().parents[3]
_ENTRIES_FILE = _REPO_ROOT / '.gclient_entries'


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--source-filter', required=True)
  parser.add_argument('--output', required=True)
  args = parser.parse_args()

  source_filter = args.source_filter

  # Ninja validates that the file exists since it's marked as an input.
  try:
    text = _ENTRIES_FILE.read_text()
    result = {}
    exec(text, result)
    entries = result['entries']
    private_dirs = sorted(d for d, s in entries.items()
                          if s and source_filter in s)
  except Exception as e:
    # Make the test fail rather than the compile step so that failures here do
    # not prevent other bot functionality.
    private_dirs = [
        '# ERROR parsing .gclient_entries',
        str(e), '', 'File was:', text
    ]

  pathlib.Path(args.output).write_text('\n'.join(private_dirs) + '\n')


if __name__ == '__main__':
  main()
