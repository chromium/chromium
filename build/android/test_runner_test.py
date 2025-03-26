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
