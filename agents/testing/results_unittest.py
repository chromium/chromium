#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for the results module."""

import pathlib
import sys
import threading
import time
import unittest
import unittest.mock

import results

CHROMIUM_SRC = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(CHROMIUM_SRC / 'build' / 'util'))
from lib.results import result_sink
from lib.results import result_types

# Polling interval for threads in nanoseconds.
_POLLING_INTERVAL = 100


class TestResultTest(unittest.TestCase):

    def test_lt_less_than(self):
        result1 = results.TestResult(test_file=pathlib.Path('a'),
                                     success=True,
                                     duration=1,
                                     test_log='',
                                     metrics={})
        result2 = results.TestResult(test_file=pathlib.Path('b'),
                                     success=True,
                                     duration=1,
                                     test_log='',
                                     metrics={})
        self.assertLess(result1, result2)

    def test_lt_greater_than(self):
        result1 = results.TestResult(test_file=pathlib.Path('b'),
                                     success=True,
                                     duration=1,
                                     test_log='',
                                     metrics={})
        result2 = results.TestResult(test_file=pathlib.Path('a'),
                                     success=True,
                                     duration=1,
                                     test_log='',
                                     metrics={})
        self.assertGreater(result1, result2)

    def test_lt_equal(self):
        result1 = results.TestResult(test_file=pathlib.Path('a'),
                                     success=True,
                                     duration=1,
                                     test_log='',
                                     metrics={})
        result2 = results.TestResult(test_file=pathlib.Path('a'),
                                     success=False,
                                     duration=2,
                                     test_log='log',
                                     metrics={})
        self.assertFalse(result1 < result2)
        self.assertFalse(result2 < result1)

    def test_sort(self):
        result_b = results.TestResult(test_file=pathlib.Path('b'),
                                      success=True,
                                      duration=1,
                                      test_log='',
                                      metrics={})
        result_a = results.TestResult(test_file=pathlib.Path('a'),
                                      success=True,
                                      duration=1,
                                      test_log='',
                                      metrics={})
        result_c = results.TestResult(test_file=pathlib.Path('c'),
                                      success=True,
                                      duration=1,
                                      test_log='',
                                      metrics={})
        result_list = [result_b, result_c, result_a]
        self.assertEqual(sorted(result_list), [result_a, result_b, result_c])


class ReportResultTest(unittest.TestCase):

    def test_report_result(self):
        mock_client = unittest.mock.Mock(spec=result_sink.ResultSinkClient)
        test_result = results.TestResult(
            test_file=CHROMIUM_SRC / 'some_test.yaml',
            success=True,
            duration=1.23,
            test_log='log',
            metrics={},
        )
        results.report_result(mock_client, test_result)
        mock_client.Post.assert_called_once_with(
            test_id='some_test.yaml',
            status=result_types.PASS,
            duration=1230,
            test_log='log',
            test_file='//some_test.yaml',
            test_id_structured={
                'coarseName': '',
                'fineName': '',
                'caseNameComponents': ['some_test.yaml']
            },
        )

    def test_report_result_failure(self):
        mock_client = unittest.mock.Mock(spec=result_sink.ResultSinkClient)
        test_result = results.TestResult(
            test_file=CHROMIUM_SRC / 'some_test.yaml',
            success=False,
            duration=1.23,
            test_log='log',
            metrics={},
        )
        results.report_result(mock_client, test_result)
        mock_client.Post.assert_called_once_with(
            test_id='some_test.yaml',
            status=result_types.FAIL,
            duration=1230,
            test_log='log',
            test_file='//some_test.yaml',
            test_id_structured={
                'coarseName': '',
                'fineName': '',
                'caseNameComponents': ['some_test.yaml']
            },
        )


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

        self.print_output_on_success = False

    def _setUpPatches(self):
        """Set up patches for tests."""
        self.polling_mock = unittest.mock.patch(
            'results._RESULT_THREAD_POLLING_SLEEP_DURATION', 0.001)
        self.polling_mock.start()
        self.addCleanup(self.polling_mock.stop)

        stdout_patcher = unittest.mock.patch('sys.stdout')
        self.mock_stdout = stdout_patcher.start()
        self.addCleanup(stdout_patcher.stop)

        try_init_client_patcher = unittest.mock.patch(
            'results.result_sink.TryInitClient')
        self.mock_try_init_client = try_init_client_patcher.start()
        self.addCleanup(try_init_client_patcher.stop)

        report_result_patcher = unittest.mock.patch('results.report_result')
        self.mock_report_result = report_result_patcher.start()
        self.addCleanup(report_result_patcher.stop)

    def _create_result_thread(self):
        return results.ResultThread(
            print_output_on_success=self.print_output_on_success,
        )

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

    def test_successful_result(self):
        test_result = results.TestResult(test_file='test.yaml',
                                         success=True,
                                         duration=1.0,
                                         test_log='log',
                                         metrics={})
        thread = self._run_test_with_results([test_result])

        self.assertEqual(thread.total_results_reported.get(), 1)
        self.assertTrue(thread.failed_result_output_queue.empty())

    def test_failed_result(self):
        test_result = results.TestResult(test_file='test.yaml',
                                         success=False,
                                         duration=1.0,
                                         test_log='log',
                                         metrics={})
        thread = self._run_test_with_results([test_result])

        self.assertEqual(thread.total_results_reported.get(), 1)
        self.assertEqual(thread.failed_result_output_queue.qsize(), 1)
        self.assertEqual(thread.failed_result_output_queue.get(), test_result)

    def test_multiple_results(self):
        results_to_send = [
            results.TestResult(test_file='test1.yaml',
                               success=True,
                               duration=1.0,
                               test_log='log1',
                               metrics={}),
            results.TestResult(test_file='test2.yaml',
                               success=False,
                               duration=2.0,
                               test_log='log2',
                               metrics={}),
            results.TestResult(test_file='test3.yaml',
                               success=True,
                               duration=3.0,
                               test_log='log3',
                               metrics={}),
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
        with unittest.mock.patch.object(
                thread,
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
        self.print_output_on_success = True
        test_result = results.TestResult(test_file='test.yaml',
                                         success=True,
                                         duration=1.0,
                                         test_log='log',
                                         metrics={})
        self._run_test_with_results([test_result])

        self.mock_stdout.write.assert_called_once_with('log')

    def test_print_output_on_success_false(self):
        self.print_output_on_success = False
        test_result = results.TestResult(test_file='test.yaml',
                                         success=True,
                                         duration=1.0,
                                         test_log='log',
                                         metrics={})
        self._run_test_with_results([test_result])

        self.mock_stdout.write.assert_not_called()

    def test_always_print_output_on_failure(self):
        self.print_output_on_success = False
        test_result = results.TestResult(test_file='test.yaml',
                                         success=False,
                                         duration=1.0,
                                         test_log='log',
                                         metrics={})
        self._run_test_with_results([test_result])

        self.mock_stdout.write.assert_called_once_with('log')

    def test_result_sink_client_none(self):
        self.mock_try_init_client.return_value = None
        test_result = results.TestResult(test_file='test.yaml',
                                         success=True,
                                         duration=1.0,
                                         test_log='log',
                                         metrics={})
        self._run_test_with_results([test_result])

        self.mock_report_result.assert_not_called()

    def test_result_sink_client_valid(self):
        mock_client = unittest.mock.Mock()
        self.mock_try_init_client.return_value = mock_client
        test_result = results.TestResult(test_file='test.yaml',
                                         success=True,
                                         duration=1.0,
                                         test_log='log',
                                         metrics={})
        self._run_test_with_results([test_result])

        self.mock_report_result.assert_called_once_with(
            mock_client, test_result)


if __name__ == '__main__':
    unittest.main()
