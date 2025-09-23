# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for reporting test results."""

import dataclasses
import pathlib
import sys

import constants

sys.path.insert(0, str(constants.CHROMIUM_SRC / 'build' / 'util'))
from lib.results import result_sink
from lib.results import result_types


@dataclasses.dataclass
class TestResult:
    """Represents the result of a single test run."""
    # The path to the test file that was run.
    test_file: pathlib.Path
    # Whether the test ran successfully.
    success: bool
    # The duration of the test run in seconds.
    duration: float
    # Stdout/stderr of the test.
    test_log: str


def report_result(result_sink_client: result_sink.ResultSinkClient,
                  test_result: TestResult) -> None:
    """Reports a test result to ResultDB if possible.

    Args:
        result_sink_client: A ResultSinkClient to use for reporting.
        test_result: A TestResult instance containing the result to report.
    """
    relative_path = test_result.test_file.relative_to(constants.CHROMIUM_SRC)
    posix_path = relative_path.as_posix()
    result_sink_client.Post(
        test_id=str(posix_path),
        status=result_types.PASS if test_result.success else result_types.FAIL,
        duration=test_result.duration * 1000,
        test_log=test_result.test_log,
        test_file=f'//{str(posix_path)}')
