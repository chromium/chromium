#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to get Clang's resource dir from `cros_target_cc`."""

import argparse
import subprocess
import sys
from typing import List


def main(argv: List[str]) -> None:
  parser = argparse.ArgumentParser(
      description=__doc__,
      formatter_class=argparse.RawDescriptionHelpFormatter,
  )
  parser.add_argument("cros_target_cc", help="The value of 'cros_target_cc'.")
  opts = parser.parse_args(argv)

  sys.exit(
      subprocess.run(
          (opts.cros_target_cc, "--print-resource-dir"),
          check=False,
      ).returncode)


if __name__ == "__main__":
  main(sys.argv[1:])
