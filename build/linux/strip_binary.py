#!/usr/bin/env python3
#
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import subprocess
import sys


def main() -> int:
  parser = argparse.ArgumentParser(description="Strip binary using LLVM tools.")
  parser.add_argument("--llvm-strip-binary-path",
                      help="Path to llvm-strip executable.")
  parser.add_argument("--llvm-objcopy-binary-path",
                      required=True,
                      help="Path to llvm-objcopy executable.")
  parser.add_argument("--binary-input", help="Input ELF binary.")
  parser.add_argument("--symbol-output",
                      help="File to write extracted debug info (.debug).")
  parser.add_argument("--stripped-binary-output",
                      help="File to write stripped binary.")
  args = parser.parse_args()

  # Replicate the behavior of:
  # eu-strip <binary_input> -o <stripped_binary_output> -f <symbol_output>
  subprocess.check_output([
      args.llvm_objcopy_binary_path,
      "--only-keep-debug",
      args.binary_input,
      args.symbol_output,
  ])
  subprocess.check_output([
      args.llvm_strip_binary_path,
      "--strip-debug",
      "--strip-unneeded",
      "-o",
      args.stripped_binary_output,
      args.binary_input,
  ])
  subprocess.check_output([
      args.llvm_objcopy_binary_path,
      f"--add-gnu-debuglink={args.symbol_output}",
      args.stripped_binary_output,
  ])

  return 0


if __name__ == "__main__":
  sys.exit(main())
