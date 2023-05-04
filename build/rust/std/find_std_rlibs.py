#!/usr/bin/env/python3

# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# See BUILD.gn in this directory for an explanation of what this script is for.

import argparse
import os
import stat
import sys
import shutil
import subprocess
import re

from collections import defaultdict

EXPECTED_STDLIB_INPUT_REGEX = re.compile(r"([0-9a-z_]+)(?:-([0-9]+))?$")
RLIB_NAME_REGEX = re.compile(r"lib([0-9a-z_]+)-([0-9a-f]+)\.rlib$")


def main():
  parser = argparse.ArgumentParser("find_std_rlibs.py")
  parser.add_argument("--rust-bin-dir",
                      help="Path to Rust binaries",
                      required=True),
  parser.add_argument("--target", help="Rust target triple", required=False),
  parser.add_argument("--output",
                      help="Path to rlibs without suffixes",
                      required=True)
  parser.add_argument("--depfile", help="Path to write depfile", required=True)
  parser.add_argument("--depfile-target",
                      help="Target to key depfile around",
                      required=True)
  parser.add_argument("--extra-libs",
                      help="List of extra non-libstd sysroot libraries")
  parser.add_argument("--rustc-revision",
                      help="Not used, just passed from GN to add a dependency"
                      " on the rustc version.")
  args = parser.parse_args()

  extra_libs = set()
  if args.extra_libs:
    for lib in args.extra_libs.split(','):
      extra_libs.add(lib)

  # Ask rustc where to find the stdlib for this target.
  rustc = os.path.join(args.rust_bin_dir, "rustc")
  rustc_args = [rustc, "--print", "target-libdir"]
  if args.target:
    rustc_args.extend(["--target", args.target])
  rustlib_dir = subprocess.check_output(rustc_args).rstrip().decode()

  # Copy the rlibs to a predictable location. Whilst we're doing so,
  # also write a .d file so that ninja knows it doesn't need to do this
  # again unless the source rlibs change.
  # Format:
  # <output path to>/lib<lib name.rlib>: <path to each Rust stlib rlib>
  with open(args.depfile, 'w') as depfile:
    # Ninja isn't versatile at understanding depfiles. We have to say that a
    # single output depends on all the inputs. We choose any one of the
    # output rlibs for that purpose. If any of the input rlibs change, ninja
    # will run this script again and we'll copy them all afresh.
    depfile.write(
        "%s:" % (os.path.join(args.output, "lib%s.rlib" % args.depfile_target)))

    def copy_file(infile, outfile):
      depfile.write(f" {infile}")
      if (not os.path.exists(outfile)
          or os.stat(infile).st_mtime != os.stat(outfile).st_mtime):
        if os.path.exists(outfile):
          st = os.stat(outfile)
          os.chmod(outfile, st.st_mode | stat.S_IWUSR)
        shutil.copy(infile, outfile)

    # Each rlib is named "lib<crate_name>-<metadata>.rlib". The metadata
    # disambiguates multiple crates of the same name. We want to throw away the
    # metadata and use stable names. To do so, we replace the metadata bit with
    # a simple number 1, 2, etc. It doesn't matter how we assign these numbers
    # as long as it's consistent for a particular set of rlibs.

    # The rlib names present in the Rust distribution, including metadata. We
    # sort this list so crates of the same name are ordered by metadata. Also
    # filter out names that aren't rlibs.
    rlibs_present = [
        name for name in os.listdir(rustlib_dir) if name.endswith('.rlib')
    ]
    rlibs_present.sort()

    # Keep a count of the instances a crate name, so we can disambiguate the
    # rlibs with an incrementing number at the end.
    rlibs_seen = defaultdict(lambda: 0)

    for f in rlibs_present:
      # As standard Rust includes a hash on the end of each filename
      # representing certain metadata, to ensure that clients will link
      # against the correct version. As gn will be manually passing
      # the correct file path to our linker invocations, we don't need
      # that, and it would prevent us having the predictable filenames
      # which we need for statically computable gn dependency rules.
      (crate_name, metadata) = RLIB_NAME_REGEX.match(f).group(1, 2)

      # Use the number of times we've seen this name to disambiguate the output
      # filenames. Since we sort the input filenames including the metadata,
      # this will be the same every time.
      #
      # Only append the times seen if it is greater than 1. This allows the
      # BUILD.gn file to avoid adding '-1' to every name if there's only one
      # version of a particular one.
      rlibs_seen[crate_name] += 1
      if rlibs_seen[crate_name] == 1:
        concise_name = crate_name
      else:
        concise_name = "%s-%d" % (crate_name, rlibs_seen[crate_name])

      output_filename = f"lib{concise_name}.rlib"

      infile = os.path.join(rustlib_dir, f)
      outfile = os.path.join(args.output, output_filename)
      copy_file(infile, outfile)

    for f in extra_libs:
      infile = os.path.join(rustlib_dir, f)
      outfile = os.path.join(args.output, f)
      copy_file(infile, outfile)

    depfile.write("\n")


if __name__ == '__main__':
  sys.exit(main())
