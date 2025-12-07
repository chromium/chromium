#!/usr/bin/env python3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Invokes `rustc --print=cfg --target=...` and writes results to a file.

This script is typically invoked using the
`//build/rust/gni_impl:rustc_print_cfg` target.
"""

import argparse
import io
import os
import platform
import re
import subprocess
import sys
import tempfile


def rustc_name():
  if platform.system() == 'Windows':
    return "rustc.exe"
  else:
    return "rustc"


def capture_rustc_cfg(rust_prefix, target, output_path):
  """ Invokes `rustc --print=cfg --target=<target>` and saves results.

  Results are saved to a file at `output_path`. """

  rustc_path = os.path.join(rust_prefix, rustc_name())

  # TODO(lukasza): Check if command-line flags other `--target` may affect the
  # output of `--print-cfg`.  If so, then consider also passing extra `args`
  # (derived from `rustflags` maybe?).
  args = [rustc_path, "--print=cfg", f"--target={target}"]

  os.makedirs(os.path.dirname(output_path), exist_ok=True)
  with open(output_path, 'w') as output_file:
    subprocess.run(args, stdout=output_file, check=True)


def main():
  parser = argparse.ArgumentParser("rustc_print_cfg.py")
  parser.add_argument('--rust-prefix', required=True, help='rust path prefix')
  parser.add_argument('--target', required=True, help='rust target triple')
  parser.add_argument('--output-path', required=True, help='output file')
  args = parser.parse_args()
  capture_rustc_cfg(args.rust_prefix, args.target, args.output_path)
  return 0


if __name__ == '__main__':
  sys.exit(main())
