#!/usr/bin/env vpython3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=protected-access


import unittest

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


if __name__ == '__main__':
  unittest.main(verbosity=2)
