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


if __name__ == '__main__':
  unittest.main(verbosity=2)
