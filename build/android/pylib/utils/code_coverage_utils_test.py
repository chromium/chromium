# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for code_coverage_utils_test.py."""

# pylint: disable=protected-access

import os
import tempfile
import unittest

from pylib.utils import code_coverage_utils
from py_utils import tempfile_ext

import mock  # pylint: disable=import-error


class MockDevicePathExists:
  def __init__(self, value):
    self._path_exists = value

  def PathExists(self, directory, retries):  # pylint: disable=unused-argument
    return self._path_exists


class CodeCoverageUtilsTest(unittest.TestCase):
  @mock.patch('subprocess.check_output')
  def testMergeCoverageFiles(self, mock_sub):
    with tempfile_ext.NamedTemporaryDirectory() as cov_tempd:
      pro_tempd = os.path.join(cov_tempd, 'profraw')
      os.mkdir(pro_tempd)
      profdata = tempfile.NamedTemporaryFile(
          dir=pro_tempd,
          delete=False,
          suffix=code_coverage_utils._PROFRAW_FILE_EXTENSION)
      code_coverage_utils.MergeClangCoverageFiles(cov_tempd, pro_tempd)
      # Merged file should be deleted.
      self.assertFalse(os.path.exists(profdata.name))
      self.assertTrue(mock_sub.called)

  @mock.patch('os.path.isfile', return_value=True)
  @mock.patch('shutil.rmtree')
  @mock.patch('pylib.utils.code_coverage_utils.PullClangCoverageFiles')
  @mock.patch('pylib.utils.code_coverage_utils.MergeClangCoverageFiles')
  def testPullAndMaybeMergeClangCoverageFiles(self, mock_merge_function,
                                              mock_pull_function, mock_rmtree,
                                              _):
    mock_device = MockDevicePathExists(True)
    code_coverage_utils.PullAndMaybeMergeClangCoverageFiles(
        mock_device, 'device_coverage_dir', 'output_dir',
        'output_subfolder_name')
    mock_pull_function.assert_called_with(mock_device, 'device_coverage_dir',
                                          'output_dir/output_subfolder_name')
    mock_merge_function.assert_called_with(
        'output_dir', 'output_dir/output_subfolder_name/device_coverage_dir')
    self.assertTrue(mock_rmtree.called)

  @mock.patch('os.path.isfile', return_value=True)
  @mock.patch('shutil.rmtree')
  @mock.patch('pylib.utils.code_coverage_utils.PullClangCoverageFiles')
  @mock.patch('pylib.utils.code_coverage_utils.MergeClangCoverageFiles')
  def testPullAndMaybeMergeClangCoverageFilesNoPull(self, mock_merge_function,
                                                    mock_pull_function,
                                                    mock_rmtree, _):
    mock_device = MockDevicePathExists(False)
    code_coverage_utils.PullAndMaybeMergeClangCoverageFiles(
        mock_device, 'device_coverage_dir', 'output_dir',
        'output_subfolder_name')
    self.assertFalse(mock_pull_function.called)
    self.assertFalse(mock_merge_function.called)
    self.assertFalse(mock_rmtree.called)

  @mock.patch('os.path.isfile', return_value=False)
  @mock.patch('shutil.rmtree')
  @mock.patch('pylib.utils.code_coverage_utils.PullClangCoverageFiles')
  @mock.patch('pylib.utils.code_coverage_utils.MergeClangCoverageFiles')
  def testPullAndMaybeMergeClangCoverageFilesNoMerge(self, mock_merge_function,
                                                     mock_pull_function,
                                                     mock_rmtree, _):
    mock_device = MockDevicePathExists(True)
    code_coverage_utils.PullAndMaybeMergeClangCoverageFiles(
        mock_device, 'device_coverage_dir', 'output_dir',
        'output_subfolder_name')
    mock_pull_function.assert_called_with(mock_device, 'device_coverage_dir',
                                          'output_dir/output_subfolder_name')
    self.assertFalse(mock_merge_function.called)
    self.assertFalse(mock_rmtree.called)


if __name__ == '__main__':
  unittest.main(verbosity=2)
