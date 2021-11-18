#!/usr/bin/env python3

# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import pathlib
import subprocess
import sys

# This script performs a simple function to work around some of the
# parameter escaping performed by ninja/gn.
#
# rustc invocations are given access to {{rustflags}} and {{ldflags}}.
# We want to pass {{ldflags}} into rustc, using -Clink-args="{{ldflags}}".
# Unfortunately, ninja assumes that each item in {{ldflags}} is an
# independent command-line argument and will have escaped them appropriately
# for use on a bare command line, instead of in a string.
#
# This script converts such {{ldflags}} into individual -Clink-arg=X
# arguments to rustc.
#
# Usage:
#   rust_link_wrapper.py --rustc <path to rustc> -- <normal rustc args>
#      LDFLAGS {{ldflags}}
# The LDFLAGS token is discarded, and everything after that is converted
# to being a series of -Clink-arg=X arguments.


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--rustc', required=True, type=pathlib.Path)
  parser.add_argument('args', metavar='ARG', nargs='+')

  args = parser.parse_args()

  remaining_args = args.args

  separator = remaining_args.index("LDFLAGS")

  rustc_args = remaining_args[:separator]
  rustc_args.extend(
      ["-Clink-arg=%s" % arg for arg in remaining_args[separator + 1:]])

  return subprocess.run([args.rustc, *rustc_args]).returncode


if __name__ == '__main__':
  sys.exit(main())
