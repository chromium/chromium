#!/usr/bin/env python3

# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import contextlib
import os
import subprocess
import sys

# Set up path to be able to import action_helpers.
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir,
                 os.pardir, 'build'))
import action_helpers

from filter_clang_args import filter_clang_args


def main():
  parser = argparse.ArgumentParser("run_bindgen.py")
  parser.add_argument("--exe", help="Path to bindgen", required=True),
  parser.add_argument("--header",
                      help="C header file to generate bindings for",
                      required=True)
  parser.add_argument("--depfile",
                      help="depfile to output with header dependencies")
  parser.add_argument("--output", help="output .rs bindings", required=True)
  parser.add_argument(
      "--wrap-static-fns",
      help="output source file for `static` and `static inline` functions")
  parser.add_argument("--ld-library-path",
                      help="LD_LIBRARY_PATH (or DYLD_LIBRARY_PATH on Mac) to "
                      "set")
  parser.add_argument("--libclang-path",
                      help="Path to the libclang shared libray.")
  parser.add_argument("-I", "--include", help="include path", action="append")
  parser.add_argument("--bindgen-flags",
                      help="flags to pass to bindgen",
                      nargs="*")
  parser.add_argument(
      "clangargs",
      metavar="CLANGARGS",
      help="arguments to pass to libclang (see "
      "https://docs.rs/bindgen/latest/bindgen/struct.Builder.html#method.clang_args)",
      nargs="*")
  args = parser.parse_args()

  # Abort if `TARGET` exists in the environment. Cargo sets `TARGET` when
  # running build scripts and bindgen will try to be helpful by using that value
  # if it's set. In practice we've seen a case where someone had the value set
  # in their build environment with no intention of it reaching bindgen, leading
  # to a hard-to-debug build error.
  if 'TARGET' in os.environ:
    sys.exit('ERROR: saw TARGET in environment, remove to avoid bindgen'
             ' failures')

  with contextlib.ExitStack() as stack:
    # Args passed to the actual bindgen cli
    genargs = []
    genargs.append('--no-layout-tests')
    if args.bindgen_flags is not None:
      for flag in args.bindgen_flags:
        genargs.append("--" + flag)

    # TODO(danakj): We need to point bindgen to
    # //third_party/rust-toolchain/bin/rustfmt.
    genargs.append('--no-rustfmt-bindings')
    genargs += ['--rust-target', 'nightly']

    if args.depfile:
      depfile = stack.enter_context(action_helpers.atomic_output(args.depfile))
      genargs.append('--depfile')
      genargs.append(depfile.name)
    # Ideally we would use action_helpers.atomic_output for the output file, but
    # this would put the wrong name in the depfile.
    genargs.append('--output')
    genargs.append(args.output)

    # The GN rules know what path to find the system headers in, and we want to
    # use the headers we specify, instead of non-hermetic headers from elsewhere
    # in the system.
    genargs.append('--no-include-path-detection')

    if args.wrap_static_fns:
      wrap_static_fns = stack.enter_context(
          action_helpers.atomic_output(args.wrap_static_fns))
      genargs.append('--experimental')
      genargs.append('--wrap-static-fns')
      genargs.append('--wrap-static-fns-path')
      genargs.append(wrap_static_fns.name)
    genargs.append(args.header)
    genargs.append('--')
    genargs.extend(filter_clang_args(args.clangargs))
    env = os.environ
    if args.ld_library_path:
      if sys.platform == 'darwin':
        env["DYLD_LIBRARY_PATH"] = args.ld_library_path
      else:
        env["LD_LIBRARY_PATH"] = args.ld_library_path
    if args.libclang_path:
      env["LIBCLANG_PATH"] = args.libclang_path
    try:
      subprocess.check_call([args.exe, *genargs], env=env)
    except:
      # Make sure we don't emit anything if bindgen failed. The other files use
      # action_helpers for this.
      try:
        os.remove(args.output)
      except FileNotFoundError:
        pass
      raise


if __name__ == '__main__':
  main()
