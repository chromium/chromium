#!/usr/bin/env vpython3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=protected-access


import os
import unittest

import sys

sys.path.append(
    os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..')))

from pylib.base import base_test_result
from pylib.local.device import local_device_test_run

import mock  # pylint: disable=import-error


class TestLocalDeviceTestRun(local_device_test_run.LocalDeviceTestRun):

  # pylint: disable=abstract-method

  def __init__(self):
    super().__init__(mock.MagicMock(), mock.MagicMock())


class TestLocalDeviceNonStringTestRun(
    local_device_test_run.LocalDeviceTestRun):

  # pylint: disable=abstract-method

  def __init__(self):
    super().__init__(mock.MagicMock(), mock.MagicMock())

  def _GetUniqueTestName(self, test):
    return test['name']


class TestLocalDeviceRetryTestRun(local_device_test_run.LocalDeviceTestRun):

  # pylint: disable=abstract-method

  def __init__(self, tests=None):
    mock_env = mock.MagicMock()
    mock_prop_current_try = mock.PropertyMock(side_effect=[0, 0, 1, 1, 2, 2, 3])
    type(mock_env).current_try = mock_prop_current_try
    mock_prop_max_tries = mock.PropertyMock(return_value=3)
    type(mock_env).max_tries = mock_prop_max_tries
    super().__init__(mock_env, mock.MagicMock())
    self._tests = tests or []

  def _GetTests(self):
    return self._tests

  def _GetTestsToRetry(self, tests, try_results):
    return [test for test in self._tests if test.endswith('fail')]

  def _ShouldShardTestsForDevices(self):
    pass


class LocalDeviceTestRunTest(unittest.TestCase):

  def testSortTests(self):
    test_run = TestLocalDeviceTestRun()
    self.assertEqual(test_run._SortTests(['a', 'b', 'c', 'd', 'e', 'f', 'g']),
                     ['d', 'f', 'c', 'b', 'e', 'a', 'g'])

  def testGetTestsToRetry_allTestsPassed(self):
    results = [
        base_test_result.BaseTestResult(
            'Test1', base_test_result.ResultType.PASS),
        base_test_result.BaseTestResult(
            'Test2', base_test_result.ResultType.PASS),
    ]

    tests = [r.GetName() for r in results]
    try_results = base_test_result.TestRunResults()
    try_results.AddResults(results)

    test_run = TestLocalDeviceTestRun()
    tests_to_retry = test_run._GetTestsToRetry(tests, try_results)
    self.assertEqual(0, len(tests_to_retry))

  def testGetTestsToRetry_testFailed(self):
    results = [
        base_test_result.BaseTestResult(
            'Test1', base_test_result.ResultType.FAIL),
        base_test_result.BaseTestResult(
            'Test2', base_test_result.ResultType.PASS),
    ]

    tests = [r.GetName() for r in results]
    try_results = base_test_result.TestRunResults()
    try_results.AddResults(results)

    test_run = TestLocalDeviceTestRun()
    tests_to_retry = test_run._GetTestsToRetry(tests, try_results)
    self.assertEqual(1, len(tests_to_retry))
    self.assertIn('Test1', tests_to_retry)

  def testGetTestsToRetry_testUnknown(self):
    results = [
        base_test_result.BaseTestResult(
            'Test2', base_test_result.ResultType.PASS),
    ]

    tests = ['Test1'] + [r.GetName() for r in results]
    try_results = base_test_result.TestRunResults()
    try_results.AddResults(results)

    test_run = TestLocalDeviceTestRun()
    tests_to_retry = test_run._GetTestsToRetry(tests, try_results)
    self.assertEqual(1, len(tests_to_retry))
    self.assertIn('Test1', tests_to_retry)

  def testGetTestsToRetry_wildcardFilter_allPass(self):
    results = [
        base_test_result.BaseTestResult(
            'TestCase.Test1', base_test_result.ResultType.PASS),
        base_test_result.BaseTestResult(
            'TestCase.Test2', base_test_result.ResultType.PASS),
    ]

    tests = ['TestCase.*']
    try_results = base_test_result.TestRunResults()
    try_results.AddResults(results)

    test_run = TestLocalDeviceTestRun()
    tests_to_retry = test_run._GetTestsToRetry(tests, try_results)
    self.assertEqual(0, len(tests_to_retry))

  def testGetTestsToRetry_wildcardFilter_oneFails(self):
    results = [
        base_test_result.BaseTestResult(
            'TestCase.Test1', base_test_result.ResultType.PASS),
        base_test_result.BaseTestResult(
            'TestCase.Test2', base_test_result.ResultType.FAIL),
    ]

    tests = ['TestCase.*']
    try_results = base_test_result.TestRunResults()
    try_results.AddResults(results)

    test_run = TestLocalDeviceTestRun()
    tests_to_retry = test_run._GetTestsToRetry(tests, try_results)
    self.assertEqual(1, len(tests_to_retry))
    self.assertIn('TestCase.*', tests_to_retry)

  def testGetTestsToRetry_nonStringTests(self):
    results = [
        base_test_result.BaseTestResult(
            'TestCase.Test1', base_test_result.ResultType.PASS),
        base_test_result.BaseTestResult(
            'TestCase.Test2', base_test_result.ResultType.FAIL),
    ]

    tests = [
        {'name': 'TestCase.Test1'},
        {'name': 'TestCase.Test2'},
    ]
    try_results = base_test_result.TestRunResults()
    try_results.AddResults(results)

    test_run = TestLocalDeviceNonStringTestRun()
    tests_to_retry = test_run._GetTestsToRetry(tests, try_results)
    self.assertEqual(1, len(tests_to_retry))
    self.assertIsInstance(tests_to_retry[0], dict)
    self.assertEqual(tests[1], tests_to_retry[0])

  def testDeviceRecovery_highFailedTests(self):
    total_count = 10
    failed_count = (total_count * local_device_test_run.FAILED_TEST_PCT_MAX //
                    100) + 1
    tests = ['TestCase.Test%d_fail' % i for i in range(1, failed_count + 1)]
    tests += [
        'TestCase.Test%d_pass' % i
        for i in range(failed_count + 1, total_count + 1)
    ]

    test_run = TestLocalDeviceRetryTestRun(tests=tests)
    with mock.patch.object(test_run, '_RecoverDevices') as mock_recover:
      test_run.RunTests(results=[])
      self.assertEqual(mock_recover.call_count, 2)

  def testDeviceRecovery_lowFailedTests(self):
    total_count = 10
    failed_count = (total_count * local_device_test_run.FAILED_TEST_PCT_MAX //
                    100)
    tests = ['TestCase.Test%d_fail' % i for i in range(1, failed_count + 1)]
    tests += [
        'TestCase.Test%d_pass' % i
        for i in range(failed_count + 1, total_count + 1)
    ]

    test_run = TestLocalDeviceRetryTestRun(tests=tests)
    with mock.patch.object(test_run, '_RecoverDevices') as mock_recover:
      test_run.RunTests(results=[])
      # Only called once for the last attempt.
      self.assertEqual(mock_recover.call_count, 1)


if __name__ == '__main__':
  unittest.main(verbosity=2)
