# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for reporting test results."""

import dataclasses
import logging
import pprint
import queue
import sys
import threading

import constants
import eval_config

sys.path.insert(0, str(constants.CHROMIUM_SRC / 'build' / 'util'))
from lib.results import result_sink
from lib.results import result_types

_RESULT_THREAD_POLLING_SLEEP_DURATION = 0.5


@dataclasses.dataclass
class IterationResult:
    """Stores per-iteration data for a single pass@k iteration."""
    # Whether this iteration ran successfully.
    success: bool
    # The duration of the iteration in seconds.
    duration: float
    # Stdout/stderr of the iteration.
    test_log: str
    # A mapping of metric name to value. Metric names can be nested, e.g.
    # {
    #   'token_usage': {
    #     'input': 10,
    #     'output': 20,
    #   },
    # }
    metrics: dict[str, dict | float]


@dataclasses.dataclass
class TestResult:
    """Represents the result of a single test run.

    This encapsulates data from one or more underlying iterations used for
    pass@k functionality.
    """
    # The config used for this test.
    config: eval_config.TestConfig
    # Whether the test ran successfully.
    success: bool
    # IterationResults for each iteration of this test.
    iteration_results: list[IterationResult]

    def __lt__(self, other: 'TestResult') -> bool:
        return self.config.test_file < other.config.test_file

    @property
    def combined_logs(self):
        if len(self.iteration_results) > 1:
            return '\n'.join(
                f'Iteration #{i}:\n{result.test_log}'
                for i, result in enumerate(self.iteration_results))
        return '\n'.join(result.test_log for result in self.iteration_results)

    @property
    def total_duration(self):
        return sum(i.duration for i in self.iteration_results)

    @property
    def average_duration(self):
        return self.total_duration / len(self.iteration_results)

    @property
    def successful_runs(self):
        return sum(i.success for i in self.iteration_results)


@dataclasses.dataclass
class ResultOptions:
    """Options for configuring result reporting."""
    # Always print test logs to stdout instead of only for failed tests.
    print_output_on_success: bool
    # Upload metrics to the perf dashboard.
    enable_perf_uploading: bool
    # The git revision to report to the perf dashboard.
    git_revision: str | None


class AtomicCounter:
    """Thread-safe integer counter."""

    def __init__(self):
        self._counter = 0
        self._lock = threading.Lock()

    def get(self) -> int:
        with self._lock:
            return self._counter

    def increment(self) -> None:
        with self._lock:
            self._counter += 1


def report_result(result_sink_client: result_sink.ResultSinkClient,
                  test_result: TestResult) -> None:
    """Reports a test result to ResultDB if possible.

    Args:
        result_sink_client: A ResultSinkClient to use for reporting.
        test_result: A TestResult instance containing the result to report.
    """
    relative_path = test_result.config.test_file.relative_to(
        constants.CHROMIUM_SRC)
    posix_path = relative_path.as_posix()
    result_sink_client.Post(
        test_id=str(posix_path),
        status=result_types.PASS if test_result.success else result_types.FAIL,
        duration=test_result.total_duration * 1000,
        test_log=test_result.combined_logs,
        test_id_structured={
            'coarseName': '',  # Leave blank for scheme 'flat'.
            'fineName': '',  # Leave blank for scheme 'flat'.
            'caseNameComponents': [str(posix_path)],
        },
        test_file=f'//{str(posix_path)}')


class ResultThread(threading.Thread):
    """Class for reporting test results from a queue."""

    def __init__(self, result_options: ResultOptions, **kwargs):
        """
        Args:
            result_options: A ResultOptions instance whose attributes will be
                used when configuring this object.
        """
        super().__init__(daemon=True, **kwargs)
        self.result_input_queue = queue.Queue()
        self.failed_result_output_queue = queue.Queue()
        self.total_results_reported = AtomicCounter()
        self._result_options = result_options
        self._shutdown_event = threading.Event()
        self._result_sink_client = result_sink.TryInitClient()
        self._fatal_exception = None

    def run(self) -> None:
        try:
            self._process_incoming_results_until_shutdown()
        except Exception as e:
            self._fatal_exception = e

    def _process_incoming_results_until_shutdown(self) -> None:
        while not self._shutdown_event.is_set():
            try:
                test_result = self.result_input_queue.get(
                    timeout=_RESULT_THREAD_POLLING_SLEEP_DURATION)
            except queue.Empty:
                continue

            # TODO(crbug.com/449818513): Actually report this to the perf
            # dashboard or to ResultDB, whichever we end up using for tracking
            # token usage and test scores.
            pp = pprint.PrettyPrinter(indent=2)
            logging.debug('Metrics: %s',
                          pp.pformat(test_result.iteration_results[0].metrics))
            if (not test_result.success
                    or self._result_options.print_output_on_success):
                sys.stdout.write(test_result.combined_logs)
            if self._result_sink_client:
                report_result(self._result_sink_client, test_result)
            if test_result.success:
                logging.info('Test passed in %.2f seconds: %s',
                             test_result.total_duration,
                             str(test_result.config.test_file))
            else:
                logging.warning('Test failed in %.2f seconds: %s',
                                test_result.total_duration,
                                str(test_result.config.test_file))
                self.failed_result_output_queue.put(test_result)

            self.total_results_reported.increment()

    def shutdown(self) -> None:
        """Tells the thread to shut down gracefully."""
        self._shutdown_event.set()

    def maybe_reraise_fatal_exception(self) -> None:
        """Reraises the fatal exception that caused the thread to die.

        No-op if no exception is stored.
        """
        if self._fatal_exception:
            raise self._fatal_exception
