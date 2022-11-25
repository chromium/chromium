#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Wrapper script to run action remotely through rewrapper with gn."""

import subprocess
import sys


def main():
  # Pass through all args. Use check=True to ensure failure is raised.
  args = sys.argv[1:]
  proc = subprocess.run(args, check=True)
  return proc.returncode


if __name__ == '__main__':
  sys.exit(main())
