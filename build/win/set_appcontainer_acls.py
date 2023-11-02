#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Sets the app container ACLs on directory."""

import os
import argparse
import sys

SRC_DIR = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

sys.path.append(os.path.join(SRC_DIR, 'testing', 'scripts'))

import common


def main():
  parser = argparse.ArgumentParser(
      description='Sets App Container ACL on a directory.')
  parser.add_argument('--stamp',
                      required=False,
                      help='Touch this stamp file on success.')
  parser.add_argument('--dir', required=True, help='Set ACL on this directory.')
  #  parser.add_argument('--fail', required=True, help='Argument to fail.')
  args = parser.parse_args()

  common.set_lpac_acls(os.path.abspath(args.dir))
  if args.stamp:
    open(args.stamp, 'w').close()  # Update mtime on stamp file.


if __name__ == '__main__':
  main()
