#!/usr/bin/env python3

# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import subprocess
import sys

# Set up path to be able to import build_utils.
THIS_DIR = os.path.dirname(os.path.abspath(__file__))
CHROMIUM_SRC_DIR = os.path.join(THIS_DIR, os.pardir, os.pardir)
sys.path.append(THIS_DIR)
sys.path.append(os.path.join(CHROMIUM_SRC_DIR, 'build', 'android', 'gyp'))
from run_bindgen import filter_clang_args
from util import build_utils

RUST_TOOLCHAIN_DIR = os.path.join(CHROMIUM_SRC_DIR, "third_party",
                                  "rust-toolchain")
RUSTFMT_EXE_PATH = os.path.join(RUST_TOOLCHAIN_DIR, "bin", "rustfmt")
RUSTFMT_CONFIG_PATH = os.path.join(CHROMIUM_SRC_DIR, ".rustfmt.toml")
RS_BINDINGS_FROM_CC_EXE_PATH = os.path.join(RUST_TOOLCHAIN_DIR, "bin",
                                            "rs_bindings_from_cc")

# TODO(crbug.com/1297592): The instructions below can be moved (once
# tools/rust/package_rust.py includes Crubit binaries) into a (new)
# doc with a title like: "Developing Crubit in Chromium".

# Instructions for manually building
# //third_party/rust-toolchain/bin/rs_bindings_from_cc:
#
# 1. Run:
#      $ tools/clang/scripts/build.py \
#           --bootstrap --without-android --without-fuchsia
#
#    (needed to generate third_party/llvm-bootstrap-install where
#     the next step will look for LLVM/Clang libs and headers)
#
# 2. Run:
#      $ tools/rust/build_crubit.py
#
# 3. Run:
#      $ cp \
# third_party/crubit/bazel-bin/rs_bindings_from_cc/rs_bindings_from_cc_impl \
# third_party/rust-toolchain/rs_bindings_from_cc
#
#    (note that the `_impl` suffix has been dropped from the binary name).


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument("--targets_and_headers_from_gn",
                      metavar="FILE",
                      help="File parsed into --targets_and_headers Crubit arg",
                      required=True),
  parser.add_argument("--public_headers",
                      metavar="FILE",
                      help="Passed through to Crubit",
                      required=True),
  parser.add_argument("--rs_out",
                      metavar="FILE",
                      help="Passed through to Crubit",
                      required=True),
  parser.add_argument("--cc_out",
                      metavar="FILE",
                      help="Passed through to Crubit",
                      required=True),
  parser.add_argument("clang_args",
                      metavar="CLANGARGS",
                      help="Arguments to forward to clang libraries",
                      nargs=argparse.REMAINDER)
  args = parser.parse_args()

  # Fixed config.
  genargs = []
  genargs.extend([
      "--crubit_support_path",
      "third_party/crubit/src/rs_bindings_from_cc/support"
  ])
  genargs.extend(["--rustfmt_exe_path", RUSTFMT_EXE_PATH])
  genargs.extend(["--rustfmt_config_path", RUSTFMT_CONFIG_PATH])

  # Morph contents of the GN-generated JSON file into a single-line string.
  with open(args.targets_and_headers_from_gn, "r") as f:
    targets_and_headers = f.read().replace('\n', ' ').replace('\r', ' ')
  genargs.extend(["--targets_and_headers", targets_and_headers])

  # Other inputs and outputs.
  genargs.extend(["--public_headers", args.public_headers])
  genargs.extend(["--rs_out", args.rs_out])
  genargs.extend(["--cc_out", args.cc_out])

  # The call to `filter_clang_args` is needed to avoid the following error:
  # error: unable to find plugin 'find-bad-constructs'
  genargs.extend(filter_clang_args(args.clang_args))

  # TODO(crbug.com/1297592): This warning needs to be suppressed, because
  # otherwise Crubit/Clang complains as follows:
  #     error: .../third_party/rust-toolchain/bin/rs_bindings_from_cc:
  #     'linker' input unused [-Werror,-Wunused-command-line-argument]
  # Maybe `build/rust/rs_bindings_from_cc.gni` gives too much in `args`?  But
  # then `{{cflags}}` seems perfectly reasonable...
  genargs += ["-Wno-unused-command-line-argument"]

  # TODO(crbug.com/1297592): run_bindgen.py removes the outputs when the tool
  # fails.  Maybe we need to do something similar here?  OTOH in most failure
  # modes Crubit will fail *before* generating its outputs...
  return subprocess.run([RS_BINDINGS_FROM_CC_EXE_PATH, *genargs]).returncode


if __name__ == '__main__':
  sys.exit(main())
