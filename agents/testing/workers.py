# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for isolated workers that run promptfoo tests."""

import collections.abc
import copy
import contextlib
import dataclasses
import json
import logging
import pathlib
import queue
import shutil
import subprocess
import sys
import tempfile
import threading
import time

import checkout_helpers
import constants
import eval_config
import promptfoo_installation
import results

sys.path.append(str(constants.CHROMIUM_SRC))
from agents.common import tempfile_ext

_ALL_QUEUED_TESTS_RUN_POLLING_SLEEP_DURATION = 0.5
_AVAILABLE_TEST_POLLING_SLEEP_DURATION = 0.1


class WorkDir(contextlib.AbstractContextManager):
    """A WorkDir used for testing destructive changes by an agent.

    Each workdir acts like a local shallow clone and has its own isolated
    checkout state (staging, untracked files, `//.gemini/extensions/`).
    """

    def __init__(
        self,
        name: str,
        src_root_dir: pathlib.Path,
        clean: bool,
        verbose: bool,
        force: bool,
    ):
        self.path = src_root_dir.parent.joinpath(name)
        self.src_root_dir = src_root_dir
        self.clean = clean
        self.verbose = verbose
        self.force = force
        self.btrfs = checkout_helpers.check_btrfs(src_root_dir)

    def __enter__(self) -> 'WorkDir':
        if self.path.exists():
            self._clean()

        logging.info('Creating new workdir: %s', self.path)
        start_time = time.time()
        if self.btrfs:
            # gclient-new-workdir does the same thing but reflinks the .git dirs
            # which we don't need to waste time on
            subprocess.check_call(
                ['btrfs', 'subvol', 'snapshot', self.src_root_dir, self.path],
                stdout=None if self.verbose else subprocess.DEVNULL,
                stderr=subprocess.STDOUT,
            )
        else:
            subprocess.check_call(
                ['gclient-new-workdir.py', self.src_root_dir, self.path],
                stdout=None if self.verbose else subprocess.DEVNULL,
                stderr=subprocess.STDOUT,
            )
        logging.debug('Took %s seconds', time.time() - start_time)
        return self

    def __exit__(self, *_exc_info) -> None:
        if self.clean:
            self._clean()

    def _clean(self) -> None:
        logging.info('Removing existing workdir: %s', self.path)
        cmd = ['sudo', 'btrfs', 'subvolume', 'delete', self.path]
        start_time = time.time()
        if self.btrfs:
            if self.force:
                cmd.insert(1, '-n')
            result = subprocess.call(
                cmd,
                stdout=None if self.verbose else subprocess.DEVNULL,
                stderr=subprocess.STDOUT,
            )
            if result != 0:
                logging.debug('Failed to remove with subvolume delete.')
        if not self.btrfs or result != 0:
            logging.debug('Removing with shutil')
            shutil.rmtree(self.path)
        logging.debug('Took %s seconds', time.time() - start_time)


@dataclasses.dataclass
class WorkerOptions:
    """Options for configuring a single worker."""
    # Whether to clean the workdir after a test.
    clean: bool
    # Whether to log verbosely.
    verbose: bool
    # Whether to force cleaning.
    force: bool
    # Whether to run tests in a sandbox.
    sandbox: bool
    # An optional path to a gemini-cli binary to use.
    gemini_cli_bin: pathlib.Path | None = None
    # An optional path to a nodejs binary to use.
    node_bin: pathlib.Path | None = None


class WorkerPool:
    """Abstracts away one or more WorkerThreads and a ResultThread."""

    def __init__(self, num_workers: int,
                 promptfoo: promptfoo_installation.PromptfooInstallation,
                 worker_options: WorkerOptions,
                 result_options: results.ResultOptions):
        """
        Args:
            num_workers: The number of workers to use to run tests.
            promptfoo: A PromptfooInstallation to use when running tests.
            worker_options: A WorkerOptions instance whose attributes will be
                used when setting up workers.
            result_options: A ResultOptions instance whose attributes will be
                used when handling test results.
        """
        assert num_workers > 0
        # Create a copy so that options cannot be externally modified.
        # This is not done for result_options because its result_handlers are
        # liable to contain callables that use locks under the hood for thread
        # safety, which causes errors with deepcopy due to them being
        # un-picklable.
        worker_options = copy.deepcopy(worker_options)

        self._result_thread = results.ResultThread(
            result_options=result_options)
        self._result_thread.start()

        self._total_tests_queued = 0
        self._test_input_queue = queue.Queue()
        self._workers = []
        for i in range(num_workers):
            worker_thread = WorkerThread(
                i,
                promptfoo,
                worker_options,
                test_input_queue=self._test_input_queue,
                test_result_queue=self._result_thread.result_input_queue,
            )
            worker_thread.start()
            self._workers.append(worker_thread)

    def __del__(self):
        self.shutdown_blocking(2)

    def queue_tests(
            self,
            tests: collections.abc.Collection[eval_config.TestConfig]) -> None:
        """Queues the provided tests to be run.

        Args:
            tests: A Collection of paths to promptfoo test configs to run.
        """
        self._total_tests_queued += len(tests)
        for t in tests:
            self._test_input_queue.put(t)

    def wait_for_all_queued_tests(self) -> list[results.TestResult]:
        """Waits for all tests that have been queued to finish.

        Returns:
            A list of failed TestResults that were produced since the last time
            this method was called.
        """
        while (self._result_thread.total_results_reported.get()
               != self._total_tests_queued):
            self._result_thread.maybe_reraise_fatal_exception()
            for w in self._workers:
                w.maybe_reraise_fatal_exception()
            time.sleep(_ALL_QUEUED_TESTS_RUN_POLLING_SLEEP_DURATION)

        failed_tests = []
        failed_test_queue = self._result_thread.failed_result_output_queue
        while not failed_test_queue.empty():
            failed_tests.append(failed_test_queue.get())
        return failed_tests

    def shutdown_blocking(self, timeout: float | None = None) -> None:
        """Gracefully shuts down stored threads and waits for them to finish.

        Args:
            timeout: An optional timeout to use when joining underlying
                threads.
        """
        threads_to_shutdown = self._workers + [self._result_thread]
        for t in threads_to_shutdown:
            t.shutdown()
        for t in threads_to_shutdown:
            t.join(timeout)
            if t.is_alive():
                logging.error(
                    'Failed to gracefully shut down thread %s in a WorkerPool',
                    t.native_id)


def _parse_test_log_results(results_json) -> str:
    """Extracts a summary of the test run for displaying

    Args:
        results_json: The decoded JSON from a promptfoo results file.

    Returns:
        A string summarizing the test/eval.
    """
    if results_json is None:
        return ''

    # Display the assertion failures
    results_list = results_json.get('results', {}).get('results')
    if not results_list:
        return 'No results found in promptfoo output.'
    run_result = results_list[0]
    assert_results = []
    grading_result = run_result.get('gradingResult')
    if grading_result:
        for componentResult in grading_result.get('componentResults', []):
            assert_results.append(f"pass: {componentResult['pass']}\n"
                                  f"reason: {componentResult['reason']}\n"
                                  f"score: {componentResult['score']}\n\n")
    response = run_result.get('response', {})
    return (f"Input prompt: {response.get('metrics', {}).get('user_prompt')}\n"
            f"Response: {response.get('metrics', {}).get('full_output')}\n"
            "Assertion results:\n" + ''.join(assert_results))


def _extract_metrics_from_promptfoo_results(
        results_json: dict) -> dict[str, dict | float]:
    """Extracts relevant metrics from promptfoo results.

    Args:
        results_json: The decoded JSON from a promptfoo results file.

    Returns:
        A potentially empty dict of extracted metrics.
    """
    if not results_json:
        return {}

    extracted_metrics = {
        'token_usage':
        _extract_token_usage_from_promptfoo_results(results_json),
    }
    score = _extract_score_from_promptfoo_results(results_json)
    if score is not None:
        extracted_metrics['score'] = score
    return extracted_metrics


def _parse_input_prompt(results_json: dict[str, any]) -> str | None:
    prompts_json = results_json.get('results', {}).get('prompts')
    if not prompts_json:
        logging.error('Did not find the prompts in the test result.')
        return None
    prompt = prompts_json[0].get('raw')
    if not prompt:
        logging.error('Did not find prompt in the test result.')
    return prompt


def _parse_output(results_json: dict[str, any]) -> str | None:
    results_json = results_json.get('results', {}).get('results')
    if not results_json:
        logging.error('Did not find the response in the test result.')
        return None
    output = results_json[0].get('response', {}).get('output')
    if not output:
        logging.error('Did not find output response in the test result.')
    return output


def _load_promptfoo_results(results_file: pathlib.Path) -> dict[str, any]:
    """Loads a promptfoo results file into memory.

    Args:
        results_file: A path to a file containing promptfoo results for a test.

    Returns:
        The decoded JSON content of |results_file|. If loading fails for any
        reason, this will be an empty dict.
    """
    with open(results_file, encoding='utf-8') as infile:
        try:
            return json.load(infile)
        except (json.JSONDecodeError, UnicodeDecodeError) as e:
            logging.error(
                'Error when parsing promptfoo results. This is expected if '
                'promptfoo failed catastrophically: %s', e)
            return {}


def _extract_token_usage_from_promptfoo_results(
        results_json: dict[str, any]) -> dict[str, int]:
    """Extracts gemini-cli token usage from promptfoo JSON results.

    Args:
        results_json: The decoded JSON from a promptfoo results file.

    Returns:
        A dict mapping gemini-cli token type to tokens used at the end of the
        test.
    """
    token_usage = {}
    results_list = results_json.get('results', {}).get('results', [])
    if not results_list:
        logging.error(
            'Did not find promptfoo result information, cannot extract token '
            'usage. This is not expected to ever happen.')
        return token_usage

    if len(results_list) > 1:
        logging.warning(
            'Unexpectedly got %d results from promptfoo when 1 is expected. '
            'Only using the first result for token usage.', len(results_list))
    r = results_list[0]
    token_usage = r.get('response',
                        {}).get('metrics',
                                {}).get(constants.GEMINI_CLI_TOKEN_USAGE, {})
    if not token_usage:
        logging.warning(
            'Did not find gemini-cli token usage. This is not expected to '
            'ever happen')
    return token_usage


def _extract_score_from_promptfoo_results(
        results_json: dict[str, any]) -> float | None:
    """Extracts the test score from promptfoo JSON results.

    Args:
        results_json: The decoded JSON from a promptfoo results file.

    Returns:
        None if the score cannot be extracted. Otherwise, a float representing
        the test score reported by promptfoo. This value is expected to be
        between 0 and 1, inclusive.
    """
    results_list = results_json.get('results', {}).get('results', [])
    if not results_list:
        logging.error(
            'Did not find promptfoo result information, cannot extract score. '
            'This is not expected to ever happen.')
        return None

    if len(results_list) > 1:
        logging.warning(
            'Unexpectedly got %d results from promptfoo when 1 is expected. '
            'Only using the first result for score.', len(results_list))
    r = results_list[0]
    score = r.get('score')
    if score is None:
        logging.warning(
            'Did not find reported score. This is not expected to ever happen')
    return score


class WorkerThread(threading.Thread):
    """Class for running tests from a queue in an isolated environment."""

    def __init__(self, worker_index: int,
                 promptfoo: promptfoo_installation.PromptfooInstallation,
                 worker_options: WorkerOptions,
                 test_input_queue: queue.Queue[eval_config.TestConfig],
                 test_result_queue: queue.Queue[results.TestResult], **kwargs):
        """
        Args:
            worker_index: The unique index of this thread.
            promptfoo: The PromptfooInstallation to use when running tests.
            worker_options: A WorkerOptions instance whose attributes will be
                used when configuring this object.
            test_input_queue: A Queue that will be read from to get tests to
                run.
            test_result_queue: A Queue that will receive TestResults for
                completed tests.
        """
        super().__init__(daemon=True, **kwargs)
        self._worker_index = worker_index
        self._promptfoo = promptfoo
        self._worker_options = worker_options
        self._console_width = shutil.get_terminal_size().columns

        self._test_input_queue = test_input_queue
        self._test_result_queue = test_result_queue
        self._shutdown_event = threading.Event()
        self._fatal_exception = None

    def run(self) -> None:
        try:
            self._run_incoming_tests_until_shutdown()
        except Exception as e:
            self._fatal_exception = e

    def _run_incoming_tests_until_shutdown(self) -> None:
        while not self._shutdown_event.is_set():
            try:
                config = self._test_input_queue.get(
                    timeout=_AVAILABLE_TEST_POLLING_SLEEP_DURATION)
            except queue.Empty:
                continue
            self._run_one_config(config)

    def _run_one_config(self, config: eval_config.TestConfig) -> None:
        """Runs a single test config and queues a TestResult.

        Args:
            config: The TestConfig object for the test to run.
        """
        successful_runs = 0
        all_iteration_results = []

        for i in range(config.runs_per_test):
            logging.info('Running test %s (iteration %d of up to %d)',
                         config.test_file, i + 1, config.runs_per_test)
            iteration_result = self._run_single_iteration(config)
            all_iteration_results.append(iteration_result)

            if iteration_result.success:
                successful_runs += 1

            # Exit early if the test has already passed.
            if successful_runs >= config.pass_k_threshold:
                break

            # Exit early if the test can no longer pass.
            num_failures = (i + 1) - successful_runs
            max_failures_allowed = (config.runs_per_test -
                                    config.pass_k_threshold)
            if num_failures > max_failures_allowed:
                break

        success = successful_runs >= config.pass_k_threshold
        r = results.TestResult(config=config,
                               success=success,
                               iteration_results=all_iteration_results)
        self._test_result_queue.put(r)

    def _run_single_iteration(
            self, config: eval_config.TestConfig) -> results.IterationResult:
        """Runs a single iteration of a test and returns an IterationResult.

        Args:
            config: The TestConfig object for the test to run.
        """
        with (
                WorkDir(
                    f'workdir-{self._worker_index}',
                    checkout_helpers.get_gclient_root(),
                    self._worker_options.clean,
                    self._worker_options.verbose,
                    self._worker_options.force,
                ) as workdir,
                tempfile.TemporaryDirectory() as home_dir,
                tempfile_ext.mkstemp_closed(suffix='.json') as
                promptfoo_output,
        ):
            command = [
                'eval',
                '-j',
                '1',
                '--no-cache',
                # Not useful since we're running one test per eval and the
                # tables don't render properly in captured logs.
                '--no-table',
                '-c',
                str(config.test_file),
                '--var',
                f'console_width={self._console_width}',
                '--var',
                f'home_dir={home_dir}',
                '--output',
                str(promptfoo_output),
            ]
            if self._worker_options.sandbox:
                command.extend(['--var', 'sandbox=True'])
            if self._worker_options.verbose:
                command.extend(['--var', 'verbose=True'])
            if self._worker_options.gemini_cli_bin:
                command.extend([
                    '--var',
                    f'gemini_cli_bin={self._worker_options.gemini_cli_bin}'
                ])
            if self._worker_options.node_bin:
                command.extend(
                    ['--var', f'node_bin={self._worker_options.node_bin}'])

            start_time = time.time()
            proc = self._promptfoo.run(command, cwd=workdir.path / 'src')
            duration = time.time() - start_time

            results_json = _load_promptfoo_results(promptfoo_output)
            metrics = _extract_metrics_from_promptfoo_results(results_json)
            test_log = _parse_test_log_results(results_json)
            prompt = _parse_input_prompt(results_json)
            response = _parse_output(results_json)
            if proc.returncode != 0 and proc.stdout:
                test_log += f'\npromptfoo stdout:\n{proc.stdout}'
            return results.IterationResult(
                success=not proc.returncode,
                duration=duration,
                test_log=test_log,
                metrics=metrics,
                prompt=prompt,
                response=response,
            )

    def shutdown(self) -> None:
        """Tells the thread to shut down gracefully."""
        self._shutdown_event.set()

    def maybe_reraise_fatal_exception(self) -> None:
        """Reraises the fatal exception that caused the thread to die.

        No-op if no exception is stored.
        """
        if self._fatal_exception:
            raise self._fatal_exception
