#!/usr/bin/env vpython3

# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is a wrapper script which runs a Cargo build.rs build script
# executable in a Cargo-like environment. Build scripts can do arbitrary
# things and we can't support everything. Moreover, we do not WANT
# to support everything because that means the build is not deterministic.
# Code review processes must be applied to ensure that the build script
# depends upon only these inputs:
#
# * The environment variables set by Cargo here:
#   https://doc.rust-lang.org/cargo/reference/environment-variables.html#environment-variables-cargo-sets-for-build-scripts
# * Output from rustc commands, e.g. to figure out the Rust version.
#
# Similarly, the only allowable output from such a build script
# is currently:
#
# * Generated .rs files
# * cargo:rustc-cfg output.
#
# That's it. We don't even support the other standard cargo:rustc-
# output messages.

import os
import sys

# Set up path to be able to import build_utils
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir,
                 os.pardir, 'build', 'android', 'gyp'))
from util import build_utils

import argparse
import io
import subprocess
import re
import platform

RUSTC_VERSION_LINE = re.compile(r"(\w+): (.*)")


def rustc_name():
  if platform.system() == 'Windows':
    return "rustc.exe"
  else:
    return "rustc"


def host_triple(rustc_path):
  """ Works out the host rustc target. """
  args = [rustc_path, "-vV"]
  known_vars = dict()
  proc = subprocess.Popen(args, stdout=subprocess.PIPE)
  for line in io.TextIOWrapper(proc.stdout, encoding="utf-8"):
    m = RUSTC_VERSION_LINE.match(line.rstrip())
    if m:
      known_vars[m.group(1)] = m.group(2)
  return known_vars["host"]


RUSTC_CFG_LINE = re.compile("cargo:rustc-cfg=(.*)")

parser = argparse.ArgumentParser(description='Run Rust build script.')
parser.add_argument('--build-script', required=True, help='build script to run')
parser.add_argument('--output',
                    required=True,
                    help='where to write output rustc flags')
parser.add_argument('--target', help='rust target triple')
parser.add_argument('--features', help='features', nargs='+')
parser.add_argument('--rust-prefix', required=True, help='rust path prefix')
parser.add_argument('--out-dir', required=True, help='target out dir')
parser.add_argument('--src-dir', required=True, help='target source dir')

args = parser.parse_args()

rustc_path = os.path.join(args.rust_prefix, rustc_name())
env = os.environ.copy()
env.clear()  # try to avoid build scripts depending on other things
env["RUSTC"] = os.path.abspath(rustc_path)
env["OUT_DIR"] = os.path.abspath(args.out_dir)
env["CARGO_MANIFEST_DIR"] = os.path.abspath(args.src_dir)
env["HOST"] = host_triple(rustc_path)
if args.target is None:
  env["TARGET"] = env["HOST"]
else:
  env["TARGET"] = args.target
if args.features:
  for f in args.features:
    feature_name = f.upper().replace("-", "_")
    env["CARGO_FEATURE_%s" % feature_name] = "1"

# In the future we should, set all the variables listed here:
# https://doc.rust-lang.org/cargo/reference/environment-variables.html#environment-variables-cargo-sets-for-build-scripts

# In the future, we could consider isolating this build script
# into a chroot jail or similar on some platforms, but ultimately
# we are always going to be reliant on code review to ensure the
# build script is deterministic and trustworthy, so this would
# really just be a backup to humans.
proc = subprocess.run([os.path.abspath(args.build_script)],
                      env=env,
                      cwd=args.src_dir,
                      text=True,
                      capture_output=True)

if proc.stderr.rstrip():
  print(proc.stderr.rstrip(), file=sys.stderr)
proc.check_returncode()

flags = ""
for line in proc.stdout.split("\n"):
  m = RUSTC_CFG_LINE.match(line.rstrip())
  if m:
    flags = "%s--cfg\n%s\n" % (flags, m.group(1))

# AtomicOutput will ensure we only write to the file on disk if what we give to
# write() is different than what's currently on disk.
with build_utils.AtomicOutput(args.output) as output:
  output.write(flags.encode("utf-8"))
