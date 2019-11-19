#!/usr/bin/env python
# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import subprocess
import sys
import unittest

import PRESUBMIT

file_dir_path = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(file_dir_path, '..', '..'))
from PRESUBMIT_test_mocks import MockFile, MockAffectedFile
from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi

_VALID_DEP = "+third_party/blink/public/platform/web_something.h,"
_INVALID_DEP = "+third_party/blink/public/web/web_something.h,"
_INVALID_DEP2 = "+third_party/blink/public/web/web_nothing.h,"

class BlinkPublicWebUnwantedDependenciesTest(unittest.TestCase):
  def makeInputApi(self, files):
    input_api = MockInputApi()
    input_api.files = files
    # Override os_path.exists because the presubmit uses the actual
    # os.path.exists.
    input_api.CreateMockFileInPath(
        [x.LocalPath() for x in input_api.AffectedFiles(include_deletes=True)])
    return input_api

  INVALID_DEPS_MESSAGE = ('chrome/browser cannot depend on '
                          'blink/public/web interfaces. Use'
                          ' blink/public/common instead.')

  def testAdditionOfUnwantedDependency(self):
    input_api = self.makeInputApi([
        MockAffectedFile('DEPS', [_INVALID_DEP], [], action='M')])
    warnings = PRESUBMIT._CheckUnwantedDependencies(input_api, MockOutputApi())
    self.assertEqual(1, len(warnings))
    self.assertEqual(self.INVALID_DEPS_MESSAGE, warnings[0].message)
    self.assertEqual(1, len(warnings[0].items))

  def testAdditionOfUnwantedDependencyInComment(self):
    input_api = self.makeInputApi([
        MockAffectedFile('DEPS', ["#" + _INVALID_DEP], [], action='M')])
    warnings = PRESUBMIT._CheckUnwantedDependencies(input_api, MockOutputApi())
    self.assertEqual([], warnings)

  def testAdditionOfValidDependency(self):
    input_api = self.makeInputApi([
        MockAffectedFile('DEPS', [_VALID_DEP], [], action='M')])
    warnings = PRESUBMIT._CheckUnwantedDependencies(input_api, MockOutputApi())
    self.assertEqual([], warnings)

  def testAdditionOfMultipleUnwantedDependency(self):
    input_api = self.makeInputApi([
        MockAffectedFile('DEPS', [_INVALID_DEP, _INVALID_DEP2], action='M')])
    warnings = PRESUBMIT._CheckUnwantedDependencies(input_api, MockOutputApi())
    self.assertEqual(1, len(warnings))
    self.assertEqual(self.INVALID_DEPS_MESSAGE, warnings[0].message)
    self.assertEqual(2, len(warnings[0].items))

    input_api = self.makeInputApi([
        MockAffectedFile('DEPS', [_INVALID_DEP, _VALID_DEP], [], action='M')])
    warnings = PRESUBMIT._CheckUnwantedDependencies(input_api, MockOutputApi())
    self.assertEqual(1, len(warnings))
    self.assertEqual(self.INVALID_DEPS_MESSAGE, warnings[0].message)
    self.assertEqual(1, len(warnings[0].items))

  def testRemovalOfUnwantedDependency(self):
    input_api = self.makeInputApi([
        MockAffectedFile('DEPS', [], [_INVALID_DEP], action='M')])
    warnings = PRESUBMIT._CheckUnwantedDependencies(input_api, MockOutputApi())
    self.assertEqual([], warnings)

  def testRemovalOfValidDependency(self):
    input_api = self.makeInputApi([
        MockAffectedFile('DEPS', [], [_VALID_DEP], action='M')])
    warnings = PRESUBMIT._CheckUnwantedDependencies(input_api, MockOutputApi())
    self.assertEqual([], warnings)

if __name__ == '__main__':
  unittest.main()
