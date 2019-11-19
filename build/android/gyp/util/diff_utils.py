#!/usr/bin/env python
#
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import difflib
from util import build_utils


def DiffFileContents(expected_path, actual_path):
  """Check file contents for equality and return the diff or None."""
  with open(expected_path) as f_expected, open(actual_path) as f_actual:
    expected_lines = f_expected.readlines()
    actual_lines = f_actual.readlines()

  if expected_lines == actual_lines:
    return None

  expected_path = os.path.relpath(expected_path, build_utils.DIR_SOURCE_ROOT)
  actual_path = os.path.relpath(actual_path, build_utils.DIR_SOURCE_ROOT)

  diff = difflib.unified_diff(
      expected_lines,
      actual_lines,
      fromfile=os.path.join('before', expected_path),
      tofile=os.path.join('after', expected_path),
      n=0)

  # Space added before "patch" so that giant command is not put in bash history.
  return """\
Files Compared:
  * {}
  * {}

If you are looking at this through LogDog, click "Raw log" before copying.
See https://bugs.chromium.org/p/chromium/issues/detail?id=984616.

To update the file, run:
########### START ###########
 patch -p1 <<'END_DIFF'
{}
END_DIFF
############ END ############
""".format(expected_path, actual_path, ''.join(diff).rstrip())
