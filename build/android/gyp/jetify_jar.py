#!/usr/bin/env python3
#
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import argparse
import os
import subprocess
import sys

from util import build_utils


def _AddArguments(parser):
  """Adds arguments related to jetifying to parser.

  Args:
    parser: ArgumentParser object.
  """
  parser.add_argument(
      '--input-path',
      required=True,
      help='Path to input file(s). Either the classes '
      'directory, or the path to a jar.')
  parser.add_argument(
      '--output-path',
      required=True,
      help='Path to output final file(s) to. Either the '
      'final classes directory, or the directory in '
      'which to place the instrumented/copied jar.')
  parser.add_argument(
      '--jetify-path', required=True, help='Path to jetify bin.')
  parser.add_argument(
      '--jetify-config-path', required=True, help='Path to jetify config file.')


def _RunJetifyCommand(parser):
  args = parser.parse_args()
  cmd = [
      args.jetify_path,
      '-i',
      args.input_path,
      '-o',
      args.output_path,
      # Need to suppress a lot of warning output when jar doesn't have
      # any references rewritten.
      '-l',
      'error'
  ]
  if args.jetify_config_path:
    cmd.extend(['-c', args.jetify_config_path])
  # Must wait for jetify command to complete to prevent race condition.
  env = os.environ.copy()
  env['JAVA_HOME'] = build_utils.JAVA_HOME
  subprocess.check_call(cmd, env=env)


def main():
  parser = argparse.ArgumentParser()
  _AddArguments(parser)
  _RunJetifyCommand(parser)


if __name__ == '__main__':
  sys.exit(main())
