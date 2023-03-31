#!/usr/bin/env python3
#
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import shutil
import sys

from util import build_utils
import action_helpers  # build_utils adds //build to sys.path.


def main(args):
  parser = argparse.ArgumentParser(args)
  parser.add_argument('--strip-path', required=True, help='')
  parser.add_argument('--input-path', required=True, help='')
  parser.add_argument('--stripped-output-path', required=True, help='')
  parser.add_argument('--unstripped-output-path', required=True, help='')
  options = parser.parse_args(args)

  # eu-strip's output keeps mode from source file which might not be writable
  # thus it fails to override its output on the next run. AtomicOutput fixes
  # the issue.
  with action_helpers.atomic_output(options.stripped_output_path) as out:
    cmd = [
        options.strip_path,
        options.input_path,
        '-o',
        out.name,
    ]
    build_utils.CheckOutput(cmd)
  shutil.copyfile(options.input_path, options.unstripped_output_path)


if __name__ == '__main__':
  main(sys.argv[1:])
