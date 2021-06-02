#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import PRESUBMIT

sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from PRESUBMIT_test_mocks import MockFile, MockInputApi

class InvalidOSMacroNamesTest(unittest.TestCase):
  def testChromeDoesNotUseOSAPPLE(self):
    lines = ['#if defined(OS_APPLE)',
             '#error OS_APPLE not allowed',
             '#endif']
    errors = PRESUBMIT._CheckNoOSAPPLEMacrosInChromeFile(
        MockInputApi(), MockFile('chrome/path/foo_platform.cc', lines))
    self.assertEqual(1, len(errors))
    self.assertEqual('    chrome/path/foo_platform.cc:1', errors[0])

  def testChromeDoesNotUseOSIOS(self):
    lines = ['#if defined(OS_IOS)',
             '#error OS_IOS not allowed',
             '#endif']
    errors = PRESUBMIT._CheckNoOSIOSMacrosInChromeFile(
        MockInputApi(), MockFile('chrome/path/foo_platform.cc', lines))
    self.assertEqual(1, len(errors))
    self.assertEqual('    chrome/path/foo_platform.cc:1', errors[0])

if __name__ == '__main__':
  unittest.main()
