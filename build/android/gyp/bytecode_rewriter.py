#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Wrapper script around ByteCodeRewriter subclass scripts."""

import argparse
import sys

from util import build_utils
import action_helpers  # build_utils adds //build to sys.path.


def main(argv):
  argv = build_utils.ExpandFileArgs(argv[1:])
  parser = argparse.ArgumentParser()
  action_helpers.add_depfile_arg(parser)
  parser.add_argument('--script',
                      required=True,
                      help='Path to the java binary wrapper script.')
  parser.add_argument('--classpath', action='append', nargs='+')
  parser.add_argument('--input-jar', required=True)
  parser.add_argument('--output-jar', required=True)
  args = parser.parse_args(argv)

  classpath = action_helpers.parse_gn_list(args.classpath)
  action_helpers.write_depfile(args.depfile, args.output_jar, inputs=classpath)

  classpath.append(args.input_jar)
  cmd = [
      args.script, '--classpath', ':'.join(classpath), args.input_jar,
      args.output_jar
  ]
  build_utils.CheckOutput(cmd, print_stdout=True)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
