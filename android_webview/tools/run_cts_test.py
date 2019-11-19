# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import tempfile
import sys
import unittest

import mock  # pylint: disable=import-error
import run_cts

sys.path.append(os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir, 'build', 'android'))
import devil_chromium  # pylint: disable=import-error, unused-import
from devil.android.ndk import abis  # pylint: disable=import-error
from devil.android.sdk import version_codes  # pylint: disable=import-error

class _RunCtsTest(unittest.TestCase):
  """Unittests for the run_cts module.
  """

  _EXCLUDED_TEST = 'bad#test'
  # This conforms to schema of the test_run entry in
  # cts_config/webview_cts_gcs_path.json
  _CTS_RUN = {'apk': 'module.apk', 'excludes': [{'match': _EXCLUDED_TEST}]}

  @staticmethod
  def _getArgsMock(**kwargs):
    args = {'test_filter_file': None, 'test_filter': None,
            'isolated_script_test_filter': None,
            'skip_expected_failures': False}
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

  def testDetermineCtsRelease_marshmallow(self):
    logging_mock = mock.Mock()
    logging.info = logging_mock
    device = mock.Mock(build_version_sdk=version_codes.MARSHMALLOW)
    self.assertEqual(run_cts.DetermineCtsRelease(device), 'M')
    # We should log a message to explain how we auto-determined the CTS release.
    # We don't assert the message itself, since that's rather strict.
    logging_mock.assert_called()

  def testDetermineCtsRelease_tooLow(self):
    device = mock.Mock(build_version_sdk=version_codes.KITKAT)
    with self.assertRaises(Exception) as cm:
      run_cts.DetermineCtsRelease(device)
    message = str(cm.exception)
    self.assertIn('not updatable', message)

  def testDetermineCtsRelease_tooHigh(self):
    device = mock.Mock(build_version_sdk=version_codes.OREO)
    # Mock this out with a couple version codes to check that the logic is
    # correct, without making assumptions about what version_codes we may
    # support in the future.
    mock_sdk_platform_dict = {
        version_codes.MARSHMALLOW: 'min fake release',
        version_codes.NOUGAT: 'max fake release',
    }
    run_cts.SDK_PLATFORM_DICT = mock_sdk_platform_dict
    with self.assertRaises(Exception) as cm:
      run_cts.DetermineCtsRelease(device)
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
    mock_args = self._getArgsMock(test_filter='good#test',
                                  skip_expected_failures=False)
    self.assertEqual([run_cts.TEST_FILTER_OPT + '=good.test'],
                     run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testFilter_CombinesAll(self):
    mock_args = self._getArgsMock(test_filter='good#test',
                                  skip_expected_failures=True)
    skip = self._getSkipString()
    self.assertEqual([run_cts.TEST_FILTER_OPT + '=good.test-' + skip],
                     run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testFilter_ForMultipleTests(self):
    mock_args = self._getArgsMock(test_filter='good#t1:good#t2',
                                  skip_expected_failures=True)
    skip = self._getSkipString()
    self.assertEqual([run_cts.TEST_FILTER_OPT + '=good.t1:good.t2-' + skip],
                     run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testIsolatedFilter_CombinesExcludedMatches(self):
    mock_args = self._getArgsMock(isolated_script_test_filter='good#test',
                                  skip_expected_failures=False)
    self.assertEqual([run_cts.TEST_FILTER_OPT + '=good.test'],
                     run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testIsolatedFilter_CombinesAll(self):
    mock_args = self._getArgsMock(isolated_script_test_filter='good#test',
                                  skip_expected_failures=True)
    skip = self._getSkipString()
    self.assertEqual([run_cts.TEST_FILTER_OPT + '=good.test-' + skip],
                     run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testIsolatedFilter_ForMultipleTests(self):
    # Isolated test filters use :: to separate matches
    mock_args = self._getArgsMock(
        isolated_script_test_filter='good#t1::good#t2',
        skip_expected_failures=True)
    skip = self._getSkipString()
    self.assertEqual([run_cts.TEST_FILTER_OPT + '=good.t1:good.t2-' + skip],
                     run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testFilterFile_CombinesExcludedMatches(self):
    with tempfile.NamedTemporaryFile(prefix='cts_run_test') as filter_file:
      filter_file.write('suite.goodtest')
      filter_file.seek(0)
      mock_args = self._getArgsMock(
          test_filter_file=filter_file.name,
          skip_expected_failures=False)
      self.assertEqual([run_cts.TEST_FILTER_OPT + '=suite.goodtest'],
                       run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testFilterFile_CombinesAll(self):
    with tempfile.NamedTemporaryFile(prefix='cts_run_test') as filter_file:
      filter_file.write('suite.goodtest')
      filter_file.seek(0)
      mock_args = self._getArgsMock(
          test_filter_file=filter_file.name,
          skip_expected_failures=True)
      skip = self._getSkipString()
      self.assertEqual([run_cts.TEST_FILTER_OPT + '=suite.goodtest-' + skip],
                       run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testNegative_Filter(self):
    mock_args = self._getArgsMock(test_filter='-good#t1:good#t2',
                                  skip_expected_failures=True)
    skip = self._getSkipString()
    self.assertEqual([run_cts.TEST_FILTER_OPT + '=-good.t1:good.t2:' + skip],
                     run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testNegative_IsolatedFilter(self):
    mock_args = self._getArgsMock(
        isolated_script_test_filter='-good#t1::good#t2',
        skip_expected_failures=True)
    skip = self._getSkipString()
    self.assertEqual([run_cts.TEST_FILTER_OPT + '=-good.t1:good.t2:' + skip],
                     run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testFilter_OverridesInclusion(self):
    mock_args = self._getArgsMock(test_filter='good#test1',
                                  skip_expected_failures=False)
    cts_run = {'apk': 'module.apk', 'includes': [{'match': 'good#test2'}]}
    self.assertEqual([run_cts.TEST_FILTER_OPT + '=good.test1'],
                     run_cts.GetTestRunFilterArg(mock_args, cts_run))

if __name__ == '__main__':
  unittest.main()
