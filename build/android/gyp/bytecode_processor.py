#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wraps bin/helper/java_bytecode_rewriter and expands @FileArgs."""

import argparse
import os
import subprocess
import sys

from util import build_utils


def _AddSwitch(parser, val):
  parser.add_argument(
      val, action='store_const', default='--disabled', const=val)


def main(argv):
  argv = build_utils.ExpandFileArgs(argv[1:])
  parser = argparse.ArgumentParser()
  parser.add_argument('--script', required=True,
                      help='Path to the java binary wrapper script.')
  parser.add_argument('--input-jar', required=True)
  parser.add_argument('--output-jar', required=True)
  parser.add_argument('--direct-classpath-jars', required=True)
  parser.add_argument('--sdk-classpath-jars', required=True)
  parser.add_argument('--extra-classpath-jars', dest='extra_jars',
                      action='append', default=[],
                      help='Extra inputs, passed last to the binary script.')
  parser.add_argument('-v', '--verbose', action='store_true')
  _AddSwitch(parser, '--is-prebuilt')
  _AddSwitch(parser, '--enable-custom-resources')
  _AddSwitch(parser, '--enable-assert')
  _AddSwitch(parser, '--enable-thread-annotations')
  _AddSwitch(parser, '--enable-check-class-path')
  args = parser.parse_args(argv)

  sdk_jars = build_utils.ParseGnList(args.sdk_classpath_jars)
  assert len(sdk_jars) > 0

  direct_jars = build_utils.ParseGnList(args.direct_classpath_jars)
  assert len(direct_jars) > 0

  extra_classpath_jars = []
  for a in args.extra_jars:
    extra_classpath_jars.extend(build_utils.ParseGnList(a))

  if args.verbose:
    verbose = '--verbose'
  else:
    verbose = '--not-verbose'

  cmd = ([
      args.script, args.input_jar, args.output_jar, verbose, args.is_prebuilt,
      args.enable_assert, args.enable_custom_resources,
      args.enable_thread_annotations, args.enable_check_class_path,
      str(len(sdk_jars))
  ] + sdk_jars + [str(len(direct_jars))] + direct_jars + extra_classpath_jars)
  subprocess.check_call(cmd)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
