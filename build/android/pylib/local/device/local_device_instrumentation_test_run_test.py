#!/usr/bin/env vpython3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for local_device_instrumentation_test_run."""

# pylint: disable=protected-access


import unittest

import os
import sys
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__),
    '../../..')))

from pylib.base import base_test_result
from pylib.base import mock_environment
from pylib.base import mock_test_instance
from pylib.local.device import local_device_instrumentation_test_run

import mock  # pylint: disable=import-error


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


class LocalDeviceInstrumentationTestRunShardingTest(unittest.TestCase):
  def setUp(self):
    super().setUp()
    self._env = mock_environment.MockEnvironment()
    self._ti = mock_test_instance.MockTestInstance()
    self._obj = (
        local_device_instrumentation_test_run.LocalDeviceInstrumentationTestRun(
            self._env, self._ti))

    # Create test data
    self.test1_batch1 = create_test({'Batch': {'value': 'batch1'}},
        'com.example.TestA', 'test1')
    self.test2 = create_test(
        {'Features$EnableFeatures': {'value': 'defg'}},
        'com.example.TestB', 'test2')
    self.test3 = create_test({}, 'com.example.TestC', 'test3')
    self.test3_multiprocess = create_test({}, 'com.example.TestC',
        'test3__multiprocess_mode')
    self.test4_batch1 = create_test({'Batch': {'value': 'batch1'}},
        'com.example.TestD', 'test4')
    self.test5_batch1 = create_test({'Batch': {'value': 'batch1'}},
        'com.example.TestE', 'test5')
    self.test6 = create_test({}, 'com.example.TestF', 'test6')
    self.test7 = create_test({}, 'com.example.TestG', 'test7')
    self.test8 = create_test({}, 'com.example.TestH', 'test8')

    self.test_list = [
        self.test1_batch1, self.test2, self.test3, self.test3_multiprocess,
        self.test4_batch1, self.test5_batch1, self.test6, self.test7,
        self.test8
    ]

  def test_GroupTestsIntoBatchesAndOthers(self):
    expected_batched_tests = {
        'batch1': [self.test1_batch1, self.test4_batch1, self.test5_batch1]
    }
    expected_other_tests = [
        self.test2, self.test3, self.test3_multiprocess, self.test6,
        self.test7, self.test8
    ]
    batched_tests, other_tests = self._obj._GroupTestsIntoBatchesAndOthers(
        self.test_list)
    self.assertEqual(batched_tests, expected_batched_tests)
    self.assertEqual(other_tests, expected_other_tests)

  @mock.patch(('pylib.local.device.local_device_instrumentation_test_run.'
      '_NON_UNIT_TEST_MAX_GROUP_SIZE'), 2)
  def test_GroupTests(self):
    expected_tests = [
        [self.test1_batch1, self.test4_batch1],
        self.test3_multiprocess, self.test8, self.test2, self.test7,
        self.test6,
        [self.test5_batch1],
        self.test3
    ]
    actual_tests = self._obj._GroupTests(self.test_list)
    self.assertEqual(actual_tests, expected_tests)

  def test_GroupTestsAfterSharding(self):
    expected_tests = [
        [self.test1_batch1, self.test4_batch1, self.test5_batch1],
        self.test3_multiprocess, self.test8, self.test2, self.test7,
        self.test6, self.test3
    ]
    actual_tests = self._obj._GroupTestsAfterSharding(self.test_list)
    self.assertEqual(actual_tests, expected_tests)

  def test_ApplyExternalSharding_1_shard(self):
      expected_tests = [
          self.test1_batch1, self.test4_batch1, self.test5_batch1,
          self.test3_multiprocess, self.test8, self.test2, self.test7,
          self.test6, self.test3
      ]
      actual_tests = self._obj._ApplyExternalSharding(
          self.test_list, 0, 1)
      self.assertEqual(actual_tests, expected_tests)

  def test_ApplyExternalSharding_2_shards(self):
    expected_shard0 = [
        self.test1_batch1, self.test4_batch1, self.test5_batch1,
        self.test3_multiprocess
    ]
    expected_shard1 = [
        self.test8, self.test2, self.test7, self.test6, self.test3
    ]
    expected_union = str(sorted(self.test_list, key=lambda d: d['method']))

    actual_shard0 = self._obj._ApplyExternalSharding(
        self.test_list, 0, 2)
    actual_shard1 = self._obj._ApplyExternalSharding(
        self.test_list, 1, 2)
    # Serialize the sorted list of test dicts for comparison
    actual_union = str(sorted(actual_shard0 + actual_shard1,
        key=lambda d: d['method']))
    self.assertEqual(actual_shard0, expected_shard0)
    self.assertEqual(actual_shard1, expected_shard1)
    self.assertEqual(actual_union, expected_union)

  def test_CreateShardsForDevices(self):
    actual_tests = self._obj._CreateShardsForDevices(self.test_list)
    self.assertEqual(actual_tests, self.test_list)

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
