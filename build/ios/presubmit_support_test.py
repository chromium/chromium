#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import sys
import unittest

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))

from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi
from build.ios import presubmit_support


class BundleDataPresubmit(unittest.TestCase):
  def setUp(self):
    self.mock_input_api = MockInputApi()
    self.mock_input_api.change.RepositoryRoot = lambda: os.path.join(
        os.path.dirname(__file__), '..', '..')
    self.mock_input_api.PresubmitLocalPath = lambda: os.path.dirname(__file__)
    self.mock_output_api = MockOutputApi()
    self.mock_input_api.verbose = False

  def testBasic(self):
    """
        Checks that a glob can be expanded to build a file list and if it
        matches the existing file list, we should see no error.
        """
    results = presubmit_support.CheckBundleData(self.mock_input_api,
                                                self.mock_output_api,
                                                'test_data/basic', '.')
    self.assertEqual(0, len(results))

  def testExclusion(self):
    """
        Check that globs can be used to exclude files from file lists.
        """
    results = presubmit_support.CheckBundleData(self.mock_input_api,
                                                self.mock_output_api,
                                                'test_data/exclusions', '.')
    self.assertEqual(0, len(results))

  def testDifferentLocalPath(self):
    """
        Checks the case where the presubmit directory is not the same as the
        globroot, but it is still local (i.e., not relative to the repository
        root)
        """
    results = presubmit_support.CheckBundleData(
        self.mock_input_api, self.mock_output_api,
        'test_data/different_local_path', 'test_data')
    self.assertEqual(0, len(results))

  def testRepositoryRelative(self):
    """
        Checks the case where globs are relative to the repository root.
        """
    results = presubmit_support.CheckBundleData(
        self.mock_input_api, self.mock_output_api,
        'test_data/repository_relative')
    self.assertEqual(0, len(results))

  def testMissingFilesInFilelist(self):
    """
        Checks that we do indeed return an error if the filelist is missing a
        file. In this case, all of the test .filelist and .globlist files are
        excluded.
        """
    results = presubmit_support.CheckBundleData(self.mock_input_api,
                                                self.mock_output_api,
                                                'test_data/missing', '.')
    self.assertEqual(1, len(results))

  def testExtraFilesInFilelist(self):
    """
        Checks the case where extra files have been included in the file list.
        """
    results = presubmit_support.CheckBundleData(self.mock_input_api,
                                                self.mock_output_api,
                                                'test_data/extra', '.')
    self.assertEqual(1, len(results))

  def testOrderInsensitive(self):
    """
        Checks that we do not trigger an error for cases where the file list is
        correct, but in a different order than the globlist expansion.
        """
    results = presubmit_support.CheckBundleData(self.mock_input_api,
                                                self.mock_output_api,
                                                'test_data/reorder', '.')
    self.assertEqual(0, len(results))

  def testUnexpectedHeader(self):
    """
        Checks an unexpected header in a file list causes an error.
        """
    results = presubmit_support.CheckBundleData(self.mock_input_api,
                                                self.mock_output_api,
                                                'test_data/comment', '.')
    self.assertEqual(1, len(results))


if __name__ == '__main__':
  unittest.main()
