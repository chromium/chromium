#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for local_device_gtest_test_run."""

# pylint: disable=protected-access


import os
import unittest

from pylib.gtest import gtest_test_instance
from pylib.local.device import local_device_environment
from pylib.local.device import local_device_gtest_run

import mock  # pylint: disable=import-error


def isSliceInList(s, l):
  lenOfSlice = len(s)
  return any(s == l[i:lenOfSlice + i] for i in range(len(l) - lenOfSlice + 1))


class LocalDeviceGtestRunTest(unittest.TestCase):
  def setUp(self):
    self._obj = local_device_gtest_run.LocalDeviceGtestRun(
        mock.MagicMock(spec=local_device_environment.LocalDeviceEnvironment),
        mock.MagicMock(spec=gtest_test_instance.GtestTestInstance))

  def testExtractTestsFromFilter(self):
    # Checks splitting by colons.
    self.assertEqual(
        set([
            'm4e3',
            'p51',
            'b17',
        ]),
        set(local_device_gtest_run._ExtractTestsFromFilters(['b17:m4e3:p51'])))
    # Checks the '-' sign.
    self.assertIsNone(local_device_gtest_run._ExtractTestsFromFilters(['-mk2']))
    # Checks the more than one asterick.
    self.assertIsNone(
        local_device_gtest_run._ExtractTestsFromFilters(['.mk2*:.M67*']))
    # Checks just an asterick without a period
    self.assertIsNone(local_device_gtest_run._ExtractTestsFromFilters(['M67*']))
    # Checks an asterick at the end with a period.
    self.assertEqual(['.M67*'],
                     local_device_gtest_run._ExtractTestsFromFilters(['.M67*']))
    # Checks multiple filters intersect
    self.assertEqual(['m4e3'],
                     local_device_gtest_run._ExtractTestsFromFilters(
                         ['b17:m4e3:p51', 'b17:m4e3', 'm4e3:p51']))

  def testGetLLVMProfilePath(self):
    path = local_device_gtest_run._GetLLVMProfilePath('test_dir', 'sr71', '5')
    self.assertEqual(path, os.path.join('test_dir', 'sr71_5_%2m%c.profraw'))

  def testGroupTests(self):
    test = [
        "TestClass1.testcase1",
        "TestClass1.otherTestCase",
        "TestClass1.PRE_testcase1",
        "TestClass1.abc_testcase2",
        "TestClass1.PRE_PRE_testcase1",
        "TestClass1.PRE_abc_testcase2",
        "TestClass1.PRE_PRE_abc_testcase2",
    ]
    expectedTestcase1 = [
        "TestClass1.PRE_PRE_testcase1",
        "TestClass1.PRE_testcase1",
        "TestClass1.testcase1",
    ]
    expectedTestcase2 = [
        "TestClass1.PRE_PRE_abc_testcase2",
        "TestClass1.PRE_abc_testcase2",
        "TestClass1.abc_testcase2",
    ]
    expectedOtherTestcase = [
        "TestClass1.otherTestCase",
    ]
    actualTestCase = self._obj._GroupTests(test)
    self.assertTrue(isSliceInList(expectedTestcase1, actualTestCase))
    self.assertTrue(isSliceInList(expectedTestcase2, actualTestCase))
    self.assertTrue(isSliceInList(expectedOtherTestcase, actualTestCase))

  def testAppendPreTests(self):
    failed_tests = [
        "TestClass1.PRE_PRE_testcase1",
        "TestClass1.abc_testcase2",
        "TestClass1.PRE_def_testcase3",
        "TestClass1.otherTestCase",
    ]
    tests = [
        "TestClass1.testcase1",
        "TestClass1.otherTestCase",
        "TestClass1.def_testcase3",
        "TestClass1.PRE_testcase1",
        "TestClass1.abc_testcase2",
        "TestClass1.PRE_PRE_testcase1",
        "TestClass1.PRE_abc_testcase2",
        "TestClass1.PRE_def_testcase3",
        "TestClass1.PRE_PRE_abc_testcase2",
    ]
    expectedTestcase1 = [
        "TestClass1.PRE_PRE_testcase1",
        "TestClass1.PRE_testcase1",
        "TestClass1.testcase1",
    ]
    expectedTestcase2 = [
        "TestClass1.PRE_PRE_abc_testcase2",
        "TestClass1.PRE_abc_testcase2",
        "TestClass1.abc_testcase2",
    ]
    expectedTestcase3 = [
        "TestClass1.PRE_def_testcase3",
        "TestClass1.def_testcase3",
    ]
    expectedOtherTestcase = [
        "TestClass1.otherTestCase",
    ]
    actualTestCase = self._obj._AppendPreTestsForRetry(failed_tests, tests)
    self.assertTrue(isSliceInList(expectedTestcase1, actualTestCase))
    self.assertTrue(isSliceInList(expectedTestcase2, actualTestCase))
    self.assertTrue(isSliceInList(expectedTestcase3, actualTestCase))
    self.assertTrue(isSliceInList(expectedOtherTestcase, actualTestCase))

if __name__ == '__main__':
  unittest.main(verbosity=2)
