# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implements commands for running blink web tests."""

import os
import subprocess

from argparse import Namespace
from typing import Optional

from common import DIR_SRC_ROOT
from test_runner import TestRunner

_BLINK_TEST_SCRIPT = os.path.join(DIR_SRC_ROOT, 'third_party', 'blink',
                                  'tools', 'run_web_tests.py')


class BlinkTestRunner(TestRunner):
    """Test runner for running blink web tests."""

    def __init__(self, out_dir: str, test_args: Namespace,
                 target_id: Optional[str]) -> None:
        super().__init__(out_dir, test_args, ['content_shell'], target_id)

    def run_test(self):
        test_cmd = [_BLINK_TEST_SCRIPT, '-t', os.path.basename(self._out_dir)]

        if self._test_args:
            test_cmd.extend(self._test_args)
        return subprocess.run(test_cmd, check=True)
