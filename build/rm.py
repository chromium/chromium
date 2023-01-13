#!/usr/bin/env python3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Delete a file.

This module works much like the rm posix command.
"""


import argparse
import os
import sys


def Main():
  parser = argparse.ArgumentParser()
  parser.add_argument('files', nargs='+')
  parser.add_argument('-f', '--force', action='store_true',
                      help="don't err on missing")
  parser.add_argument('--stamp', required=True, help='touch this file')
  args = parser.parse_args()
  for f in args.files:
    try:
      os.remove(f)
    except OSError:
      if not args.force:
        print("'%s' does not exist" % f, file=sys.stderr)
        return 1

  with open(args.stamp, 'w'):
    os.utime(args.stamp, None)

  return 0


if __name__ == '__main__':
  sys.exit(Main())
