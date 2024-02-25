#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Wrapper script around TraceEventAdder script."""

import argparse
import sys
import tempfile
import os

from util import build_utils
import action_helpers  # build_utils adds //build to sys.path.


# The real limit is generally >100kb, but 10k seems like a reasonable "it's big"
# threshold.
_MAX_CMDLINE = 10000


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

  cmd = [
      args.script, '--classpath', ':'.join(args.classpath),
      ':'.join(args.input_jars), ':'.join(args.output_jars)
  ]
  if sum(len(x) for x in cmd) > _MAX_CMDLINE:
    # Cannot put --classpath in the args file because that is consumed by the
    # wrapper script.
    args_file = tempfile.NamedTemporaryFile(mode='w')
    args_file.write('\n'.join(cmd[3:]))
    args_file.flush()
    cmd[3:] = ['@' + args_file.name]

  build_utils.CheckOutput(cmd, print_stdout=True)

  build_utils.Touch(args.stamp)

  all_input_jars = args.input_jars + args.classpath
  action_helpers.write_depfile(args.depfile, args.stamp, inputs=all_input_jars)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
