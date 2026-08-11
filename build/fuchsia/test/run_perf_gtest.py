# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implements commands for running GTest performance tests."""

import argparse
import json
import logging
import os
import subprocess
from typing import List, Optional

from common import DIR_SRC_ROOT
from test_runner import TestRunner


class PerfGtestTestRunner(TestRunner):
    """Test runner for running GTest performance tests."""

    def __init__(
        self,
        out_dir: str,
        test_args: List[str],
        target_id: Optional[str],
        logs_dir: Optional[str],
    ) -> None:
        if not test_args:
            raise ValueError(
                'Executable name must be passed as the first test argument.'
            )

        # The first argument is the host-side wrapper script path (e.g.,
        # "bin/run_views_perftests_fuchsia"). We need to extract the clean
        # package name (e.g., "views_perftests_fuchsia") so that the TestRunner
        # base class can locate and install the package on the device.
        assert test_args[0].startswith('bin/run_')
        package = test_args[0][8:]

        super().__init__(out_dir, test_args, [package], target_id)

        # Create the invocations directory if logs_dir is provided or can be
        # obtained from the environment. This is required because the directory
        # is not automatically created by the isolated_script recipe (unlike
        # the gtest recipe), which causes ResultDB integration to fail if the
        # directory is missing.
        logs_dir = logs_dir or os.environ.get('ISOLATED_OUTDIR')
        if logs_dir:
            invocations_dir = os.path.join(logs_dir, 'invocations')
            try:
                os.makedirs(invocations_dir, exist_ok=True)
            except OSError as error:
                logging.warning(
                    'Failed to create invocations directory %s: %s',
                    invocations_dir,
                    error,
                )

    def run_test(self):
        # Prevent run_performance_tests.py from uploading to ResultSink by
        # removing LUCI_CONTEXT from the environment.
        # We will restructure the output JSON file ourselves later so that
        # the recipe can upload it correctly under the "gtest" scheme.
        os.environ.pop('LUCI_CONTEXT', None)

        test_cmd = [
            os.path.join(
                DIR_SRC_ROOT, 'testing', 'scripts', 'run_performance_tests.py'
            )
        ]
        if self._test_args:
            test_cmd.extend(self._test_args)

        if self._target_id:
            test_cmd.extend(
                [
                    f'--passthrough-arg=--target-id={self._target_id}',
                    '--passthrough-arg=-d',
                ]
            )

        result = subprocess.run(test_cmd, cwd=self._out_dir, check=False)

        # Find the output JSON file path.
        parser = argparse.ArgumentParser()
        parser.add_argument('--isolated-script-test-output')
        parsed_args, _ = parser.parse_known_args(self._test_args)
        output_file = parsed_args.isolated_script_test_output

        # Restructure the output JSON to match the "gtest" scheme.
        if output_file and os.path.exists(output_file):
            try:
                _restructure_output_json(output_file)
            except (
                OSError,
                json.JSONDecodeError,
                KeyError,
                AttributeError,
                TypeError,
            ) as error:
                # Don't fail the step if restructuring fails, but log it.
                logging.warning(
                    'Failed to restructure output JSON for GTest scheme: %s',
                    error,
                )

        return result


def _restructure_output_json(output_file_path: str):
    with open(output_file_path, 'r') as f:
        results = json.load(f)

    # The output JSON from run_performance_tests.py uses a flat structure
    # (Suite_shard_N as the key) which is not valid under the "gtest" scheme
    # (which requires a Suite/Case hierarchy). We restructure it here by
    # using "summary" as the case name since run_performance_tests.py
    # doesn't provide the case names.
    if 'tests' in results:
        new_tests = {}
        for flat_key, val in results['tests'].items():
            new_tests[flat_key] = {'summary': val}
        results['tests'] = new_tests

        with open(output_file_path, 'w') as f:
            json.dump(results, f)
