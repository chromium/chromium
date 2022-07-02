#!/usr/bin/env python3
# Copyright (c) 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Delete .ninja_deps if it references files inside libc++'s __string dir,
which has since been reverted back to a file, and would cause Ninja fail on
Windows. See crbug.com/1337238 ..."""

import os
import sys


def main():
  os.chdir(os.path.join(os.path.dirname(__file__), '..'))

  if os.path.isdir('buildtools/third_party/libc++/trunk/include/__string'):
    # If __string is a dir, Ninja will not fail.
    return 0

  for d in os.listdir('out'):
    obj_file = os.path.join(
        'out', d,
        'obj/buildtools/third_party/libc++/libc++/legacy_debug_handler.obj')
    if not os.path.exists(obj_file):
      # It seems we have not done a build with the libc++ roll.
      continue

    try:
      deps = os.path.join('out', d, '.ninja_deps')
      if b'__string/char_traits.h' in open(deps, 'rb').read():
        print('Deleting ', deps)
        os.remove(deps)
        print('Deleting ', obj_file)
        os.remove(obj_file)
    except FileNotFoundError:
      pass

  return 0


if __name__ == '__main__':
  sys.exit(main())
