#!/usr/bin/env python
#
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import shutil
import sys

from util import build_utils


def main(args):
  parser = argparse.ArgumentParser(args)
  parser.add_argument('--strip-path', required=True, help='')
  parser.add_argument('--input-path', required=True, help='')
  parser.add_argument('--stripped-output-path', required=True, help='')
  parser.add_argument('--unstripped-output-path', required=True, help='')
  options = parser.parse_args(args)

  cmd = [
      options.strip_path,
      options.input_path,
      '-o',
      options.stripped_output_path,
  ]

  build_utils.CheckOutput(cmd)
  shutil.copyfile(options.input_path, options.unstripped_output_path)


if __name__ == '__main__':
  main(sys.argv[1:])
