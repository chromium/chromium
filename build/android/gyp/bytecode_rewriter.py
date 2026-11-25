#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Wrapper script around bytecode rewriter tools."""

import argparse
import contextlib
import logging
import os
import sys
import tempfile

from util import build_utils
import action_helpers  # build_utils adds //build to sys.path.

_BYTECODE_REWRITER_DIR = os.path.join(
    build_utils.DIR_SOURCE_ROOT, 'build', 'android', 'bytecode', 'cipd'
)
_BYTECODE_REWRITER_BINARY = os.path.join(
    _BYTECODE_REWRITER_DIR, 'bytecode_rewriter'
)
_BYTECODE_REWRITER_JAR = os.path.join(
    _BYTECODE_REWRITER_DIR, 'bytecode_rewriter.jar'
)

# The real OS limit is generally >100 KiB (Linux MAX_ARG_STRLEN is 128 KiB),
# so 10 KiB is a conservative threshold to switch to an @args response file.
_MAX_CMDLINE = 10000


def RunRewriter(tool, tool_args):
    if build_utils.IS_LINUX_X64:
        cmd = [
            _BYTECODE_REWRITER_BINARY,
            f'-Djava.home={build_utils.JAVA_HOME}',
            tool,
        ]
    else:
        cmd = build_utils.JavaCmd() + ['-jar', _BYTECODE_REWRITER_JAR, tool]

    with contextlib.ExitStack() as stack:
        if sum(len(x) for x in tool_args) <= _MAX_CMDLINE:
            cmd.extend(tool_args)
        else:
            is_debug = logging.getLogger().isEnabledFor(logging.DEBUG)
            args_file = stack.enter_context(
                tempfile.NamedTemporaryFile(
                    mode='w', encoding='utf-8', delete=not is_debug
                )
            )
            args_file.write('\n'.join(tool_args))
            args_file.flush()
            cmd.append('@' + args_file.name)

        build_utils.CheckOutput(cmd, print_stdout=True)


def main(argv):
    argv = build_utils.ExpandFileArgs(argv[1:])
    parser = argparse.ArgumentParser()
    action_helpers.add_depfile_arg(parser)
    parser.add_argument(
        '--tool',
        required=True,
        help='Bytecode rewriter subcommand name.',
    )
    parser.add_argument('--classpath', action='append', nargs='+')
    parser.add_argument('--input-jar', required=True)
    parser.add_argument('--output-jar', required=True)
    parser.add_argument(
        'args', nargs='*', help='Arguments to pass to the rewriter tool.'
    )
    args = parser.parse_args(argv)

    classpath = action_helpers.parse_gn_list(args.classpath)
    action_helpers.write_depfile(
        args.depfile, args.output_jar, inputs=classpath
    )

    classpath.append(args.input_jar)
    tool_args = [
        '--classpath',
        ':'.join(classpath),
        args.input_jar,
        args.output_jar,
    ] + args.args
    RunRewriter(args.tool, tool_args)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
