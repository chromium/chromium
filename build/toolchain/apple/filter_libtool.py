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

SUPPRESSED_PATTERNS = [
    re.compile(v) for v in [
        r'^.*libtool: (?:for architecture: \S* )?file: .* has no symbols$',
        # Xcode 11 spelling of the "empty archive" warning.
        # TODO(thakis): Remove once we require Xcode 12.
        r'^.*libtool: warning for library: .* the table of contents is empty ' \
            r'\(no object file members in the library define global symbols\)$',
        # Xcode 12 spelling of the "empty archive" warning.
        r'^warning: .*libtool: archive library: .* ' \
            r'the table of contents is empty ',
            r'\(no object file members in the library define global symbols\)$',
        r'^.*libtool: warning same member name \(\S*\) in output file used ' \
            r'for input files: \S* and: \S* \(due to use of basename, ' \
            r'truncation, blank padding or duplicate input files\)$',
    ]
]


def ShouldSuppressLine(line):
  """Returns whether the line should be filtered out."""
  for pattern in SUPPRESSED_PATTERNS:
    if pattern.match(line):
      return True
  return False


def Main(cmd_list):
  env = os.environ.copy()
  libtoolout = subprocess.Popen(cmd_list, stderr=subprocess.PIPE, env=env)
  _, err = libtoolout.communicate()
  for line in err.decode('UTF-8').splitlines():
    if not ShouldSuppressLine(line):
      print(line, file=sys.stderr)
  return libtoolout.returncode


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
