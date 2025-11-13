#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for the results module."""

import pathlib
import threading
import time
import unittest
from unittest import mock

import eval_config
import results

# Polling interval for threads in nanoseconds.
_POLLING_INTERVAL = 100


# pylint: disable=protected-access


class TestResultTest(unittest.TestCase):

    def test_lt_less_than(self):
        result1 = results.TestResult(
            config=eval_config.TestConfig(test_file=pathlib.Path('a')),
            success=True,
            iteration_results=[])
        result2 = results.TestResult(
            config=eval_config.TestConfig(test_file=pathlib.Path('b')),
            success=True,
            iteration_results=[])
        self.assertLess(result1, result2)

    def test_lt_greater_than(self):
        result1 = results.TestResult(
            config=eval_config.TestConfig(test_file=pathlib.Path('b')),
            success=True,
            iteration_results=[])
        result2 = results.TestResult(
            config=eval_config.TestConfig(test_file=pathlib.Path('a')),
            success=True,
            iteration_results=[])
        self.assertGreater(result1, result2)

    def test_lt_equal(self):
        result1 = results.TestResult(
            config=eval_config.TestConfig(test_file=pathlib.Path('a')),
            success=True,
            iteration_results=[
                results.IterationResult(
                    success=True,
                    duration=1,
                    test_log='log',
                    metrics={},
                    prompt=None,
                    response=None,
                )
            ])
        result2 = results.TestResult(
            config=eval_config.TestConfig(test_file=pathlib.Path('a')),
            success=False,
            iteration_results=[
                results.IterationResult(
                    success=False,
                    duration=2,
                    test_log='log2',
                    metrics={},
                    prompt=None,
                    response=None,
                )
            ])
        self.assertFalse(result1 < result2)
        self.assertFalse(result2 < result1)

    def test_sort(self):
        result_b = results.TestResult(
            config=eval_config.TestConfig(test_file=pathlib.Path('b')),
            success=True,
            iteration_results=[])
        result_a = results.TestResult(
            config=eval_config.TestConfig(test_file=pathlib.Path('a')),
            success=True,
            iteration_results=[])
        result_c = results.TestResult(
            config=eval_config.TestConfig(test_file=pathlib.Path('c')),
            success=True,
            iteration_results=[])
        result_list = [result_b, result_c, result_a]
        self.assertEqual(sorted(result_list), [result_a, result_b, result_c])

    def test_combined_logs(self):
        result = results.TestResult(
            config=eval_config.TestConfig(test_file=pathlib.Path('a')),
            success=True,
            iteration_results=[
                results.IterationResult(
                    success=True,
                    duration=1,
                    test_log='log1',
                    metrics={},
                    prompt=None,
                    response=None,
                ),
                results.IterationResult(
                    success=True,
                    duration=1,
                    test_log='log2',
                    metrics={},
                    prompt=None,
                    response=None,
                )
            ])
        self.assertEqual(result.combined_logs,
                         'Iteration #0:\nlog1\nIteration #1:\nlog2')

    def test_total_duration(self):
        result = results.TestResult(
            config=eval_config.TestConfig(test_file=pathlib.Path('a')),
            success=True,
            iteration_results=[
                results.IterationResult(
                    success=True,
                    duration=1.2,
                    test_log='',
                    metrics={},
                    prompt=None,
                    response=None,
                ),
                results.IterationResult(
                    success=True,
                    duration=3.4,
                    test_log='',
                    metrics={},
                    prompt=None,
                    response=None,
                )
            ])
        self.assertAlmostEqual(result.total_duration, 4.6)

    def test_average_duration(self):
        result = results.TestResult(
            config=eval_config.TestConfig(test_file=pathlib.Path('a')),
            success=True,
            iteration_results=[
                results.IterationResult(
                    success=True,
                    duration=1.0,
                    test_log='',
                    metrics={},
                    prompt=None,
                    response=None,
                ),
                results.IterationResult(
                    success=True,
                    duration=3.0,
                    test_log='',
                    metrics={},
                    prompt=None,
                    response=None,
                )
            ])
        self.assertAlmostEqual(result.average_duration, 2.0)

    def test_successful_runs(self):
        result = results.TestResult(
            config=eval_config.TestConfig(test_file=pathlib.Path('a')),
            success=True,
            iteration_results=[
                results.IterationResult(
                    success=True,
                    duration=1,
                    test_log='',
                    metrics={},
                    prompt=None,
                    response=None,
                ),
                results.IterationResult(
                    success=False,
                    duration=1,
                    test_log='',
                    metrics={},
                    prompt=None,
                    response=None,
                ),
                results.IterationResult(
                    success=True,
                    duration=1,
                    test_log='',
                    metrics={},
                    prompt=None,
                    response=None,
                )
            ])
        self.assertEqual(result.successful_runs, 2)





class AtomicCounterTest(unittest.TestCase):

    def test_initial_value(self):
        counter = results.AtomicCounter()
        self.assertEqual(counter.get(), 0)

    def test_increment(self):
        counter = results.AtomicCounter()
        counter.increment()
        self.assertEqual(counter.get(), 1)
        counter.increment()
        self.assertEqual(counter.get(), 2)

    def test_thread_safety(self):
        counter = results.AtomicCounter()
        num_threads = 10
        increments_per_thread = 100

        def worker():
            for _ in range(increments_per_thread):
                counter.increment()

        threads = []
        for _ in range(num_threads):
            thread = threading.Thread(target=worker)
            threads.append(thread)
            thread.start()

        for thread in threads:
            thread.join()

        self.assertEqual(counter.get(), num_threads * increments_per_thread)


class ResultThreadTest(unittest.TestCase):

    def setUp(self):
        self._setUpPatches()

        self.result_options = results.ResultOptions(
            print_output_on_success=False,
            result_handlers=[],
        )

    def _setUpPatches(self):
        """Set up patches for tests."""
        self.polling_mock = mock.patch(
            'results._RESULT_THREAD_POLLING_SLEEP_DURATION', 0.001)
        self.polling_mock.start()
        self.addCleanup(self.polling_mock.stop)

        stdout_patcher = mock.patch('sys.stdout')
        self.mock_stdout = stdout_patcher.start()
        self.addCleanup(stdout_patcher.stop)



    def _create_result_thread(self):
        return results.ResultThread(result_options=self.result_options)

    def _run_test_with_results(self, results_to_send):
        """Helper to run a test with a list of results."""
        thread = self._create_result_thread()
        thread.start()

        for r in results_to_send:
            thread.result_input_queue.put(r)

        # Wait for all results to be processed.
        while thread.total_results_reported.get() < len(results_to_send):
            thread.maybe_reraise_fatal_exception()
            time.sleep(_POLLING_INTERVAL / 1e9)

        thread.shutdown()
        thread.join(1)
        return thread

    def test_result_handlers_called(self):
        handler_mock = mock.Mock()
        self.result_options.result_handlers = [handler_mock]

        test_result = results.TestResult(
            config=eval_config.TestConfig(test_file=pathlib.Path('test.yaml')),
            success=True,
            iteration_results=[
                results.IterationResult(
                    success=True,
                    duration=1.0,
                    test_log='log',
                    metrics={},
                    prompt=None,
                    response=None,
                )
            ])
        _ = self._run_test_with_results([test_result])

        handler_mock.assert_called_once_with(test_result)


    def test_passed_result(self):
        test_result = results.TestResult(
            config=eval_config.TestConfig(test_file=pathlib.Path('test.yaml')),
            success=True,
            iteration_results=[
                results.IterationResult(
                    success=True,
                    duration=1.0,
                    test_log='log',
                    metrics={},
                    prompt=None,
                    response=None,
                )
            ])
        thread = self._run_test_with_results([test_result])

        self.assertEqual(thread.total_results_reported.get(), 1)
        self.assertTrue(thread.failed_result_output_queue.empty())

    def test_failed_result(self):
        test_result = results.TestResult(
            config=eval_config.TestConfig(test_file=pathlib.Path('test.yaml')),
            success=False,
            iteration_results=[
                results.IterationResult(
                    success=False,
                    duration=1.0,
                    test_log='log',
                    metrics={},
                    prompt=None,
                    response=None,
                )
            ])
        thread = self._run_test_with_results([test_result])

        self.assertEqual(thread.total_results_reported.get(), 1)
        self.assertEqual(thread.failed_result_output_queue.qsize(), 1)
        self.assertEqual(thread.failed_result_output_queue.get(), test_result)

    def test_multiple_results(self):
        results_to_send = [
            results.TestResult(config=eval_config.TestConfig(
                test_file=pathlib.Path('test1.yaml')),
                               success=True,
                               iteration_results=[
                                   results.IterationResult(
                                       success=True,
                                       duration=1.0,
                                       test_log='log1',
                                       metrics={},
                                       prompt=None,
                                       response=None,
                                   )
                               ]),
            results.TestResult(config=eval_config.TestConfig(
                test_file=pathlib.Path('test2.yaml')),
                               success=False,
                               iteration_results=[
                                   results.IterationResult(
                                       success=False,
                                       duration=2.0,
                                       test_log='log2',
                                       metrics={},
                                       prompt=None,
                                       response=None,
                                   )
                               ]),
            results.TestResult(config=eval_config.TestConfig(
                test_file=pathlib.Path('test3.yaml')),
                               success=True,
                               iteration_results=[
                                   results.IterationResult(
                                       success=True,
                                       duration=3.0,
                                       test_log='log3',
                                       metrics={},
                                       prompt=None,
                                       response=None,
                                   )
                               ]),
        ]
        thread = self._run_test_with_results(results_to_send)

        self.assertEqual(thread.total_results_reported.get(), 3)
        self.assertEqual(thread.failed_result_output_queue.qsize(), 1)
        self.assertEqual(thread.failed_result_output_queue.get(),
                         results_to_send[1])

    def test_shutdown(self):
        thread = self._create_result_thread()
        thread.start()
        self.assertTrue(thread.is_alive())
        thread.shutdown()
        thread.join(1)
        self.assertFalse(thread.is_alive())

    def test_fatal_exception(self):
        thread = self._create_result_thread()
        with mock.patch.object(thread,
                               '_process_incoming_results_until_shutdown',
                               side_effect=ValueError('Test Error')):
            thread.start()
            thread.join(1)

        with self.assertRaisesRegex(ValueError, 'Test Error'):
            thread.maybe_reraise_fatal_exception()

    def test_no_fatal_exception(self):
        thread = self._create_result_thread()
        thread.start()
        # Should be a no-op.
        thread.maybe_reraise_fatal_exception()
        thread.shutdown()
        thread.join(1)

    def test_print_output_on_success_true(self):
        self.result_options.print_output_on_success = True
        test_result = results.TestResult(
            config=eval_config.TestConfig(test_file=pathlib.Path('test.yaml')),
            success=True,
            iteration_results=[
                results.IterationResult(
                    success=True,
                    duration=1.0,
                    test_log='log',
                    metrics={},
                    prompt=None,
                    response=None,
                )
            ])
        self._run_test_with_results([test_result])

        self.mock_stdout.write.assert_called_once_with('log')

    def test_print_output_on_success_false(self):
        self.result_options.print_output_on_success = False
        test_result = results.TestResult(
            config=eval_config.TestConfig(test_file=pathlib.Path('test.yaml')),
            success=True,
            iteration_results=[
                results.IterationResult(
                    success=True,
                    duration=1.0,
                    test_log='log',
                    metrics={},
                    prompt=None,
                    response=None,
                )
            ])
        self._run_test_with_results([test_result])

        self.mock_stdout.write.assert_not_called()

    def test_always_print_output_on_failure(self):
        self.result_options.print_output_on_success = False
        test_result = results.TestResult(
            config=eval_config.TestConfig(test_file=pathlib.Path('test.yaml')),
            success=False,
            iteration_results=[
                results.IterationResult(
                    success=False,
                    duration=1.0,
                    test_log='log',
                    metrics={},
                    prompt=None,
                    response=None,
                )
            ])
        self._run_test_with_results([test_result])

        self.mock_stdout.write.assert_called_once_with('log')




if __name__ == '__main__':
    unittest.main()
