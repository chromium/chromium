#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Wrapper script around TraceEventAdder."""

import argparse
import os
import sys

import bytecode_rewriter
from util import build_utils
import action_helpers  # build_utils adds //build to sys.path.


def main(argv):
    build_utils.InitLogging('TRACE_EVENT_REWRITER_DEBUG')
    argv = build_utils.ExpandFileArgs(argv[1:])
    parser = argparse.ArgumentParser()
    action_helpers.add_depfile_arg(parser)
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

    tool_args = [
        '--classpath',
        ':'.join(args.classpath),
        ':'.join(args.input_jars),
        ':'.join(args.output_jars),
    ]
    bytecode_rewriter.RunRewriter('trace_event_adder', tool_args)

    build_utils.Touch(args.stamp)

    all_input_jars = args.input_jars + args.classpath
    action_helpers.write_depfile(
        args.depfile, args.stamp, inputs=all_input_jars
    )


if __name__ == '__main__':
    sys.exit(main(sys.argv))
