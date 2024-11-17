#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import sys
import unittest

import PRESUBMIT

# Switch to src/ directory to import test mocks.
file_path = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(file_path, '..', '..', '..'))
from PRESUBMIT_test_mocks import (MockInputApi, MockOutputApi, MockAffectedFile,
    MockFile)

_CHROME_BROWSER_ASH = os.path.join('chrome', 'browser', 'ash')

class DepsFileProhibitingChromeExistsFileTest(unittest.TestCase):
  def assertDepsFilesWithErrors(self, input_api, expected_missing_deps_files,
      expected_deps_files_missing_chrome):

    results = PRESUBMIT._CheckDepsFileProhibitingChromeExists(
        input_api, MockOutputApi())
    self.assertTrue(len(results) <= 2)

    missing_deps_file_pattern = input_api.re.compile(
        r'.*require a DEPS file.*\[(.*)\].*')
    missing_chrome_rule_pattern = input_api.re.compile(
        r'.*must prohibit new \/\/chrome dependencies.*\[(.*)\].*')

    missing_deps_files = []
    deps_files_missing_chrome = []

    for r in results:
      regex_results = missing_deps_file_pattern.search(r.message)
      if regex_results:
        missing_deps_files = regex_results.group(1).split(', ')

      regex_results = missing_chrome_rule_pattern.search(r.message)
      if missing_chrome_rule_pattern.search(r.message):
        deps_files_missing_chrome = regex_results.group(1).split(', ')

    self.assertEqual(set(missing_deps_files), set(expected_missing_deps_files))
    self.assertEqual(set(deps_files_missing_chrome),
                     set(expected_deps_files_missing_chrome))

  # No files created; no errors expected.
  def testNoFiles(self):
    input_api = MockInputApi()
    input_api.InitFiles([])
    self.assertDepsFilesWithErrors(input_api, [], [])

  # Create files in new subdirectories without DEPS files.
  def testFileInDirectoryWithNoDeps(self):
    input_api = MockInputApi()
    input_api.InitFiles([
        MockAffectedFile(
            os.path.join(_CHROME_BROWSER_ASH, 'foo', 'bar.h'), ''),
        MockAffectedFile(
            os.path.join(_CHROME_BROWSER_ASH, 'bar', 'baz.h'), ''),
    ])
    self.assertDepsFilesWithErrors(
        input_api,
        [
          os.path.join(_CHROME_BROWSER_ASH, 'foo', 'DEPS'),
          os.path.join(_CHROME_BROWSER_ASH, 'bar', 'DEPS'),
        ], [])

  # Create files in a new subdirectories with DEPS files not prohibiting
  # //chrome.
  def testFileInDirectoryNotProhibitingChrome(self):
    DEPS_FILE_NOT_PROHIBITING_CHROME = ['include_rules = []']

    input_api = MockInputApi()
    input_api.InitFiles([
        MockAffectedFile(
            os.path.join(_CHROME_BROWSER_ASH, 'foo', 'bar.h'), ''),
        MockAffectedFile(
            os.path.join(_CHROME_BROWSER_ASH, 'foo', 'DEPS'),
            DEPS_FILE_NOT_PROHIBITING_CHROME),
        MockAffectedFile(
            os.path.join(_CHROME_BROWSER_ASH, 'bar', 'baz.h'), ''),
        MockAffectedFile(
            os.path.join(_CHROME_BROWSER_ASH, 'bar', 'DEPS'),
            DEPS_FILE_NOT_PROHIBITING_CHROME),
    ])
    self.assertDepsFilesWithErrors(
        input_api, [],
        [
          os.path.join(_CHROME_BROWSER_ASH, 'foo', 'DEPS'),
          os.path.join(_CHROME_BROWSER_ASH, 'bar', 'DEPS'),
        ])

  # Create files in a new subdirectories with DEPS files prohibiting //chrome.
  def testFilesWithDepsFilesProhibitingChrome(self):
    DEPS_FILE_PROHIBITING_CHROME = ['include_rules = [ "-chrome", ]']

    input_api = MockInputApi()
    input_api.InitFiles([
        MockAffectedFile(
            os.path.join(_CHROME_BROWSER_ASH, 'foo', 'bar.h'), ''),
        MockAffectedFile(
            os.path.join(_CHROME_BROWSER_ASH, 'foo', 'DEPS'),
            DEPS_FILE_PROHIBITING_CHROME),
        MockAffectedFile(
            os.path.join(_CHROME_BROWSER_ASH, 'bar', 'baz.h'), ''),
        MockAffectedFile(
            os.path.join(_CHROME_BROWSER_ASH, 'bar', 'DEPS'),
            DEPS_FILE_PROHIBITING_CHROME),
    ])
    self.assertDepsFilesWithErrors(input_api, [], [])

if __name__ == '__main__':
  unittest.main()
