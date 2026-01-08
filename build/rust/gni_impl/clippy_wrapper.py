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
  parser.add_argument('--clippy-driver', required=True, type=pathlib.Path)
  parser.add_argument('--rustc-env-and-flags', type=pathlib.Path, required=True)
  parser.add_argument('--build-stamp-file', type=pathlib.Path, required=True)
  args = parser.parse_args()

  (rustenv, rustflags) = LoadRustEnvAndFlags(args.rustc_env_and_flags)
  ConvertPathsToAbsolute(rustenv)

  rustflags += [
      # Ask `clippy` to treat lint failures either as errors, or ignore
      # them altogether (i.e. avoid reporting failures as warnings which
      # are noisy but ignorable).  We cover lint categories from
      # https://doc.rust-lang.org/stable/clippy/lints.html that are either
      # `deny` or `warn` by default.
      #
      # If certain targets want to opt into additional lints, then they can
      # do so by using `#![deny(clippy::pedantic)]` or a similar attribute.
      "-Dclippy::correctness",
      "-Dclippy::suspicious",
      "-Dclippy::complexity",
      "-Dclippy::perf",
      "-Dclippy::style",
  ]

  # `clippy-driver` should not write any files into the build directory
  # (e.g. into `out/`).
  #
  # `--emit=metadata` asks Clippy to only emit `.rmeta`.  This:
  # * Matches how `cargo clippy` invokes `clippy-driver` - see
  #   https://crrev.com/c/7316228/17#message-fa234861e30ee37399388ca5a6a048822b658833
  # * Seems sufficient for triggering Clippy lints.
  # * Avoids performance overhead (and general ickiness) of also building
  #   `.rlib` from within `clippy-driver`.
  assert not [x for x in rustflags if x.startswith("--emit")]
  assert not [x for x in rustflags if x.startswith("-o")]
  assert not [x for x in rustflags if x.startswith("--out-dir")]
  temp_dir = tempfile.TemporaryDirectory()
  rustflags += ["--out-dir", temp_dir.name]
  rustflags += ["--emit=metadata"]

  r = subprocess.run([args.clippy_driver, *rustflags], env=rustenv, check=False)
  if r.returncode == 0:
    args.build_stamp_file.touch()
  return r.returncode


if __name__ == '__main__':
  sys.exit(main())
