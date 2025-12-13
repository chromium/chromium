# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Assert a set of GTests pass consistently."""

import collections
import json
import pathlib
import re
import subprocess
import tempfile


def assert_test_count(_output: str, context):
    """Asserts that the model output contains the correct test count.

    The prompt is expected to request this format from the model.

    This function reads the content of a specified C++ test file, counts the
    number of GTest macros (e.g. `TEST(`, `TEST_F(`) to determine the
    actual number of tests, and then verifies that the model's output includes
    a key-value pair like `'test_count': <number>` or `"test_count": <number>`
    with the matching count.
    """
    config = context.get('config', {})
    file_path_str = config.get('file')
    if not file_path_str:
        return _failure('"file" not specified in assert config')

    file_path = _repo_root() / file_path_str
    if not file_path.exists():
        return _failure(f'File not found: {file_path}')
    content = file_path.read_text(encoding='utf-8')

    test_macros = [
        'TEST', 'TEST_F', 'TEST_P', 'TYPED_TEST', 'TYPED_TEST_SUITE'
    ]
    regex = r'^\s*(?:' + '|'.join(test_macros) + r')\('
    test_count = len(re.findall(regex, content, re.MULTILINE))
    match = re.search(r'["\']test_count["\']\s*:\s*(\d+)', _output)
    if not match:
        return _failure(
                'Could not find `"test_count": <number>` in model output.')

    reported_count = int(match.group(1))

    if test_count != reported_count:
        return _failure(f'Expected {test_count} tests, but model reported '
                        f'{reported_count}')

    return True


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
