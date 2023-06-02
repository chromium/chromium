#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import PRESUBMIT

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))

from PRESUBMIT_test_mocks import MockAffectedFile
from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi



def _fails_deps_check(line, filename='BUILD.gn'):
  mock_input_api = MockInputApi()
  mock_input_api.files = [MockAffectedFile(filename, [line])]
  errors = PRESUBMIT.CheckNoBadDeps(mock_input_api, MockOutputApi())
  return bool(errors)


class CheckNoBadDepsTest(unittest.TestCase):
  def testComments(self):
    self.assertFalse(_fails_deps_check('no # import("//third_party/foo")'))

  def testFiles(self):
    self.assertFalse(
        _fails_deps_check('import("//third_party/foo")', filename='foo.txt'))
    self.assertTrue(
        _fails_deps_check('import("//third_party/foo")', filename='foo.gni'))

  def testPaths(self):
    self.assertFalse(_fails_deps_check('import("//build/things.gni")'))
    self.assertTrue(_fails_deps_check('import("//chrome/things.gni")'))


if __name__ == '__main__':
  unittest.main()
