#!/usr/bin/env python3

# Copyright 2022 The Chromium Authors. All rights reserved.
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


def atomic_copy(in_path, out_path):
  with open(in_path, 'rb') as input:
    with build_utils.AtomicOutput(out_path, only_if_changed=True) as output:
      content = input.read()
      output.write(content)


def copy_to_prefixed_filename(path, filename, prefix):
  atomic_copy(os.path.join(path, filename),
              os.path.join(path, prefix + "_" + filename))


def main():
  parser = argparse.ArgumentParser("run_autocxx_gen.py")
  parser.add_argument("--exe", help="Path to autocxx_gen", required=True),
  parser.add_argument("--source",
                      help="rust file containing cxx or autocxx macros",
                      required=True)
  parser.add_argument("--outdir", help="output dir", required=True)
  parser.add_argument("--header", help="output header filename", required=True)
  parser.add_argument("--ld-library-path", help="LD_LIBRARY_PATH to set")
  parser.add_argument(
      "--output-prefix",
      help="prefix for output filenames (cc only, not h/rs). The purpose is "
      "to ensure each .o file has a unique name, as is required by ninja.")
  parser.add_argument("--cxx-impl-annotations",
                      help="annotation for exported symbols")
  parser.add_argument("--cxx-h-path", help="path to cxx.h")
  parser.add_argument("-I", "--include", help="include path", action="append")
  parser.add_argument(
      "clangargs",
      metavar="CLANGARGS",
      help="arguments to pass to libclang (see "
      "https://docs.rs/bindgen/latest/bindgen/struct.Builder.html#method.clang_args)",
      nargs="*")
  args = parser.parse_args()
  genargs = []
  genargs.append(args.source)
  genargs.append('--outdir')
  genargs.append(args.outdir)
  genargs.append('--gen-rs-include')
  genargs.append('--gen-cpp')
  genargs.append('--generate-exact')
  genargs.append('2')
  genargs.append('--fix-rs-include-name')
  if args.cxx_impl_annotations:
    genargs.append('--cxx-impl-annotations')
    genargs.append(args.cxx_impl_annotations)
  if args.cxx_h_path:
    genargs.append('--cxx-h-path')
    genargs.append(args.cxx_h_path)
  genargs.append('--')
  genargs.extend(args.clangargs)
  env = os.environ
  if args.ld_library_path:
    env["LD_LIBRARY_PATH"] = args.ld_library_path
  subprocess.run([args.exe, *genargs], check=True, env=env)
  # TODO(crbug.com/1306841): Remove the need for these intermediate files.
  if args.output_prefix:
    copy_to_prefixed_filename(args.outdir, "gen0.cc", args.output_prefix)
    copy_to_prefixed_filename(args.outdir, "gen1.cc", args.output_prefix)
  atomic_copy(os.path.join(args.outdir, "cxxgen.h"), args.header)


if __name__ == '__main__':
  sys.exit(main())
