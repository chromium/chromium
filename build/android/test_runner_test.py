#!/usr/bin/env vpython3
#
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from unittest import mock

import test_runner


class UploadTestScriptRecordsTest(unittest.TestCase):

  def setUp(self):
    self.sink_client = mock.MagicMock()
    self.exc_recorder = mock.MagicMock()
    self.mm_recorder = mock.MagicMock()

  def testNoRecords(self):
    self.exc_recorder.size.return_value = 0
    self.mm_recorder.size.return_value = 0
    test_runner.UploadTestScriptRecords(self.sink_client, self.exc_recorder,
                                        self.mm_recorder)
    self.exc_recorder.to_dict.assert_not_called()
    self.mm_recorder.to_dict.assert_not_called()
    self.sink_client.UpdateInvocationExtendedProperties.assert_not_called()

  def testUploadSuccess(self):
    test_runner.UploadTestScriptRecords(self.sink_client, self.exc_recorder,
                                        self.mm_recorder)
    self.exc_recorder.to_dict.assert_called_once()
    self.mm_recorder.to_dict.assert_called_once()
    self.sink_client.UpdateInvocationExtendedProperties.assert_called_once()
    self.exc_recorder.clear.assert_called_once()
    self.mm_recorder.clear.assert_called_once()

  def testUploadSuccessWithClearStacktrace(self):
    self.sink_client.UpdateInvocationExtendedProperties.side_effect = [
        Exception("Error 1"), None
    ]
    test_runner.UploadTestScriptRecords(self.sink_client, self.exc_recorder,
                                        self.mm_recorder)
    self.assertEqual(self.exc_recorder.to_dict.call_count, 2)
    self.assertEqual(self.mm_recorder.to_dict.call_count, 2)
    self.assertEqual(
        self.sink_client.UpdateInvocationExtendedProperties.call_count, 2)
    self.exc_recorder.clear_stacktrace.assert_called_once()
    self.exc_recorder.clear.assert_called_once()
    self.mm_recorder.clear.assert_called_once()

  def testUploadSuccessWithClearRecords(self):
    self.sink_client.UpdateInvocationExtendedProperties.side_effect = [
        Exception("Error 1"), Exception("Error 2"), None
    ]
    test_runner.UploadTestScriptRecords(self.sink_client, self.exc_recorder,
                                        self.mm_recorder)
    self.assertEqual(self.exc_recorder.to_dict.call_count, 3)
    self.assertEqual(self.mm_recorder.to_dict.call_count, 3)
    self.assertEqual(
        self.sink_client.UpdateInvocationExtendedProperties.call_count, 3)
    self.exc_recorder.clear_stacktrace.assert_called_once()
    self.assertEqual(self.exc_recorder.clear.call_count, 2)
    self.exc_recorder.register.assert_called_once()
    self.mm_recorder.clear.assert_called_once()

  def testUploadFailure(self):
    self.sink_client.UpdateInvocationExtendedProperties.side_effect = (
        Exception("Error"))
    test_runner.UploadTestScriptRecords(self.sink_client, self.exc_recorder,
                                        self.mm_recorder)
    self.assertEqual(self.exc_recorder.to_dict.call_count, 3)
    self.assertEqual(self.mm_recorder.to_dict.call_count, 3)
    self.assertEqual(
        self.sink_client.UpdateInvocationExtendedProperties.call_count, 3)
    self.assertEqual(self.exc_recorder.clear.call_count, 2)
    self.exc_recorder.clear_stacktrace.assert_called_once()
    self.exc_recorder.register.assert_called_once()
    self.mm_recorder.clear.assert_called_once()


class TestRunnerHelperTest(unittest.TestCase):

  def testCreateStructuredTestDict(self):
    # pylint: disable=protected-access
    t_instance_mock = mock.MagicMock()
    t_result_mock = mock.MagicMock()
    test_id = 'foo.bar.class#test1[28]'
    t_result_mock.GetNameForResultSink.return_value = test_id
    t_instance_mock.suite = 'foo_suite'

    # junit tests
    t_instance_mock.TestType.return_value = 'junit'
    test_dict = test_runner._CreateStructuredTestDict(t_instance_mock,
                                                      t_result_mock)
    self.assertEqual(test_dict['coarseName'], 'foo.bar')
    self.assertEqual(test_dict['fineName'], 'class')
    self.assertTrue('test1[28]' in test_dict['caseNameComponents'])

    # instrumentation tests
    t_instance_mock.TestType.return_value = 'instrumentation'
    test_dict = test_runner._CreateStructuredTestDict(t_instance_mock,
                                                      t_result_mock)
    self.assertEqual(test_dict['coarseName'], 'foo.bar')
    self.assertEqual(test_dict['fineName'], 'class')
    self.assertTrue('test1[28]' in test_dict['caseNameComponents'])

    # Can't be parsed as an instrumentation test as it's missing the #.
    test_id = 'foo.bar.class.test1[28]'
    t_result_mock.GetNameForResultSink.return_value = test_id
    test_dict = test_runner._CreateStructuredTestDict(t_instance_mock,
                                                      t_result_mock)
    self.assertIsNone(test_dict)

    test_id = 'foo.bar.class$parameter#test1[28]'
    t_result_mock.GetNameForResultSink.return_value = test_id
    test_dict = test_runner._CreateStructuredTestDict(t_instance_mock,
                                                      t_result_mock)
    self.assertEqual(test_dict['coarseName'], 'foo.bar')
    self.assertEqual(test_dict['fineName'], 'class$parameter')
    self.assertTrue('test1[28]' in test_dict['caseNameComponents'])

    # gtest
    t_instance_mock.TestType.return_value = 'gtest'
    test_id = 'foo.bar.class.test1[28]'
    t_result_mock.GetNameForResultSink.return_value = test_id
    test_dict = test_runner._CreateStructuredTestDict(t_instance_mock,
                                                      t_result_mock)
    self.assertIsNone(test_dict['coarseName'])
    self.assertEqual(test_dict['fineName'], 'foo_suite')
    self.assertTrue(test_id in test_dict['caseNameComponents'])
    # pylint: disable=protected-access


if __name__ == '__main__':
  # Suppress logging messages.
  unittest.main(buffer=True)
