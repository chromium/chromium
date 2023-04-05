#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generate java source files from flatbuffer files.

This is the action script for the flatbuffer_java_library template.
"""

import argparse
import sys

from util import build_utils
import action_helpers
import zip_helpers


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--flatc', required=True, help='Path to flatc binary.')
  parser.add_argument('--srcjar', required=True, help='Path to output srcjar.')
  parser.add_argument(
      '--import-dir',
      action='append',
      default=[],
      help='Extra import directory for flatbuffers, can be repeated.')
  parser.add_argument('flatbuffers', nargs='+', help='flatbuffer source files')
  options = parser.parse_args(argv)

  import_args = []
  for path in options.import_dir:
    import_args += ['-I', path]
  with build_utils.TempDir() as temp_dir:
    build_utils.CheckOutput([options.flatc, '-j', '-o', temp_dir] +
                            import_args + options.flatbuffers)

    with action_helpers.atomic_output(options.srcjar) as f:
      zip_helpers.zip_directory(f, temp_dir)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
