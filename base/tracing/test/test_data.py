#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
A wrapper script for //third_party/perfetto/tools/test_data. The wrapper
ensures that we upload the correct directory.

Usage:
./test_data.py status     # Prints the status of new & modified files.
./test_data.py download   # To sync remote>local (used by gclient runhooks).
./test_data.py upload     # To upload newly created and modified files.

WARNING: the `download` command will overwrite any locally modified files.
If you want to keep locally modified test data, you should upload it before
running `gclient runhooks` otherwise you will lose this data.
"""

import argparse
import os
import subprocess
import sys

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('cmd', choices=['status', 'download', 'upload'])
  parser.add_argument('--verbose', '-v', action='store_true')
  args = parser.parse_args()

  src_root = os.path.abspath(os.path.join(__file__, '..', '..', '..', '..'))
  perfetto_dir = os.path.join(src_root, 'third_party', 'perfetto')
  tool = os.path.join(perfetto_dir, "tools", "test_data")
  test_dir = os.path.join(src_root, 'base', 'tracing', 'test', 'data')

  command = ['vpython3', tool, '--dir', test_dir, '--overwrite', args.cmd]
  if args.verbose:
    command.append('--verbose')

  completed_process = subprocess.run(
      command,
      check=False,
      capture_output=True)
  sys.stderr.buffer.write(completed_process.stderr)
  sys.stdout.buffer.write(completed_process.stdout)

if __name__ == '__main__':
  sys.exit(main())