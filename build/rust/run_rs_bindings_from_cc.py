#!/usr/bin/env python3

# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import subprocess
import sys

# Set up path to be able to import build_utils.
THIS_DIR = os.path.dirname(os.path.abspath(__file__))
CHROMIUM_SRC_DIR = os.path.relpath(os.path.join(THIS_DIR, os.pardir, os.pardir))
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

  # Output paths
  generator_args = []
  generator_args.append("--rs_out={0}".format(os.path.relpath(args.rs_out)))
  generator_args.append("--cc_out={0}".format(os.path.relpath(args.cc_out)))
  if "CRUBIT_DEBUG" in os.environ:
    generator_args.append("--ir_out={0}".format(
        os.path.relpath(args.rs_out).replace(".rs", ".ir")))

  # Public headers.
  generator_args.append("--public_headers={0}".format(",".join(
      [os.path.relpath(hdr) for hdr in args.public_headers.split(",")])))

  # Targets to headers map.
  with open(args.targets_and_headers_from_gn, "r") as f:
    targets_and_headers = json.load(f)
  for entry in targets_and_headers:
    hdrs = entry["h"]
    for i in range(len(hdrs)):
      hdrs[i] = os.path.relpath(hdrs[i])
  generator_args.append("--targets_and_headers={0}".format(
      json.dumps(targets_and_headers)))

  # All Crubit invocations in Chromium share the following cmdline args.
  generator_args.append(f"--rustfmt_exe_path={RUSTFMT_EXE_PATH}")
  generator_args.append(f"--rustfmt_config_path={RUSTFMT_CONFIG_PATH}")
  generator_args.append(
      "--crubit_support_path=third_party/crubit/src/rs_bindings_from_cc/support"
  )

  # Long cmdlines may not work - work around that by using Abseil's `--flagfile`
  # https://abseil.io/docs/python/guides/flags#a-note-about---flagfile
  #
  # Note that `clang_args` are not written to the flag file, because Abseil's
  # flag parsing code is only aware of `ABSL_FLAG`-declared flags and doesn't
  # know about Clang args (e.g. `-W...` or `-I...`).
  params_file_path = os.path.relpath(args.rs_out).replace(".rs", ".params")
  with open(params_file_path, "w") as f:
    for line in generator_args:
      print(line, file=f)

  # Clang arguments.
  #
  # The call to `filter_clang_args` is needed to avoid the following error:
  # error: unable to find plugin 'find-bad-constructs'
  clang_args = []
  clang_args.extend(filter_clang_args(args.clang_args))
  # TODO(crbug.com/1329611): This warning needs to be suppressed, because
  # otherwise Crubit/Clang complains as follows:
  #     error: .../third_party/rust-toolchain/bin/rs_bindings_from_cc:
  #     'linker' input unused [-Werror,-Wunused-command-line-argument]
  # Maybe `build/rust/rs_bindings_from_cc.gni` gives too much in `args`?  But
  # then `{{cflags}}` seems perfectly reasonable...
  clang_args += ["-Wno-unused-command-line-argument"]

  # Print a copy&pastable final cmdline when asked for debugging help.
  cmdline = [RS_BINDINGS_FROM_CC_EXE_PATH, f"--flagfile={params_file_path}"]
  cmdline.extend(clang_args)
  if "CRUBIT_DEBUG" in os.environ:
    pretty_cmdline = format_cmdline(cmdline)
    print(f"CRUBIT_DEBUG: CMDLINE: {pretty_cmdline}", file=sys.stderr)

  # TODO(crbug.com/1329611): run_bindgen.py removes the outputs when the tool
  # fails.  Maybe we need to do something similar here?  OTOH in most failure
  # modes Crubit will fail *before* generating its outputs...
  return subprocess.run(cmdline).returncode


if __name__ == '__main__':
  sys.exit(main())
