#!/usr/bin/env vpython3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for local_device_instrumentation_test_run."""

# pylint: disable=protected-access


import unittest

import random
import os
import sys
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__),
    '../../..')))

from pylib.base import base_test_result
from pylib.base import mock_environment
from pylib.base import mock_test_instance
from pylib.local.device import local_device_instrumentation_test_run


class LocalDeviceInstrumentationTestRunTest(unittest.TestCase):

  def setUp(self):
    super().setUp()
    self._env = mock_environment.MockEnvironment()
    self._ti = mock_test_instance.MockTestInstance()
    self._obj = (
        local_device_instrumentation_test_run.LocalDeviceInstrumentationTestRun(
            self._env, self._ti))

  # TODO(crbug.com/41361955): Decide whether the _ShouldRetry hook is worth
  # retaining and remove these tests if not.

  def testShouldRetry_failure(self):
    test = {
        'annotations': {},
        'class': 'SadTest',
        'method': 'testFailure',
    }
    result = base_test_result.BaseTestResult(
        'SadTest.testFailure', base_test_result.ResultType.FAIL)
    self.assertTrue(self._obj._ShouldRetry(test, result))

  def testShouldRetry_retryOnFailure(self):
    test = {
        'annotations': {'RetryOnFailure': None},
        'class': 'SadTest',
        'method': 'testRetryOnFailure',
    }
    result = base_test_result.BaseTestResult(
        'SadTest.testRetryOnFailure', base_test_result.ResultType.FAIL)
    self.assertTrue(self._obj._ShouldRetry(test, result))

  def testShouldRetry_notRun(self):
    test = {
        'annotations': {},
        'class': 'SadTest',
        'method': 'testNotRun',
    }
    result = base_test_result.BaseTestResult(
        'SadTest.testNotRun', base_test_result.ResultType.NOTRUN)
    self.assertTrue(self._obj._ShouldRetry(test, result))

  def testIsWPRRecordReplayTest_matchedWithKey(self):
    test = {
        'annotations': {
            'Feature': {
                'value': ['WPRRecordReplayTest', 'dummy']
            }
        },
        'class': 'WPRDummyTest',
        'method': 'testRun',
    }
    self.assertTrue(
        local_device_instrumentation_test_run._IsWPRRecordReplayTest(test))

  def testIsWPRRecordReplayTest_noMatchedKey(self):
    test = {
        'annotations': {
            'Feature': {
                'value': ['abc', 'dummy']
            }
        },
        'class': 'WPRDummyTest',
        'method': 'testRun',
    }
    self.assertFalse(
        local_device_instrumentation_test_run._IsWPRRecordReplayTest(test))

  def testGetWPRArchivePath_matchedWithKey(self):
    test = {
        'annotations': {
            'WPRArchiveDirectory': {
                'value': 'abc'
            }
        },
        'class': 'WPRDummyTest',
        'method': 'testRun',
    }
    self.assertEqual(
        local_device_instrumentation_test_run._GetWPRArchivePath(test), 'abc')

  def testGetWPRArchivePath_noMatchedWithKey(self):
    test = {
        'annotations': {
            'Feature': {
                'value': 'abc'
            }
        },
        'class': 'WPRDummyTest',
        'method': 'testRun',
    }
    self.assertFalse(
        local_device_instrumentation_test_run._GetWPRArchivePath(test))

  def testIsRenderTest_matchedWithKey(self):
    test = {
        'annotations': {
            'Feature': {
                'value': ['RenderTest', 'dummy']
            }
        },
        'class': 'DummyTest',
        'method': 'testRun',
    }
    self.assertTrue(local_device_instrumentation_test_run._IsRenderTest(test))

  def testIsRenderTest_noMatchedKey(self):
    test = {
        'annotations': {
            'Feature': {
                'value': ['abc', 'dummy']
            }
        },
        'class': 'DummyTest',
        'method': 'testRun',
    }
    self.assertFalse(local_device_instrumentation_test_run._IsRenderTest(test))

  def testReplaceUncommonChars(self):
    original = 'abc#edf'
    self.assertEqual(
        local_device_instrumentation_test_run._ReplaceUncommonChars(original),
        'abc__edf')
    original = 'abc#edf#hhf'
    self.assertEqual(
        local_device_instrumentation_test_run._ReplaceUncommonChars(original),
        'abc__edf__hhf')
    original = 'abcedfhhf'
    self.assertEqual(
        local_device_instrumentation_test_run._ReplaceUncommonChars(original),
        'abcedfhhf')
    original = None
    with self.assertRaises(ValueError):
      local_device_instrumentation_test_run._ReplaceUncommonChars(original)
    original = ''
    with self.assertRaises(ValueError):
      local_device_instrumentation_test_run._ReplaceUncommonChars(original)

  def test_ApplyExternalSharding(self):
    test1_batch1 = create_test(
        {'Batch': {'value': 'batch1'}}, 'com.example.TestA', 'test1')
    test2 = create_test(
        {'Features$EnableFeatures': {'value': 'defg'}},
        'com.example.TestB', 'test2')
    test3 = create_test({}, 'com.example.TestC', 'test3')
    test3_multiprocess = create_test(
        {}, 'com.example.TestC', 'test3__multiprocess_mode')
    test4_batch1 = create_test(
        {'Batch': {'value': 'batch1'}}, 'com.example.TestD', 'test4')
    test5_batch1 = create_test(
        {'Batch': {'value': 'batch1'}}, 'com.example.TestE', 'test5')
    test6 = create_test({}, 'com.example.TestF', 'test6')
    test7 = create_test({}, 'com.example.TestG', 'test7')
    test8 = create_test({}, 'com.example.TestH', 'test8')

    tests = [
        test1_batch1, test2, test3, test3_multiprocess, test4_batch1,
        test5_batch1, test6, test7, test8
    ]
    expected_shard0 = [test8]
    expected_shard1 = [
        [test1_batch1, test4_batch1, test5_batch1],
        test3_multiprocess,
        test7,
        test3,
        test6,
        test2,
    ]
    # Shuffle the tests two times to check if the output is deterministic.
    random.shuffle(tests)
    self.assertListEqual(self._obj._ApplyExternalSharding(tests, 0, 2),
                         expected_shard0)
    self.assertListEqual(self._obj._ApplyExternalSharding(tests, 1, 2),
                         expected_shard1)
    random.shuffle(tests)
    self.assertListEqual(self._obj._ApplyExternalSharding(tests, 0, 2),
                         expected_shard0)
    self.assertListEqual(self._obj._ApplyExternalSharding(tests, 1, 2),
                         expected_shard1)

  def test_GetTestsToRetry(self):
    test1_batch1 = create_test(
        {'Batch': {'value': 'batch1'}}, 'com.example.TestA', 'test1')
    test2_batch1 = create_test(
        {'Batch': {'value': 'batch1'}}, 'com.example.TestB', 'test2')
    test3 = create_test({}, 'com.example.TestC', 'test3')
    test4 = create_test({}, 'com.example.TestD', 'test4')
    test_data = [
        (test1_batch1, base_test_result.ResultType.PASS),
        (test2_batch1, base_test_result.ResultType.FAIL),
        (test3, base_test_result.ResultType.PASS),
        (test4, base_test_result.ResultType.FAIL),
    ]
    all_tests = [[test1_batch1, test2_batch1], test3, test4]
    try_results = base_test_result.TestRunResults()
    for test, test_result in test_data:
      try_results.AddResult(
          base_test_result.BaseTestResult(self._obj._GetUniqueTestName(test),
                                          test_result))
    actual_retry = self._obj._GetTestsToRetry(all_tests, try_results)
    expected_retry = [
        [test2_batch1],
        test4,
    ]
    self.assertListEqual(actual_retry, expected_retry)


def create_test(annotation_dict, class_name, method_name):
  # Helper function to generate test dict
  test = {
        'annotations': annotation_dict,
        'class': class_name,
        'method': method_name
  }
  return test


if __name__ == '__main__':
  unittest.main(verbosity=2)
