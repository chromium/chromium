# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper script for GN to calculate the hash of an arbitrary file.

Currently used by the MSan GN config to more deterministically force a rebuild
when the ignorelist changes, rather than requiring clobbers.

Run with:
  python3 gn_hash_file.py <file>
"""

import hashlib
import sys


def main():
  with open(sys.argv[1], 'rb') as f:
    print(hashlib.file_digest(f, 'sha256').hexdigest())


if __name__ == '__main__':
  main()
