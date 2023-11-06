#!/usr/bin/env python
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import PRESUBMIT

sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
from PRESUBMIT_test_mocks import (MockInputApi, MockOutputApi, MockAffectedFile)

class GetTest(unittest.TestCase):
  def testStandardRunnerButFactoryPresent(self):
    diff = [
        '@RunWith(AwJUnit4ClassRunner.class)',
        '@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)',
        'public class AwFooBarTest {'
    ]
    input_api = MockInputApi()
    input_api.files = [MockAffectedFile('android_webview/javatests/AwFooBarTest.java', diff)]
    errors = PRESUBMIT._CheckAwJUnitTestRunner(input_api, MockOutputApi())
    self.assertEqual(1, len(errors))

  def testParameterizedRunnerButFactoryMissing(self):
    diff = [
        '@RunWith(Parameterized.class)',
        'public class AwFooBarTest {'
    ]
    input_api = MockInputApi()
    input_api.files = [MockAffectedFile('android_webview/javatests/AwFooBarTest.java', diff)]
    errors = PRESUBMIT._CheckAwJUnitTestRunner(input_api, MockOutputApi())
    self.assertEqual(1, len(errors))

  def testStandardRunnerWithoutFactory(self):
    diff = [
        '@RunWith(AwJUnit4ClassRunner.class)',
        'public class AwFooBarTest {'
    ]
    input_api = MockInputApi()
    input_api.files = [MockAffectedFile('android_webview/javatests/AwFooBarTest.java', diff)]
    errors = PRESUBMIT._CheckAwJUnitTestRunner(input_api, MockOutputApi())
    self.assertEqual(0, len(errors))

  def testParameterizedRunnerWithFactory(self):
    diff = [
        '@RunWith(Parameterized.class)',
        '@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)',
        'public class AwFooBarTest {'
    ]
    input_api = MockInputApi()
    input_api.files = [MockAffectedFile('android_webview/javatests/AwFooBarTest.java', diff)]
    errors = PRESUBMIT._CheckAwJUnitTestRunner(input_api, MockOutputApi())
    self.assertEqual(0, len(errors))

  def testIncorrectRunner(self):
    diff = [
        '@RunWith(BaseJUnit4ClassRunner.class)',
        'public class AwFooBarTest {'
    ]
    input_api = MockInputApi()
    input_api.files = [MockAffectedFile('android_webview/javatests/AwFooBarTest.java', diff)]
    errors = PRESUBMIT._CheckAwJUnitTestRunner(input_api, MockOutputApi())
    self.assertEqual(1, len(errors))

if __name__ == '__main__':
  unittest.main()
