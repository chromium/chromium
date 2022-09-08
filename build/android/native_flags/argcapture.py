#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Writes arguments to a file."""

import sys


def main():
  with open(sys.argv[1], 'w') as f:
    f.write('\n'.join(sys.argv[2:]))
    f.write('\n')


if __name__ == '__main__':
  main()
