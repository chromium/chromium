#!/usr/bin/env/python3

# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# See BUILD.gn in this directory for an explanation of what this script is for.

import argparse
import os
import sys
import shutil
import subprocess
import re

REMOVE_METADATA_SUFFIX_RE = re.compile(b"-[0-9a-f]*", re.I)


def expand_name(concise_name):
  return "lib%s.rlib" % concise_name


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
  parser.add_argument("--stdlibs",
                      help="Expected list of standard library libraries")
  parser.add_argument("--skip-stdlibs",
                      help="Standard library files to skip",
                      default="")
  args = parser.parse_args()
  # Ensure we handle each rlib in the expected list exactly once.
  if args.stdlibs:
    rlibs_expected = [expand_name(x) for x in args.stdlibs.split(',')]
  else:
    rlibs_expected = None
  rlibs_to_skip = [expand_name(x) for x in args.skip_stdlibs.split(',')]
  # Ask rustc where to find the stdlib for this target.
  rustc = os.path.join(args.rust_bin_dir, "rustc")
  rustc_args = [rustc, "--print", "target-libdir"]
  if args.target:
    rustc_args.extend(["--target", args.target])
  rustlib_dir = subprocess.check_output(rustc_args).rstrip()
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
    depfile.write("%s:" %
                  (os.path.join(args.output, expand_name(args.depfile_target))))
    for f in os.listdir(rustlib_dir):
      if f.endswith(b'.rlib'):
        # As standard Rust includes a hash on the end of each filename
        # representing certain metadata, to ensure that clients will link
        # against the correct version. As gn will be manually passing
        # the correct file path to our linker invocations, we don't need
        # that, and it would prevent us having the predictable filenames
        # which we need for statically computable gn dependency rules.
        (concise_name, count) = REMOVE_METADATA_SUFFIX_RE.subn(b"", f)
        if count == 0:
          raise Exception("Unable to remove suffix from %s" % f)
        if concise_name.decode() in rlibs_to_skip:
          continue
        if rlibs_expected is not None:
          if concise_name.decode() not in rlibs_expected:
            raise Exception("Found stdlib rlib that wasn't expected: %s" %
                            concise_name)
          rlibs_expected.remove(concise_name.decode())
        infile = os.path.join(rustlib_dir, f)
        outfile = os.path.join(str.encode(args.output), concise_name)
        depfile.write(" %s" % (infile.decode()))
        if (not os.path.exists(outfile)
            or os.stat(infile).st_mtime > os.stat(outfile).st_mtime):
          shutil.copy(infile, outfile)
    depfile.write("\n")
    if rlibs_expected:
      raise Exception("We failed to find all expected stdlib rlibs: %s" %
                      ','.join(rlibs_expected))


if __name__ == '__main__':
  sys.exit(main())
