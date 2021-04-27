#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Wrapper script around ByteCodeRewriter subclass scripts."""

import argparse
import sys

from util import build_utils


def main(argv):
  argv = build_utils.ExpandFileArgs(argv[1:])
  parser = argparse.ArgumentParser()
  build_utils.AddDepfileOption(parser)
  parser.add_argument('--script',
                      required=True,
                      help='Path to the java binary wrapper script.')
  parser.add_argument('--classpath', action='append', nargs='+')
  parser.add_argument('--input-jar', required=True)
  parser.add_argument('--output-jar', required=True)
  args = parser.parse_args(argv)

  classpath = build_utils.ParseGnList(args.classpath)
  build_utils.WriteDepfile(args.depfile, args.output_jar, inputs=classpath)

  classpath.append(args.input_jar)
  cmd = [
      args.script, '--classpath', ':'.join(classpath), args.input_jar,
      args.output_jar
  ]
  build_utils.CheckOutput(cmd, print_stdout=True)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
