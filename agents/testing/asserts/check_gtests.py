# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Assert a set of GTests pass consistently."""

import collections
import json
import pathlib
import subprocess
import tempfile


def get_assert(_output: str, context):
    config = context.get('config', {})
    path_to_binary = _repo_root() / 'out' / 'Default' / config['binary']
    timeout = config.get('timeout', 60)

    with tempfile.NamedTemporaryFile() as summary_file:
        command = [
            str(path_to_binary),
            f'--test-launcher-summary-output={summary_file.name}',
        ]
        if test_filter := config.get('filter'):
            command.append(f'--gtest_filter={test_filter}')
        # Check exit code manually after checking the results summary.
        test_process = subprocess.run(command, timeout=timeout, check=False)
        summary = json.load(summary_file)

    return _evaluate_result(test_process, summary)


def _evaluate_result(test_process: subprocess.CompletedProcess, summary):
    statuses_by_test = collections.defaultdict(set)
    for tests_for_iteration in summary.get('per_iteration_data', []):
        for test, data_for_repeats in tests_for_iteration.items():
            statuses_by_test[test].update(data['status']
                                          for data in data_for_repeats)

    if not statuses_by_test:
        return _failure('No tests ran. Did the agent add the expected tests?')

    if failed_tests := {
            test
            for test, statuses in statuses_by_test.items()
            if not statuses.issubset({'SUCCESS'})
    }:
        return _failure('Some tests failed: ' +
                        ', '.join(sorted(failed_tests)))

    if test_process.returncode != 0:
        return _failure(f'Test process exited with {test_process.returncode}, '
                        'but no failures were detected in the summary')
    return True


def _failure(reason: str):
    return {'pass': False, 'score': 0, 'reason': reason}


def _repo_root() -> pathlib.Path:
    raw_output = subprocess.check_output(['gclient', 'root'], text=True)
    return pathlib.Path(raw_output.strip()) / 'src'
