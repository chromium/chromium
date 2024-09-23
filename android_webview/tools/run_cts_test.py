#!/usr/bin/env vpython3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import absolute_import
import logging
import os
import tempfile
import sys
import unittest

import mock  # pylint: disable=import-error
import run_cts

sys.path.append(os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir, 'build', 'android'))
# pylint: disable=wrong-import-position,import-error
import devil_chromium  # pylint: disable=unused-import
from devil.android.ndk import abis
from devil.android.sdk import version_codes


class _RunCtsTest(unittest.TestCase):
  """Unittests for the run_cts module.
  """

  _EXCLUDED_TEST = 'bad#test'
  # This conforms to schema of the test_run entry in
  # cts_config/webview_cts_gcs_path.json
  _CTS_RUN = {'apk': 'module.apk', 'excludes': [{'match': _EXCLUDED_TEST}]}

  @staticmethod
  def _getArgsMock(**kwargs):
    args = {
        'test_filter_files': None,
        'test_filters': None,
        'isolated_script_test_filters': None,
        'skip_expected_failures': False
    }
    args.update(kwargs)
    return mock.Mock(**args)

  def _getSkipString(self):
    return self._EXCLUDED_TEST.replace('#', '.')

  def testDetermineArch_arm64(self):
    logging_mock = mock.Mock()
    logging.info = logging_mock
    device = mock.Mock(product_cpu_abi=abis.ARM_64)
    self.assertEqual(run_cts.DetermineArch(device), 'arm64')
    # We should log a message to explain how we auto-determined the arch. We
    # don't assert the message itself, since that's rather strict.
    logging_mock.assert_called()

  def testDetermineArch_unsupported(self):
    device = mock.Mock(product_cpu_abi='madeup-abi')
    with self.assertRaises(Exception) as _:
      run_cts.DetermineArch(device)

  def testDetermineCtsRelease_oreo(self):
    logging_mock = mock.Mock()
    logging.info = logging_mock
    device = mock.Mock(build_version_sdk=version_codes.OREO)
    self.assertEqual(run_cts.DetermineCtsRelease(device), 'O')
    # We should log a message to explain how we auto-determined the CTS release.
    # We don't assert the message itself, since that's rather strict.
    logging_mock.assert_called()

  def testDetermineCtsRelease_tooLow(self):
    device = mock.Mock(build_version_sdk=version_codes.NOUGAT_MR1)
    with self.assertRaises(Exception) as cm:
      run_cts.DetermineCtsRelease(device)
    message = str(cm.exception)
    self.assertIn("We don't support running CTS tests on platforms less than",
                  message)

  def testDetermineCtsRelease_tooHigh(self):
    device = mock.Mock(build_version_sdk=version_codes.OREO)
    # Mock this out with a couple version codes to check that the logic is
    # correct, without making assumptions about what version_codes we may
    # support in the future.
    mock_sdk_platform_dict = {
        version_codes.MARSHMALLOW: 'min fake release',
        version_codes.NOUGAT: 'max fake release',
    }
    original_sdk_platform_dict = run_cts.SDK_PLATFORM_DICT
    run_cts.SDK_PLATFORM_DICT = mock_sdk_platform_dict
    with self.assertRaises(Exception) as cm:
      run_cts.DetermineCtsRelease(device)
    run_cts.SDK_PLATFORM_DICT = original_sdk_platform_dict
    message = str(cm.exception)
    self.assertIn('--cts-release max fake release', message,
                  msg='Should recommend the highest supported CTS release')

  def testNoFilter_SkipExpectedFailures(self):
    mock_args = self._getArgsMock(skip_expected_failures=True)
    skip = self._getSkipString()
    self.assertEqual([run_cts.TEST_FILTER_OPT + '=-' + skip],
                     run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testNoFilter_ExcludedMatches(self):
    mock_args = self._getArgsMock(skip_expected_failures=False)
    self.assertEqual([],
                     run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testFilter_CombinesExcludedMatches(self):
    mock_args = self._getArgsMock(test_filters=['good#test'],
                                  skip_expected_failures=False)
    self.assertEqual([run_cts.TEST_FILTER_OPT + '=good.test'],
                     run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testFilter_CombinesAll(self):
    mock_args = self._getArgsMock(test_filters=['good#test'],
                                  skip_expected_failures=True)
    skip = self._getSkipString()
    self.assertEqual([
        run_cts.TEST_FILTER_OPT + '=good.test',
        run_cts.TEST_FILTER_OPT + '=-' + skip
    ], run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testFilter_ForMultipleTests(self):
    mock_args = self._getArgsMock(test_filters=['good#t1:good#t2'],
                                  skip_expected_failures=True)
    skip = self._getSkipString()
    self.assertEqual([
        run_cts.TEST_FILTER_OPT + '=good.t1:good.t2',
        run_cts.TEST_FILTER_OPT + '=-' + skip
    ], run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testFilter_IncludesForArchitecture(self):
    mock_args = self._getArgsMock()

    cts_run = {
        'apk':
        'module.apk',
        'includes': [{
            'match': 'good#test1',
            'arch': 'x86'
        }, {
            'match': 'good#test2'
        }, {
            'match': 'exclude#test4',
            'arch': 'arm64'
        }]
    }

    self.assertEqual([run_cts.TEST_FILTER_OPT + '=good.test1:good.test2'],
                     run_cts.GetTestRunFilterArg(mock_args, cts_run,
                                                 arch='x86'))

  def testFilter_ExcludesForArchitecture(self):
    mock_args = self._getArgsMock(skip_expected_failures=True)

    cts_run = {
        'apk':
        'module.apk',
        'excludes': [{
            'match': 'good#test1',
            'arch': 'x86'
        }, {
            'match': 'good#test2'
        }, {
            'match': 'exclude#test4',
            'arch': 'arm64'
        }]
    }

    self.assertEqual([run_cts.TEST_FILTER_OPT + '=-good.test1:good.test2'],
                     run_cts.GetTestRunFilterArg(mock_args, cts_run,
                                                 arch='x86'))

  def testFilter_IncludesForMode(self):
    mock_args = self._getArgsMock()

    cts_run = {
        'apk':
        'module.apk',
        'includes': [{
            'match': 'good#test1',
            'mode': 'instant'
        }, {
            'match': 'good#test2'
        }, {
            'match': 'exclude#test4',
            'mode': 'full'
        }]
    }

    self.assertEqual([run_cts.TEST_FILTER_OPT + '=good.test1:good.test2'],
                     run_cts.GetTestRunFilterArg(mock_args,
                                                 cts_run,
                                                 test_app_mode='instant'))

  def testFilter_ExcludesForMode(self):
    mock_args = self._getArgsMock(skip_expected_failures=True)

    cts_run = {
        'apk':
        'module.apk',
        'excludes': [{
            'match': 'good#test1',
            'mode': 'instant'
        }, {
            'match': 'good#test2'
        }, {
            'match': 'exclude#test4',
            'mode': 'full'
        }]
    }

    self.assertEqual([run_cts.TEST_FILTER_OPT + '=-good.test1:good.test2'],
                     run_cts.GetTestRunFilterArg(mock_args,
                                                 cts_run,
                                                 test_app_mode='instant'))

  def testIsolatedFilter_CombinesExcludedMatches(self):
    mock_args = self._getArgsMock(isolated_script_test_filters=['good#test'],
                                  skip_expected_failures=False)
    self.assertEqual([run_cts.TEST_FILTER_OPT + '=good.test'],
                     run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testIsolatedFilter_CombinesAll(self):
    mock_args = self._getArgsMock(isolated_script_test_filters=['good#test'],
                                  skip_expected_failures=True)
    skip = self._getSkipString()
    self.assertEqual([
        run_cts.TEST_FILTER_OPT + '=good.test',
        run_cts.TEST_FILTER_OPT + '=-' + skip
    ], run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testIsolatedFilter_ForMultipleTests(self):
    # Isolated test filters use :: to separate matches
    mock_args = self._getArgsMock(
        isolated_script_test_filters=['good#t1::good#t2'],
        skip_expected_failures=True)
    skip = self._getSkipString()
    self.assertEqual([
        run_cts.TEST_FILTER_OPT + '=good.t1:good.t2',
        run_cts.TEST_FILTER_OPT + '=-' + skip
    ], run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  @unittest.skipIf(os.name == "nt", "Opening NamedTemporaryFile by name "
                   "doesn't work in Windows.")
  def testFilterFile_CombinesExcludedMatches(self):
    with tempfile.NamedTemporaryFile(prefix='cts_run_test') as filter_file:
      filter_file.write('suite.goodtest'.encode())
      filter_file.seek(0)
      mock_args = self._getArgsMock(test_filter_files=[filter_file.name],
                                    skip_expected_failures=False)
      self.assertEqual([run_cts.TEST_FILTER_OPT + '=suite.goodtest'],
                       run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  @unittest.skipIf(os.name == "nt", "Opening NamedTemporaryFile by name "
                   "doesn't work in Windows.")
  def testFilterFile_MultipleFilters(self):
    with tempfile.NamedTemporaryFile(prefix='cts_run_test') as filter_file:
      filter_file.write(
          'suite.goodtest1\nsuite.goodtest2\n-suite.badtest1'.encode())
      filter_file.seek(0)
      with tempfile.NamedTemporaryFile(prefix='cts_run_test2') as filter_file2:
        filter_file2.write(
            'suite.goodtest2\nsuite.goodtest3\n-suite.badtest2'.encode())
        filter_file2.seek(0)
        mock_args = self._getArgsMock(
            test_filter_files=[filter_file.name, filter_file2.name],
            skip_expected_failures=False)
        self.assertEqual([
            run_cts.TEST_FILTER_OPT +
            '=suite.goodtest1:suite.goodtest2-suite.badtest1',
            run_cts.TEST_FILTER_OPT +
            '=suite.goodtest2:suite.goodtest3-suite.badtest2'
        ], run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  @unittest.skipIf(os.name == "nt", "Opening NamedTemporaryFile by name "
                   "doesn't work in Windows.")
  def testFilterFile_CombinesAll(self):
    with tempfile.NamedTemporaryFile(prefix='cts_run_test') as filter_file:
      filter_file.write('suite.goodtest'.encode())
      filter_file.seek(0)
      mock_args = self._getArgsMock(test_filter_files=[filter_file.name],
                                    skip_expected_failures=True)
      skip = self._getSkipString()
      self.assertEqual([
          run_cts.TEST_FILTER_OPT + '=suite.goodtest',
          run_cts.TEST_FILTER_OPT + '=-' + skip
      ], run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testNegative_Filter(self):
    mock_args = self._getArgsMock(test_filters=['-good#t1:good#t2'],
                                  skip_expected_failures=True)
    skip = self._getSkipString()
    self.assertEqual([
        run_cts.TEST_FILTER_OPT + '=-good.t1:good.t2',
        run_cts.TEST_FILTER_OPT + '=-' + skip
    ], run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testNegative_IsolatedFilter(self):
    mock_args = self._getArgsMock(
        isolated_script_test_filters=['-good#t1::good#t2'],
        skip_expected_failures=True)
    skip = self._getSkipString()
    self.assertEqual([
        run_cts.TEST_FILTER_OPT + '=-good.t1:good.t2',
        run_cts.TEST_FILTER_OPT + '=-' + skip
    ], run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testFilter_OverridesInclusion(self):
    mock_args = self._getArgsMock(test_filters=['good#test1'],
                                  skip_expected_failures=False)
    cts_run = {'apk': 'module.apk', 'includes': [{'match': 'good#test2'}]}
    self.assertEqual([run_cts.TEST_FILTER_OPT + '=good.test1'],
                     run_cts.GetTestRunFilterArg(mock_args, cts_run))

if __name__ == '__main__':
  unittest.main()
