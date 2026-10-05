#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Stub invoked when an Apple-only iOS build tool runs on a non-Mac host."""

import argparse
import sys


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--tool', default='<unknown>')
    args, _ = parser.parse_known_args(argv)
    sys.stderr.write(
        f'error: {args.tool} requires macOS; set apple_tool_wrapper in '
        'args.gn (see //build/toolchain/apple/apple_tool_wrapper.gni)\n'
    )
    return 1


if __name__ == '__main__':
    sys.exit(main())
