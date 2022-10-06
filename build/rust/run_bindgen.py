#!/usr/bin/env python3

# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import subprocess
import sys

# Set up path to be able to import build_utils.
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir,
                 os.pardir, 'build', 'android', 'gyp'))
from util import build_utils

from filter_clang_args import filter_clang_args


def atomic_copy(in_path, out_path):
  with open(in_path, 'rb') as input:
    with build_utils.AtomicOutput(out_path, only_if_changed=True) as output:
      content = input.read()
      output.write(content)


def copy_to_prefixed_filename(path, filename, prefix):
  atomic_copy(os.path.join(path, filename),
              os.path.join(path, prefix + "_" + filename))


def main():
  parser = argparse.ArgumentParser("run_bindgen.py")
  parser.add_argument("--exe", help="Path to bindgen", required=True),
  parser.add_argument("--header",
                      help="C header file to generate bindings for",
                      required=True)
  parser.add_argument("--depfile",
                      help="depfile to output with header dependencies")
  parser.add_argument("--output", help="output .rs bindings", required=True)
  parser.add_argument("--ld-library-path", help="LD_LIBRARY_PATH to set")
  parser.add_argument("-I", "--include", help="include path", action="append")
  parser.add_argument(
      "clangargs",
      metavar="CLANGARGS",
      help="arguments to pass to libclang (see "
      "https://docs.rs/bindgen/latest/bindgen/struct.Builder.html#method.clang_args)",
      nargs="*")
  args = parser.parse_args()
  genargs = []

  # Bindgen settings we use for Chromium
  genargs.append('--no-layout-tests')
  genargs.append('--size_t-is-usize')
  genargs += ['--rust-target', 'nightly']

  if args.depfile:
    genargs.append('--depfile')
    genargs.append(args.depfile)
  genargs.append('--output')
  genargs.append(args.output)
  genargs.append(args.header)
  genargs.append('--')
  genargs.extend(filter_clang_args(args.clangargs))
  env = os.environ
  if args.ld_library_path:
    env["LD_LIBRARY_PATH"] = args.ld_library_path
  returncode = subprocess.run([args.exe, *genargs], env=env).returncode
  if returncode != 0:
    # Make sure we don't emit anything if bindgen failed.
    try:
      os.remove(args.output)
    except FileNotFoundError:
      pass
    try:
      os.remove(args.depfile)
    except FileNotFoundError:
      pass
  return returncode


if __name__ == '__main__':
  sys.exit(main())
