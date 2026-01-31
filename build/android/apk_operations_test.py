#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from unittest import mock
import sys
import io

import apk_operations


# Testing internal members of apk_operations.
# pylint: disable=protected-access
class LogcatProcessorTest(unittest.TestCase):

  def setUp(self):
    self.device = mock.Mock()
    self.package_name = 'com.example.app'

    # Mock _GetPackageProcesses to avoid device interaction
    self.get_package_processes_patcher = mock.patch(
        'apk_operations._GetPackageProcesses', return_value=[])
    self.get_package_processes_patcher.start()

  def tearDown(self):
    self.get_package_processes_patcher.stop()

  def create_processor(self, log_level='V', filter_regex=None):
    return apk_operations._LogcatProcessor(self.device,
                                           self.package_name,
                                           stack_script_context=None,
                                           log_level=log_level,
                                           filter_regex=filter_regex)

  def testFilterLogLevel(self):
    processor = self.create_processor(log_level='W')

    # Capture stdout
    captured_output = io.StringIO()
    sys.stdout = captured_output

    try:
      # Verbose log - should be filtered out
      line = '01-01 00:00:00.000 1000 1000 V Tag : Message'
      parsed = processor._ParseLine(line)
      processor._PrintParsedLine(parsed)
      self.assertEqual(captured_output.getvalue(), '')

      # Debug log - should be filtered out
      line = '01-01 00:00:00.000 1000 1000 D Tag : Message'
      parsed = processor._ParseLine(line)
      processor._PrintParsedLine(parsed)
      self.assertEqual(captured_output.getvalue(), '')

      # Warning log - should be printed
      line = '01-01 00:00:00.000 1000 1000 W Tag : Message'
      parsed = processor._ParseLine(line)
      processor._PrintParsedLine(parsed)
      self.assertNotEqual(captured_output.getvalue(), '')
      # Reset buffer
      captured_output.truncate(0)
      captured_output.seek(0)

      # Error log - should be printed
      line = '01-01 00:00:00.000 1000 1000 E Tag : Message'
      parsed = processor._ParseLine(line)
      processor._PrintParsedLine(parsed)
      self.assertNotEqual(captured_output.getvalue(), '')
    finally:
      sys.stdout = sys.__stdout__

  def testFilterRegex(self):
    processor = self.create_processor(filter_regex='MyTag|Important')

    captured_output = io.StringIO()
    sys.stdout = captured_output

    try:
      # Matching tag - should be printed
      line = '01-01 00:00:00.000 1000 1000 I MyTag : Message'
      parsed = processor._ParseLine(line)
      processor._PrintParsedLine(parsed)
      self.assertNotEqual(captured_output.getvalue(), '')
      captured_output.truncate(0)
      captured_output.seek(0)

      # Matching message - should be printed
      line = '01-01 00:00:00.000 1000 1000 I OtherTag : Important message'
      parsed = processor._ParseLine(line)
      processor._PrintParsedLine(parsed)
      self.assertNotEqual(captured_output.getvalue(), '')
      captured_output.truncate(0)
      captured_output.seek(0)

      # No match - should be filtered out
      line = '01-01 00:00:00.000 1000 1000 I OtherTag : Boring message'
      parsed = processor._ParseLine(line)
      processor._PrintParsedLine(parsed)
      self.assertEqual(captured_output.getvalue(), '')
    finally:
      sys.stdout = sys.__stdout__

  def testFilterBoth(self):
    processor = self.create_processor(log_level='W', filter_regex='MyTag')

    captured_output = io.StringIO()
    sys.stdout = captured_output

    try:
      # Matching tag but low priority - should be filtered
      line = '01-01 00:00:00.000 1000 1000 D MyTag : Message'
      parsed = processor._ParseLine(line)
      processor._PrintParsedLine(parsed)
      self.assertEqual(captured_output.getvalue(), '')

      # High priority but no match - should be filtered
      line = '01-01 00:00:00.000 1000 1000 W OtherTag : Message'
      parsed = processor._ParseLine(line)
      processor._PrintParsedLine(parsed)
      self.assertEqual(captured_output.getvalue(), '')

      # High priority and matching tag - should be printed
      line = '01-01 00:00:00.000 1000 1000 W MyTag : Message'
      parsed = processor._ParseLine(line)
      processor._PrintParsedLine(parsed)
      self.assertNotEqual(captured_output.getvalue(), '')
    finally:
      sys.stdout = sys.__stdout__

  def testRealisticLogFormat(self):
    processor = self.create_processor()

    # "Command line" tag test
    line = '09-29 18:38:30.064     0     0 I Command line: 8250.nr_uarts=1'
    parsed = processor._ParseLine(line)

    self.assertEqual(parsed.tag, 'Command line')
    self.assertEqual(parsed.message, '8250.nr_uarts=1')

  def testRealisticLogFormatWithSpaces(self):
    processor = self.create_processor()
    # Case with empty tag (just whitespace before :)
    line = '09-29 18:38:30.064     0     0 I         : Linux version'
    parsed = processor._ParseLine(line)
    self.assertEqual(parsed.tag, '')
    self.assertEqual(parsed.message, 'Linux version')


if __name__ == '__main__':
  unittest.main()
