#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Wrapper script around TraceEventAdder script."""

import argparse
import sys
import os

from util import build_utils
import action_helpers  # build_utils adds //build to sys.path.


def main(argv):
  argv = build_utils.ExpandFileArgs(argv[1:])
  parser = argparse.ArgumentParser()
  action_helpers.add_depfile_arg(parser)
  parser.add_argument('--script',
                      required=True,
                      help='Path to the java binary wrapper script.')
  parser.add_argument('--stamp', help='Path to stamp to mark when finished.')
  parser.add_argument('--classpath', action='append', nargs='+')
  parser.add_argument('--input-jars', action='append', nargs='+')
  parser.add_argument('--output-jars', action='append', nargs='+')
  args = parser.parse_args(argv)

  args.classpath = action_helpers.parse_gn_list(args.classpath)
  args.input_jars = action_helpers.parse_gn_list(args.input_jars)
  args.output_jars = action_helpers.parse_gn_list(args.output_jars)

  for output_jar in args.output_jars:
    jar_dir = os.path.dirname(output_jar)
    if not os.path.exists(jar_dir):
      os.makedirs(jar_dir)

  all_input_jars = set(args.classpath + args.input_jars)
  cmd = [
      args.script, '--classpath', ':'.join(sorted(all_input_jars)),
      ':'.join(args.input_jars), ':'.join(args.output_jars)
  ]
  build_utils.CheckOutput(cmd, print_stdout=True)

  build_utils.Touch(args.stamp)

  action_helpers.write_depfile(args.depfile, args.stamp, inputs=all_input_jars)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
