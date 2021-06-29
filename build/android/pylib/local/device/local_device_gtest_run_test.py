#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for local_device_gtest_test_run."""

# pylint: disable=protected-access


import os
import tempfile
import unittest

from pylib.gtest import gtest_test_instance
from pylib.local.device import local_device_environment
from pylib.local.device import local_device_gtest_run
from py_utils import tempfile_ext

import mock  # pylint: disable=import-error


class LocalDeviceGtestRunTest(unittest.TestCase):
  def setUp(self):
    self._obj = local_device_gtest_run.LocalDeviceGtestRun(
        mock.MagicMock(spec=local_device_environment.LocalDeviceEnvironment),
        mock.MagicMock(spec=gtest_test_instance.GtestTestInstance))

  def testExtractTestsFromFilter(self):
    # Checks splitting by colons.
    self.assertEqual([
        'b17',
        'm4e3',
        'p51',
    ], local_device_gtest_run._ExtractTestsFromFilter('b17:m4e3:p51'))
    # Checks the '-' sign.
    self.assertIsNone(local_device_gtest_run._ExtractTestsFromFilter('-mk2'))
    # Checks the more than one asterick.
    self.assertIsNone(
        local_device_gtest_run._ExtractTestsFromFilter('.mk2*:.M67*'))
    # Checks just an asterick without a period
    self.assertIsNone(local_device_gtest_run._ExtractTestsFromFilter('M67*'))
    # Checks an asterick at the end with a period.
    self.assertEqual(['.M67*'],
                     local_device_gtest_run._ExtractTestsFromFilter('.M67*'))

  def testGetLLVMProfilePath(self):
    path = local_device_gtest_run._GetLLVMProfilePath('test_dir', 'sr71', '5')
    self.assertEqual(path, os.path.join('test_dir', 'sr71_5_%2m.profraw'))

  @mock.patch('subprocess.check_output')
  def testMergeCoverageFiles(self, mock_sub):
    with tempfile_ext.NamedTemporaryDirectory() as cov_tempd:
      pro_tempd = os.path.join(cov_tempd, 'profraw')
      os.mkdir(pro_tempd)
      profdata = tempfile.NamedTemporaryFile(
          dir=pro_tempd,
          delete=False,
          suffix=local_device_gtest_run._PROFRAW_FILE_EXTENSION)
      local_device_gtest_run._MergeCoverageFiles(cov_tempd, pro_tempd)
      # Merged file should be deleted.
      self.assertFalse(os.path.exists(profdata.name))
      self.assertTrue(mock_sub.called)

  @mock.patch('pylib.utils.google_storage_helper.upload')
  def testUploadTestArtifacts(self, mock_gsh):
    link = self._obj._UploadTestArtifacts(mock.MagicMock(), None)
    self.assertFalse(mock_gsh.called)
    self.assertIsNone(link)

    result = 'A/10/warthog/path'
    mock_gsh.return_value = result
    with tempfile_ext.NamedTemporaryFile() as temp_f:
      link = self._obj._UploadTestArtifacts(mock.MagicMock(), temp_f)
    self.assertTrue(mock_gsh.called)
    self.assertEqual(result, link)


if __name__ == '__main__':
  unittest.main(verbosity=2)
