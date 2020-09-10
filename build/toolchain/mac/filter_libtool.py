# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import os
import re
import subprocess
import sys

# This script executes libool and filters out logspam lines like:
#    '/path/to/libtool: file: foo.o has no symbols'

BLACKLIST_PATTERNS = [
    re.compile(v) for v in [
        r'^.*libtool: (?:for architecture: \S* )?file: .* has no symbols$',
        r'^.*libtool: warning for library: .* the table of contents is empty '
        r'\(no object file members in the library define global symbols\)$',
        r'^.*libtool: warning same member name \(\S*\) in output file used for '
        r'input files: \S* and: \S* \(due to use of basename, truncation, '
        r'blank padding or duplicate input files\)$',
    ]
]


def IsBlacklistedLine(line):
  """Returns whether the line should be filtered out."""
  for pattern in BLACKLIST_PATTERNS:
    if pattern.match(line):
      return True
  return False


def Main(cmd_list):
  env = os.environ.copy()
  libtoolout = subprocess.Popen(cmd_list, stderr=subprocess.PIPE, env=env)
  _, err = libtoolout.communicate()
  for line in err.decode('UTF-8').splitlines():
    if not IsBlacklistedLine(line):
      print(line, file=sys.stderr)
  return libtoolout.returncode


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
