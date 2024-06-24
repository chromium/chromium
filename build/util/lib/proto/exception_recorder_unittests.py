#!/usr/bin/env vpython3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing exception_recorder.py."""

import os
import sys
import tempfile
import unittest
import unittest.mock

_BUILD_UTIL_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..'))
if _BUILD_UTIL_PATH not in sys.path:
  sys.path.insert(0, _BUILD_UTIL_PATH)

from lib.proto import exception_recorder
from lib.proto.exception_occurrences_pb2 import ExceptionOccurrences


class MyClass:

  class MyException(Exception):
    pass


class ExceptionRecorderTest(unittest.TestCase):

  def myException(self):
    raise MyClass.MyException('Hello')

  def test_register(self) -> None:
    exception_recorder.clear()
    with self.assertRaises(MyClass.MyException) as cm:
      self.myException()
    record = exception_recorder.register(cm.exception)
    self.assertEqual(record.name, 'MyClass.MyException')
    self.assertTrue(len(record.stacktrace) > 0)
    self.assertTrue(record.occurred_time.ToSeconds() > 0)
    self.assertEqual(len(exception_recorder._records), 1)

  def test_to_dict(self) -> None:
    exception_recorder.clear()
    with self.assertRaises(MyClass.MyException) as cm:
      self.myException()
    record = exception_recorder.register(cm.exception)
    actual = exception_recorder.to_dict()
    self.assertIn('@type', actual)
    self.assertIn(ExceptionOccurrences.DESCRIPTOR.full_name, actual['@type'])
    actual_record = actual['datapoints'][0]
    self.assertEqual(actual_record['name'], record.name)
    self.assertEqual(actual_record['stacktrace'], record.stacktrace)

  def test_dump(self) -> None:
    exception_recorder.clear()
    with self.assertRaises(MyClass.MyException) as cm:
      self.myException()
    record = exception_recorder.register(cm.exception)
    with tempfile.TemporaryDirectory() as tmpdir:
      exception_recorder.dump(tmpdir)
      file_path = os.path.join(
          tmpdir, exception_recorder.EXCEPTION_OCCURRENCES_FILENAME)
      self.assertTrue(os.path.exists(file_path))


if __name__ == '__main__':
  unittest.main()
