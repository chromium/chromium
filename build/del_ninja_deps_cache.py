#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Delete .ninja_deps if it references files inside a libc++ dir which has
since been reverted back to a file, and would cause Ninja fail on Windows. See
crbug.com/1337238"""

import os
import sys


def main():
  os.chdir(os.path.join(os.path.dirname(__file__), '..'))

  # Paths that have switched between being a directory and regular file.
  bad_dirs = [
      'buildtools/third_party/libc++/trunk/include/__string',
      'buildtools/third_party/libc++/trunk/include/__tuple',
  ]

  for bad_dir in bad_dirs:
    if os.path.isdir(bad_dir):
      # If it's a dir, .ninja_deps referencing files in it is not a problem.
      continue

    for out_dir in os.listdir('out'):
      ninja_deps = os.path.join('out', out_dir, '.ninja_deps')
      try:
        if str.encode(bad_dir) + b'/' in open(ninja_deps, 'rb').read():
          print('Deleting', ninja_deps)
          os.remove(ninja_deps)
      except FileNotFoundError:
        pass

  return 0


if __name__ == '__main__':
  sys.exit(main())
