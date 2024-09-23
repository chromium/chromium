# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implements commands for running GPU tests."""

import argparse
import os
import subprocess

from typing import List, Optional

from common import DIR_SRC_ROOT
from test_runner import TestRunner

_GPU_TEST_SCRIPT = os.path.join(DIR_SRC_ROOT, 'content', 'test', 'gpu',
                                'run_gpu_integration_test.py')
_PERF_TEST_SCRIPT = os.path.join(DIR_SRC_ROOT, 'tools', 'perf',
                                 'run_benchmark')


class TelemetryTestRunner(TestRunner):
    """Test runner for running GPU tests."""

    def __init__(self, test_type: str, out_dir: str, test_args: List[str],
                 target_id: Optional[str]) -> None:
        parser = argparse.ArgumentParser()
        parser.add_argument(
            '--browser', help='The browser to use for Telemetry based tests.')
        args, _ = parser.parse_known_args(test_args)

        if args.browser == 'web-engine-shell':
            packages = ['web_engine_shell']
        elif args.browser == 'fuchsia-chrome':
            packages = ['chrome']
        elif args.browser == 'cast-streaming-shell':
            packages = ['cast_streaming_shell']
        else:
            raise Exception('Unknown browser %s' % args.browser)

        if test_type == 'gpu':
            self._test_script = _GPU_TEST_SCRIPT
        elif test_type == 'perf':
            self._test_script = _PERF_TEST_SCRIPT
        else:
            raise ValueError('Test type can only be |gpu| or |perf|.')

        super().__init__(out_dir, test_args, packages, target_id)

    def run_test(self):
        test_cmd = [self._test_script]
        if self._test_args:
            test_cmd.extend(self._test_args)
        test_cmd.extend(['--chromium-output-directory', self._out_dir])
        if self._target_id:
            test_cmd.extend(['--fuchsia-target-id', self._target_id])
        return subprocess.run(test_cmd, check=True)
