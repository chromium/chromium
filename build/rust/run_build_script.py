#!/usr/bin/env vpython3

# Copyright 2021 The Chromium Authors
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

import argparse
import io
import os
import platform
import re
import subprocess
import sys
import tempfile

# Set up path to be able to import action_helpers
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir,
                 os.pardir, 'build'))
import action_helpers


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


# Before 1.77, the format was `cargo:rustc-cfg=`. As of 1.77 the format is now
# `cargo::rustc-cfg=`.
RUSTC_CFG_LINE = re.compile("cargo::?rustc-cfg=(.*)")


def main():
  parser = argparse.ArgumentParser(description='Run Rust build script.')
  parser.add_argument('--build-script',
                      required=True,
                      help='build script to run')
  parser.add_argument('--output',
                      required=True,
                      help='where to write output rustc flags')
  parser.add_argument('--target', help='rust target triple')
  parser.add_argument('--pointer-width', help='rust target pointer width')
  parser.add_argument('--features', help='features', nargs='+')
  parser.add_argument('--env', help='environment variable', nargs='+')
  parser.add_argument('--rust-prefix', required=True, help='rust path prefix')
  parser.add_argument('--generated-files', nargs='+', help='any generated file')
  parser.add_argument('--out-dir', required=True, help='target out dir')
  parser.add_argument('--src-dir', required=True, help='target source dir')

  args = parser.parse_args()

  rustc_path = os.path.join(args.rust_prefix, rustc_name())

  # We give the build script an OUT_DIR of a temporary directory,
  # and copy out only any files which gn directives say that it
  # should generate. Mostly this is to ensure we can atomically
  # create those files, but it also serves to avoid side-effects
  # from the build script.
  # In the future, we could consider isolating this build script
  # into a chroot jail or similar on some platforms, but ultimately
  # we are always going to be reliant on code review to ensure the
  # build script is deterministic and trustworthy, so this would
  # really just be a backup to humans.
  with tempfile.TemporaryDirectory() as tempdir:
    env = {}  # try to avoid build scripts depending on other things
    env["RUSTC"] = os.path.abspath(rustc_path)
    env["OUT_DIR"] = tempdir
    env["CARGO_MANIFEST_DIR"] = os.path.abspath(args.src_dir)
    env["HOST"] = host_triple(rustc_path)
    env["CARGO_CFG_TARGET_POINTER_WIDTH"] = args.pointer_width
    if args.target is None:
      env["TARGET"] = env["HOST"]
    else:
      env["TARGET"] = args.target
    target_components = env["TARGET"].split("-")
    if len(target_components) == 2:
      env["CARGO_CFG_TARGET_ARCH"] = target_components[0]
      env["CARGO_CFG_TARGET_VENDOR"] = ''
      env["CARGO_CFG_TARGET_OS"] = target_components[1]
      env["CARGO_CFG_TARGET_ENV"] = ''
    elif len(target_components) == 3:
      env["CARGO_CFG_TARGET_ARCH"] = target_components[0]
      env["CARGO_CFG_TARGET_VENDOR"] = target_components[1]
      env["CARGO_CFG_TARGET_OS"] = target_components[2]
      env["CARGO_CFG_TARGET_ENV"] = ''
    elif len(target_components) == 4:
      env["CARGO_CFG_TARGET_ARCH"] = target_components[0]
      env["CARGO_CFG_TARGET_VENDOR"] = target_components[1]
      env["CARGO_CFG_TARGET_OS"] = target_components[2]
      env["CARGO_CFG_TARGET_ENV"] = target_components[3]
    else:
      print(f'Invalid TARGET {env["TARGET"]}')
      sys.exit(1)
    # See https://crbug.com/325543500 for background.
    # Cargo sets CARGO_CFG_TARGET_OS to "android" even when targeting
    # *-androideabi.
    if env["CARGO_CFG_TARGET_OS"].startswith("android"):
      env["CARGO_CFG_TARGET_OS"] = "android"
    elif env["CARGO_CFG_TARGET_OS"] == "darwin":
      env["CARGO_CFG_TARGET_OS"] = "macos"
    env["CARGO_CFG_TARGET_ENDIAN"] = "little"
    if env["CARGO_CFG_TARGET_OS"] == "windows":
      env["CARGO_CFG_TARGET_FAMILY"] = "windows"
    else:
      env["CARGO_CFG_TARGET_FAMILY"] = "unix"
    if args.features:
      for f in args.features:
        feature_name = f.upper().replace("-", "_")
        env["CARGO_FEATURE_%s" % feature_name] = "1"
    if args.env:
      for e in args.env:
        (k, v) = e.split("=")
        env[k] = v
    # Pass through a couple which are useful for diagnostics
    if os.environ.get("RUST_BACKTRACE"):
      env["RUST_BACKTRACE"] = os.environ.get("RUST_BACKTRACE")
    if os.environ.get("RUST_LOG"):
      env["RUST_LOG"] = os.environ.get("RUST_LOG")

    # In the future we should, set all the variables listed here:
    # https://doc.rust-lang.org/cargo/reference/environment-variables.html#environment-variables-cargo-sets-for-build-scripts

    proc = subprocess.run([os.path.abspath(args.build_script)],
                          env=env,
                          cwd=args.src_dir,
                          encoding='utf8',
                          stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE)

    if proc.stderr.rstrip():
      print(proc.stderr.rstrip(), file=sys.stderr)
    proc.check_returncode()

    flags = ""
    for line in proc.stdout.split("\n"):
      m = RUSTC_CFG_LINE.match(line.rstrip())
      if m:
        flags = "%s--cfg\n%s\n" % (flags, m.group(1))

    # AtomicOutput will ensure we only write to the file on disk if what we
    # give to write() is different than what's currently on disk.
    with action_helpers.atomic_output(args.output) as output:
      output.write(flags.encode("utf-8"))

    # Copy any generated code out of the temporary directory,
    # atomically.
    if args.generated_files:
      for generated_file in args.generated_files:
        in_path = os.path.join(tempdir, generated_file)
        out_path = os.path.join(args.out_dir, generated_file)
        out_dir = os.path.dirname(out_path)
        if not os.path.exists(out_dir):
          os.makedirs(out_dir)
        with open(in_path, 'rb') as input:
          with action_helpers.atomic_output(out_path) as output:
            content = input.read()
            output.write(content)


if __name__ == '__main__':
  sys.exit(main())
