#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Simple helper script to run pytype on //build/fuchsia/test code."""

import os
import sys

from coveragetest import COVERED_FILES

FUCHSIA_TEST_DIR = os.path.abspath(os.path.dirname(__file__))
DIR_SRC_DIR = os.path.realpath(os.path.join(FUCHSIA_TEST_DIR, '..', '..',
                                            '..'))

sys.path.append(os.path.join(FUCHSIA_TEST_DIR, '..', '..', '..', 'testing'))

from pytype_common import pytype_runner  # pylint: disable=wrong-import-position

EXTRA_PATHS_COMPONENTS = [
    ('build', 'util', 'lib', 'common'),
]
EXTRA_PATHS = [os.path.join(DIR_SRC_DIR, *p) for p in EXTRA_PATHS_COMPONENTS]
EXTRA_PATHS.append(FUCHSIA_TEST_DIR)

FILES_AND_DIRECTORIES_TO_CHECK = [
    os.path.join(FUCHSIA_TEST_DIR, f) for f in COVERED_FILES
]
TEST_NAME = 'fuchsia_pytype'
TEST_LOCATION = "//build/fuchsia/test/run_pytype.py"


def main() -> int:
    """Run pytype check."""

    return pytype_runner.run_pytype(TEST_NAME, TEST_LOCATION,
                                    FILES_AND_DIRECTORIES_TO_CHECK,
                                    EXTRA_PATHS, FUCHSIA_TEST_DIR)


if __name__ == '__main__':
    sys.exit(main())
