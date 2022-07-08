#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Simple helper script to run pytype on //build/fuchsia/test code."""

import os
import sys

from coveragetest import COVERED_FILES

FUCHSIA_TEST_DIR = os.path.abspath(os.path.dirname(__file__))

sys.path.append(os.path.join(FUCHSIA_TEST_DIR, '..', '..', '..', 'testing'))

from pytype_common import pytype_runner  # pylint: disable=wrong-import-position

FILES_AND_DIRECTORIES_TO_CHECK = [
    os.path.join(FUCHSIA_TEST_DIR, f) for f in COVERED_FILES
]
TEST_NAME = 'fuchsia_pytype'
TEST_LOCATION = "//build/fuchsia/test/run_pytype.py"


def main() -> int:
    """Run pytype check."""

    return pytype_runner.run_pytype(TEST_NAME, TEST_LOCATION,
                                    FILES_AND_DIRECTORIES_TO_CHECK, [],
                                    FUCHSIA_TEST_DIR)


if __name__ == '__main__':
    sys.exit(main())
