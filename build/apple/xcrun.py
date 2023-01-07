#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Wrapper around xcrun adding support for --developer-dir parameter to set
the DEVELOPER_DIR environment variable, and for converting paths relative
to absolute (since this is required by most of the tool run via xcrun).
"""

import argparse
import os
import subprocess
import sys


def xcrun(command, developer_dir):
  environ = dict(os.environ)
  if developer_dir:
    environ['DEVELOPER_DIR'] = os.path.abspath(developer_dir)

  processed_args = ['/usr/bin/xcrun']
  for arg in command:
    if os.path.exists(arg):
      arg = os.path.abspath(arg)
    processed_args.append(arg)

  process = subprocess.Popen(processed_args,
                             stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE,
                             universal_newlines=True,
                             env=environ)

  stdout, stderr = process.communicate()
  sys.stdout.write(stdout)
  if process.returncode:
    sys.stderr.write(stderr)
    sys.exit(process.returncode)


def main(args):
  parser = argparse.ArgumentParser(add_help=False)
  parser.add_argument(
      '--developer-dir',
      help='path to developer dir to use for the invocation of xcrun')

  parsed, remaining_args = parser.parse_known_args(args)
  xcrun(remaining_args, parsed.developer_dir)


if __name__ == '__main__':
  main(sys.argv[1:])
