#!/usr/bin/env python3
#
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import subprocess
import sys


def main():
  argparser = argparse.ArgumentParser(description='eu-strip binary.')

  argparser.add_argument('--eu-strip-binary-path', help='eu-strip path.')
  argparser.add_argument('--binary-input', help='exe file path.')
  argparser.add_argument('--symbol-output', help='debug file path.')
  argparser.add_argument('--stripped-binary-output', help='stripped file path.')
  args = argparser.parse_args()

  cmd_line = [
      args.eu_strip_binary_path, '-o', args.stripped_binary_output, '-f',
      args.symbol_output, args.binary_input
  ]

  process = subprocess.Popen(cmd_line)
  process.wait()
  return process.returncode


if __name__ == '__main__':
  sys.exit(main())
