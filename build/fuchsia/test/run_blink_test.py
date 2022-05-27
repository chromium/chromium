# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implements commands for running blink web tests."""

import os
import subprocess

from common import DIR_SRC_ROOT, resolve_packages
from test_runner import TestRunner

_BLINK_TEST_SCRIPT = os.path.join(DIR_SRC_ROOT, 'third_party', 'blink',
                                  'tools', 'run_web_tests.py')


class BlinkTestRunner(TestRunner):
    """Test runner for running blink web tests."""

    def _get_packages(self):
        return ['content_shell']

    def run_test(self):
        resolve_packages(self.packages)
        test_cmd = [_BLINK_TEST_SCRIPT]
        test_cmd.append('--platform=fuchsia')
        if self._test_args:
            test_cmd.extend(self._test_args)
        return subprocess.run(test_cmd, check=True)
