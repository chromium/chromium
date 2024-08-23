#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing test_env_setup.py."""

import os
import subprocess
import time
import unittest

from compatible_utils import running_unattended

# Test names should be self-explained, no point of adding function docstring.
# pylint: disable=missing-function-docstring


class TestEnvSetupTests(unittest.TestCase):
    """Test class."""

    def test_run_without_packages(self):
        if running_unattended():
            # The test needs sdk and images to run and it's not designed to work
            # on platforms other than linux.
            return

        proc = subprocess.Popen([
            os.path.join(os.path.dirname(__file__), 'test_env_setup.py'),
            '--logs-dir', '/tmp'
        ])
        while not os.path.isfile('/tmp/test_env_setup.' + str(proc.pid) +
                                 '.pid'):
            proc.poll()
            self.assertIsNone(proc.returncode)
            time.sleep(1)
        proc.terminate()
        proc.wait()


if __name__ == '__main__':
    unittest.main()
