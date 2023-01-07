#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Simple helper script to run pytype on Gold Python code."""

import os
import sys

GOLD_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_SRC_DIR = os.path.realpath(os.path.join(GOLD_DIR, '..', '..'))

sys.path.append(os.path.join(CHROMIUM_SRC_DIR, 'testing'))

from pytype_common import pytype_runner  # pylint: disable=wrong-import-position

EXTRA_PATHS_COMPONENTS = [
    ('build', ),
    ('testing', ),
]
EXTRA_PATHS = [
    os.path.join(CHROMIUM_SRC_DIR, *p) for p in EXTRA_PATHS_COMPONENTS
]
EXTRA_PATHS.append(GOLD_DIR)

FILES_AND_DIRECTORIES_TO_CHECK = [
    '.',
]
FILES_AND_DIRECTORIES_TO_CHECK = [
    os.path.join(GOLD_DIR, f) for f in FILES_AND_DIRECTORIES_TO_CHECK
]

TEST_NAME = 'gold_common_pytype'
TEST_LOCATION = '//build/skia_gold_common/run_pytype.py'


def main() -> int:
  return pytype_runner.run_pytype(TEST_NAME, TEST_LOCATION,
                                  FILES_AND_DIRECTORIES_TO_CHECK, EXTRA_PATHS,
                                  GOLD_DIR)


if __name__ == '__main__':
  sys.exit(main())
