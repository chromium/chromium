#!/usr/bin/env python3

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import subprocess
import pathlib
import sys
import tempfile

from rustc_wrapper import (ConvertPathsToAbsolute, LoadRustEnvAndFlags)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--cpp-api-from-rust-exe-path',
                      required=True,
                      type=pathlib.Path)
  parser.add_argument('--rustc-env-and-flags', type=pathlib.Path, required=True)
  parser.add_argument('args', metavar='ARG', nargs='+')
  args = parser.parse_args()

  (rustenv, rustflags) = LoadRustEnvAndFlags(args.rustc_env_and_flags)
  ConvertPathsToAbsolute(rustenv)

  rustflags = [*args.args, "--", *rustflags]

  # `cpp_api_from_rust` should not write any files into the build directory
  # (e.g. into `out/`).
  assert not [x for x in rustflags if x.startswith("--emit")]
  assert not [x for x in rustflags if x.startswith("-o")]
  assert not [x for x in rustflags if x.startswith("--out-dir")]
  temp_dir = tempfile.TemporaryDirectory()
  rustflags += ["--out-dir", temp_dir.name]

  r = subprocess.run([args.cpp_api_from_rust_exe_path, *rustflags],
                     env=rustenv,
                     check=False)
  return r.returncode


if __name__ == '__main__':
  sys.exit(main())
