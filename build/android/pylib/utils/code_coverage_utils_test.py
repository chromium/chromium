# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for code_coverage_utils_test.py.

Example usage:
  vpython3 code_coverage_utils_test.py
"""

# pylint: disable=protected-access

import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

path_root = Path(__file__).parents[2]
sys.path.append(str(path_root))

from pylib.utils import code_coverage_utils
from py_utils import tempfile_ext


class MockDevicePathExists:
    def __init__(self, value):
        self._path_exists = value

    def PathExists(self, directory, as_root=False, retries=0):  # pylint: disable=unused-argument
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
                suffix=code_coverage_utils._PROFRAW_FILE_EXTENSION,
            )
            code_coverage_utils.MergeClangCoverageFiles(cov_tempd, pro_tempd)
            # Merged file should be deleted.
            self.assertFalse(os.path.exists(profdata.name))
            self.assertTrue(mock_sub.called)

    @mock.patch('os.path.isfile', return_value=True)
    @mock.patch('shutil.rmtree')
    @mock.patch('pylib.utils.code_coverage_utils.PullClangCoverageFiles')
    @mock.patch('pylib.utils.code_coverage_utils.MergeClangCoverageFiles')
    def testPullAndMaybeMergeClangCoverageFiles(
        self, mock_merge_function, mock_pull_function, mock_rmtree, _
    ):
        mock_device = MockDevicePathExists(True)
        code_coverage_utils.PullAndMaybeMergeClangCoverageFiles(
            mock_device,
            'device_coverage_dir',
            'output_dir',
            'output_subfolder_name',
        )
        mock_pull_function.assert_called_with(
            mock_device,
            'device_coverage_dir',
            'output_dir/output_subfolder_name',
            as_root=False,
        )
        mock_merge_function.assert_called_with(
            'output_dir', 'output_dir/output_subfolder_name/device_coverage_dir'
        )
        self.assertTrue(mock_rmtree.called)

    @mock.patch('os.path.isfile', return_value=True)
    @mock.patch('shutil.rmtree')
    @mock.patch('pylib.utils.code_coverage_utils.PullClangCoverageFiles')
    @mock.patch('pylib.utils.code_coverage_utils.MergeClangCoverageFiles')
    def testPullAndMaybeMergeClangCoverageFilesAsRoot(
        self, mock_merge_function, mock_pull_function, mock_rmtree, _
    ):
        mock_device = mock.MagicMock()
        mock_device.PathExists.return_value = True

        code_coverage_utils.PullAndMaybeMergeClangCoverageFiles(
            mock_device,
            '/data/media/10/sdcard/chrome/coverage',
            'output_dir',
            'output_subfolder_name',
            as_root=True,
        )
        mock_device.PathExists.assert_called_once_with(
            '/data/media/10/sdcard/chrome/coverage', as_root=True, retries=0
        )
        mock_pull_function.assert_called_with(
            mock_device,
            '/data/media/10/sdcard/chrome/coverage',
            'output_dir/output_subfolder_name',
            as_root=True,
        )
        mock_merge_function.assert_called_with(
            'output_dir', 'output_dir/output_subfolder_name/coverage'
        )
        self.assertTrue(mock_rmtree.called)

    @mock.patch('os.path.isfile', return_value=True)
    @mock.patch('shutil.rmtree')
    @mock.patch('pylib.utils.code_coverage_utils.PullClangCoverageFiles')
    @mock.patch('pylib.utils.code_coverage_utils.MergeClangCoverageFiles')
    def testPullAndMaybeMergeClangCoverageFilesNoPull(
        self, mock_merge_function, mock_pull_function, mock_rmtree, _
    ):
        mock_device = MockDevicePathExists(False)
        code_coverage_utils.PullAndMaybeMergeClangCoverageFiles(
            mock_device,
            'device_coverage_dir',
            'output_dir',
            'output_subfolder_name',
        )
        self.assertFalse(mock_pull_function.called)
        self.assertFalse(mock_merge_function.called)
        self.assertFalse(mock_rmtree.called)

    @mock.patch('os.path.isfile', return_value=False)
    @mock.patch('shutil.rmtree')
    @mock.patch('pylib.utils.code_coverage_utils.PullClangCoverageFiles')
    @mock.patch('pylib.utils.code_coverage_utils.MergeClangCoverageFiles')
    def testPullAndMaybeMergeClangCoverageFilesNoMerge(
        self, mock_merge_function, mock_pull_function, mock_rmtree, _
    ):
        mock_device = MockDevicePathExists(True)
        code_coverage_utils.PullAndMaybeMergeClangCoverageFiles(
            mock_device,
            'device_coverage_dir',
            'output_dir',
            'output_subfolder_name',
        )
        mock_pull_function.assert_called_with(
            mock_device,
            'device_coverage_dir',
            'output_dir/output_subfolder_name',
            as_root=False,
        )
        self.assertFalse(mock_merge_function.called)
        self.assertFalse(mock_rmtree.called)

    @mock.patch('os.path.exists', return_value=True)
    @mock.patch('os.listdir', return_value=['file.profraw'])
    def testPullClangCoverageFiles(self, _mock_listdir, _mock_exists):
        mock_device = mock.MagicMock()
        code_coverage_utils.PullClangCoverageFiles(
            mock_device,
            '/data/media/10/profraw',
            '/tmp/output',
            as_root=True,
        )
        mock_device.PullFile.assert_called_once_with(
            '/data/media/10/profraw', '/tmp/output', as_root=True
        )
        mock_device.RemovePath.assert_called_once_with(
            '/data/media/10/profraw', force=True, recursive=True, as_root=True
        )


if __name__ == '__main__':
    unittest.main(verbosity=2)
