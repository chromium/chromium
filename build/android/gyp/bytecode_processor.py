#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wraps bin/helper/bytecode_processor and expands @FileArgs."""

import argparse
import sys

from util import build_utils
from util import server_utils


def _AddSwitch(parser, val):
  parser.add_argument(
      val, action='store_const', default='--disabled', const=val)


def main(argv):
  argv = build_utils.ExpandFileArgs(argv[1:])
  parser = argparse.ArgumentParser()
  parser.add_argument('--target-name', help='Fully qualified GN target name.')
  parser.add_argument('--script', required=True,
                      help='Path to the java binary wrapper script.')
  parser.add_argument('--gn-target', required=True)
  parser.add_argument('--input-jar', required=True)
  parser.add_argument('--direct-classpath-jars')
  parser.add_argument('--sdk-classpath-jars')
  parser.add_argument('--full-classpath-jars')
  parser.add_argument('--full-classpath-gn-targets')
  parser.add_argument('--stamp')
  parser.add_argument('-v', '--verbose', action='store_true')
  parser.add_argument('--missing-classes-allowlist')
  parser.add_argument('--warnings-as-errors',
                      action='store_true',
                      help='Treat all warnings as errors.')
  _AddSwitch(parser, '--is-prebuilt')
  args = parser.parse_args(argv)

  if server_utils.MaybeRunCommand(name=args.target_name,
                                  argv=sys.argv,
                                  stamp_file=args.stamp):
    return

  args.sdk_classpath_jars = build_utils.ParseGnList(args.sdk_classpath_jars)
  args.direct_classpath_jars = build_utils.ParseGnList(
      args.direct_classpath_jars)
  args.full_classpath_jars = build_utils.ParseGnList(args.full_classpath_jars)
  args.full_classpath_gn_targets = build_utils.ParseGnList(
      args.full_classpath_gn_targets)
  args.missing_classes_allowlist = build_utils.ParseGnList(
      args.missing_classes_allowlist)

  verbose = '--verbose' if args.verbose else '--not-verbose'

  cmd = [args.script, args.gn_target, args.input_jar, verbose, args.is_prebuilt]
  cmd += [str(len(args.missing_classes_allowlist))]
  cmd += args.missing_classes_allowlist
  cmd += [str(len(args.sdk_classpath_jars))]
  cmd += args.sdk_classpath_jars
  cmd += [str(len(args.direct_classpath_jars))]
  cmd += args.direct_classpath_jars
  cmd += [str(len(args.full_classpath_jars))]
  cmd += args.full_classpath_jars
  cmd += [str(len(args.full_classpath_gn_targets))]
  cmd += args.full_classpath_gn_targets
  build_utils.CheckOutput(cmd,
                          print_stdout=True,
                          fail_func=None,
                          fail_on_output=args.warnings_as_errors)

  if args.stamp:
    build_utils.Touch(args.stamp)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
