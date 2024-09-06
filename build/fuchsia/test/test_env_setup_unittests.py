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

# TODO(crbug.com/352409265): Try to run the following tests on trybot.


# Test names should be self-explained, no point of adding function docstring.
# pylint: disable=missing-function-docstring


class TestEnvSetupTests(unittest.TestCase):
    """Test class."""

    @staticmethod
    def _merge_env(env: dict, env2: dict) -> dict:
        if env and env2:
            return {**env, **env2}
        # Always copy to avoid changing os.environ.
        if env:
            return {**env}
        if env2:
            return {**env2}
        return {}

    def _start_test_env(self, env: dict = None) -> subprocess.Popen:
        proc = subprocess.Popen([
            os.path.join(os.path.dirname(__file__), 'test_env_setup.py'),
            '--logs-dir', '/tmp/test_env_setup'
        ],
                                env=TestEnvSetupTests._merge_env(
                                    os.environ, env))
        while not os.path.isfile('/tmp/test_env_setup/test_env_setup.' +
                                 str(proc.pid) + '.pid'):
            proc.poll()
            self.assertIsNone(proc.returncode)
            time.sleep(1)
        return proc

    def _run_without_packages(self, env: dict = None):
        proc = self._start_test_env(env)
        proc.terminate()
        proc.wait()

    def test_run_without_packages(self):
        if running_unattended():
            # The test needs sdk and images to run and it's not designed to work
            # on platforms other than linux.
            return
        self._run_without_packages()

    def test_run_without_packages_unattended(self):
        if running_unattended():
            return
        # Do not impact the environment of the current process.
        self._run_without_packages({'CHROME_HEADLESS': '1'})

    def _run_with_base_tests(self, env: dict = None):
        env = TestEnvSetupTests._merge_env(
            {'FFX_ISOLATE_DIR': '/tmp/test_env_setup/daemon'}, env)
        proc = self._start_test_env(env)
        try:
            subprocess.run([
                os.path.join(os.path.dirname(__file__),
                             '../../out/fuchsia/bin/run_base_unittests'),
                '--logs-dir', '/tmp/test_env_setup', '--device'
            ],
                           env=TestEnvSetupTests._merge_env(os.environ, env),
                           check=True,
                           stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL)
        finally:
            proc.terminate()
            proc.wait()

    def test_run_with_base_tests(self):
        if running_unattended():
            return
        self._run_with_base_tests()

    def test_run_with_base_tests_unattended(self):
        if running_unattended():
            return
        self._run_with_base_tests({'CHROME_HEADLESS': '1'})


if __name__ == '__main__':
    unittest.main()
