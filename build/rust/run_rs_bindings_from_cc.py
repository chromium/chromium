#!/usr/bin/env python3

# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
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


def format_cmdline(args):
  def quote_arg(x):
    if ' ' not in x: return x
    x = x.replace('"', '\\"')
    return f"\"{x}\""

  return " ".join([quote_arg(x) for x in args])


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

  # Start building the cmdline args to pass to the `rs_bindings_from_cc` tool.
  genargs = [RS_BINDINGS_FROM_CC_EXE_PATH]

  # Output paths
  genargs.extend(["--rs_out", args.rs_out])
  genargs.extend(["--cc_out", args.cc_out])
  if "CRUBIT_DEBUG" in os.environ:
    genargs.extend(["--ir_out", args.rs_out.replace(".rs", ".ir")])

  # Public headers.
  genargs.extend([
      "--public_headers",
      ",".join([os.path.relpath(hdr) for hdr in args.public_headers.split(",")])
  ])

  # Targets to headers map.
  with open(args.targets_and_headers_from_gn, "r") as f:
    targets_and_headers = json.load(f)
  for entry in targets_and_headers:
    hdrs = entry["h"]
    for i in range(len(hdrs)):
      hdrs[i] = os.path.relpath(hdrs[i])
  genargs.extend(["--targets_and_headers", json.dumps(targets_and_headers)])

  # All Crubit invocations in Chromium share the following cmdline args.
  genargs.extend(["--rustfmt_exe_path", RUSTFMT_EXE_PATH])
  genargs.extend(["--rustfmt_config_path", RUSTFMT_CONFIG_PATH])
  genargs.extend([
      "--crubit_support_path",
      "third_party/crubit/src/rs_bindings_from_cc/support"
  ])

  # Clang arguments.
  #
  # The call to `filter_clang_args` is needed to avoid the following error:
  # error: unable to find plugin 'find-bad-constructs'
  genargs.extend(filter_clang_args(args.clang_args))
  # TODO(crbug.com/1329611): This warning needs to be suppressed, because
  # otherwise Crubit/Clang complains as follows:
  #     error: .../third_party/rust-toolchain/bin/rs_bindings_from_cc:
  #     'linker' input unused [-Werror,-Wunused-command-line-argument]
  # Maybe `build/rust/rs_bindings_from_cc.gni` gives too much in `args`?  But
  # then `{{cflags}}` seems perfectly reasonable...
  genargs += ["-Wno-unused-command-line-argument"]

  # Print a copy&pastable final cmdline when asked for debugging help.
  if "CRUBIT_DEBUG" in os.environ:
    pretty_cmdline = format_cmdline(genargs)
    print(f"CRUBIT_DEBUG: CMDLINE: {pretty_cmdline}", file=sys.stderr)

  # TODO(crbug.com/1329611): run_bindgen.py removes the outputs when the tool
  # fails.  Maybe we need to do something similar here?  OTOH in most failure
  # modes Crubit will fail *before* generating its outputs...
  return subprocess.run(genargs).returncode


if __name__ == '__main__':
  sys.exit(main())
