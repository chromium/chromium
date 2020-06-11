#!/usr/bin/env python
# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Prints the target paths of the given symlinks.

Prints out each target in the order that the links were passed in.
"""

import os
import sys


def main():
  for link_name in sys.argv[1:]:
    if not os.path.islink(link_name):
      sys.stderr.write("%s is not a link" % link_name)
      return 1
    target = os.readlink(link_name)
    if not os.path.isabs(target):
      target = os.path.join(os.path.dirname(link_name), target)
    print(os.path.realpath(target))
  return 0


if __name__ == '__main__':
  sys.exit(main())
