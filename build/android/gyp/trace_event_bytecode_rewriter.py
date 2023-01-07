#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Wrapper script around TraceEventAdder script."""

import argparse
import sys
import os

from util import build_utils


def main(argv):
  argv = build_utils.ExpandFileArgs(argv[1:])
  parser = argparse.ArgumentParser()
  build_utils.AddDepfileOption(parser)
  parser.add_argument('--script',
                      required=True,
                      help='Path to the java binary wrapper script.')
  parser.add_argument('--stamp', help='Path to stamp to mark when finished.')
  parser.add_argument('--classpath', action='append', nargs='+')
  parser.add_argument('--input-jars', action='append', nargs='+')
  parser.add_argument('--output-jars', action='append', nargs='+')
  args = parser.parse_args(argv)

  args.classpath = build_utils.ParseGnList(args.classpath)
  args.input_jars = build_utils.ParseGnList(args.input_jars)
  args.output_jars = build_utils.ParseGnList(args.output_jars)

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

  build_utils.WriteDepfile(args.depfile, args.stamp, inputs=all_input_jars)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
