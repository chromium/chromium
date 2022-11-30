#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import PRESUBMIT

sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from PRESUBMIT_test_mocks import MockFile, MockInputApi

class DisallowedBuildFlagsTest(unittest.TestCase):
  def testChromeDoesNotUseISAPPLE(self):
    lines = ['#if BUILDFLAG(IS_APPLE)',
             '#error IS_APPLE not allowed',
             '#endif']
    errors = PRESUBMIT._CheckNoIsAppleBuildFlagsInChromeFile(
        MockInputApi(), MockFile('chrome/path/foo_platform.cc', lines))
    self.assertEqual(1, len(errors))
    self.assertEqual('    chrome/path/foo_platform.cc:1', errors[0])

  def testChromeDoesNotUseISIOS(self):
    lines = ['#if BUILDFLAG(IS_IOS)',
             '#error IS_IOS not allowed',
             '#endif']
    errors = PRESUBMIT._CheckNoIsIOSBuildFlagsInChromeFile(
        MockInputApi(), MockFile('chrome/path/foo_platform.cc', lines))
    self.assertEqual(1, len(errors))
    self.assertEqual('    chrome/path/foo_platform.cc:1', errors[0])

if __name__ == '__main__':
  unittest.main()
