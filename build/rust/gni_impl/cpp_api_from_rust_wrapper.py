#!/usr/bin/env python3

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import subprocess
import pathlib
import sys

from rustc_wrapper import (
    PrepareRustEnvForExecution,
    LoadRustEnvAndFlags,
    HandleReturnCode,
    ExpandNestedRustStyleRspFiles,
)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--cpp-api-from-rust-exe-path', required=True, type=pathlib.Path
    )
    # `--rustc-env-and-flags` is not used when generating bindings for a
    # prebuilt stdlib in the sysroot.
    parser.add_argument('--rustc-env-and-flags', type=pathlib.Path)
    parser.add_argument('args', metavar='ARG', nargs='+')
    args = parser.parse_args()

    rustenv = None
    rustflags = []
    if args.rustc_env_and_flags:
        (rustenv, rustflags) = LoadRustEnvAndFlags(args.rustc_env_and_flags)
        PrepareRustEnvForExecution(rustenv)

    expanded_rustflags = ExpandNestedRustStyleRspFiles(rustflags)
    flag_parser = argparse.ArgumentParser(add_help=False, allow_abbrev=False)
    flag_parser.add_argument('-L', action='append', default=[])
    flag_parser.add_argument('--extern', action='append', default=[])
    known, _ = flag_parser.parse_known_args(expanded_rustflags)
    crubit_extra_args = [f'-L{x}' for x in known.L]
    # Crubit requires `CRATE=PATH`. Skip sysroot crates like
    # `--extern proc_macro`.
    crubit_extra_args += [f'--extern={x}' for x in known.extern if '=' in x]

    crubit_args = [*args.args, *crubit_extra_args]

    r = subprocess.run(
        [args.cpp_api_from_rust_exe_path, *crubit_args],
        env=rustenv,
        check=False,
    )
    HandleReturnCode(r)
    return 0


if __name__ == '__main__':
    sys.exit(main())
