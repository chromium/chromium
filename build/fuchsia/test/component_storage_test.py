#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" Runs component_storage tests on a fuchsia emulator. """

import logging
import os
import subprocess
import sys

from test_env_setup import wait_for_env_setup

LOG_DIR = os.environ.get('ISOLATED_OUTDIR', '/tmp')


def main() -> int:
    """Entry of the test."""
    proc = subprocess.Popen([
        os.path.join(os.path.dirname(__file__), 'test_env_setup.py'),
        '--logs-dir', LOG_DIR
    ])
    assert wait_for_env_setup(proc, LOG_DIR)
    logging.warning('test_env_setup.py is running on process %s', proc.pid)
    proc.terminate()
    return proc.wait()


if __name__ == '__main__':
    sys.exit(main())
