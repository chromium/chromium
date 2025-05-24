#!/usr/bin/env vpython3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Writes all command line arguments to a file, separated by newlines.

import argparse
import os
import sys

# Set up path to be able to import action_helpers
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir,
                 os.pardir, 'build'))
import action_helpers


def main():
  parser = argparse.ArgumentParser(description='Run Rust build script.')
  parser.add_argument('--output', required=True, help='output file')
  args, flags = parser.parse_known_args()
  # AtomicOutput will ensure we only write to the file on disk if what we
  # give to write() is different than what's currently on disk.
  with action_helpers.atomic_output(args.output) as output:
    output.write(b'\n'.join([f.encode('utf-8') for f in flags]))


if __name__ == '__main__':
  sys.exit(main())
