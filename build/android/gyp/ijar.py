#!/usr/bin/env python3
#
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import subprocess
import sys

from util import build_utils


# python -c "import zipfile; zipfile.ZipFile('test.jar', 'w')"
# du -b test.jar
_EMPTY_JAR_SIZE = 22


def main():
  # The point of this wrapper is to use AtomicOutput so that output timestamps
  # are not updated when outputs are unchanged.
  ijar_bin, in_jar, out_jar = sys.argv[1:]
  with build_utils.AtomicOutput(out_jar) as f:
    # ijar fails on empty jars: https://github.com/bazelbuild/bazel/issues/10162
    if os.path.getsize(in_jar) <= _EMPTY_JAR_SIZE:
      with open(in_jar, 'rb') as in_f:
        f.write(in_f.read())
    else:
      build_utils.CheckOutput([ijar_bin, in_jar, f.name])


if __name__ == '__main__':
  main()
