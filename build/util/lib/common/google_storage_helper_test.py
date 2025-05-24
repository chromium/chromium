#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittest for google_storage_helper.py.

Example usage:
  vpython3 google_storage_helper_test.py
"""

import os
import sys
import time
import unittest
from unittest import mock

import google_storage_helper as helper  # pylint: disable=import-error
from parameterized import parameterized  # pylint: disable=import-error

LIB_PATH = os.path.join(os.path.dirname(__file__), os.pardir, os.pardir)
sys.path.append(LIB_PATH)

DIR_SOURCE_ROOT = os.environ.get(
    'CHECKOUT_SOURCE_ROOT',
    os.path.abspath(
        os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir,
                     os.pardir)))
DEVIL_PATH = os.path.join(DIR_SOURCE_ROOT, 'third_party', 'catapult', 'devil')

if DEVIL_PATH not in sys.path:
  sys.path.append(DEVIL_PATH)
from devil.utils import cmd_helper


class GoogleStorageHelperTest(unittest.TestCase):

  @parameterized.expand([
      (
          'empty_bucket_name',
          '',
          '',
      ),
      (
          'bucket_name_with_gs',
          'gs://foo/bar',
          'foo/bar',
      ),
      (
          'bucket_name_with_gs_end_slash',
          'gs://foo/bar/',
          'foo/bar',
      ),
      (
          'bucket_name_no_gs',
          'foo/bar',
          'foo/bar',
      ),
      (
          'bucket_name_no_gs_end_slash',
          'gs://foo/bar/',
          'foo/bar',
      ),
  ])
  def test_format_bucket_name(self, _, bucket, expected):
    got = helper._format_bucket_name(bucket)  # pylint: disable=protected-access
    self.assertEqual(
        got,
        expected,
    )

  @mock.patch('platform.system', autospec=True)
  @mock.patch.object(cmd_helper, 'RunCmd', autospec=True)
  def test_exists(self, mock_cmd_helper, mock_system):
    bucket = 'foo'
    name = 'bar'
    with self.subTest(name='Windows'):
      with mock.patch.object(helper,
                             'get_gsutil_script_path',
                             autospec=True,
                             return_value='path_to_gsutil_py'):
        mock_system.return_value = 'Windows'
        helper.exists(name, bucket)
        mock_cmd_helper.assert_called_once_with(
            ['path_to_gsutil_py', '-q', 'stat', 'gs://foo/bar'])
    mock_cmd_helper.reset_mock()
    with self.subTest(name='Linux'):
      with mock.patch.object(helper,
                             'get_gsutil_script_path',
                             autospec=True,
                             return_value='path_to_gsutil'):
        mock_system.return_value = 'Linux'
        helper.exists(name, bucket)
        mock_cmd_helper.assert_called_once_with(
            ['path_to_gsutil', '-q', 'stat', 'gs://foo/bar'])

  @mock.patch('platform.system', autospec=True)
  @mock.patch.object(cmd_helper, 'RunCmd', autospec=True)
  @mock.patch.object(helper, 'get_url_link', autospec=True)
  def test_upload(self, mock_get_url_link, mock_cmd_helper, mock_system):
    bucket = 'foo'
    name = 'bar'
    filepath = os.path.join('abc', 'def.json')
    with self.subTest(name='Windows'):
      with mock.patch.object(helper,
                             'get_gsutil_script_path',
                             autospec=True,
                             return_value='path_to_gsutil_py'):
        mock_system.return_value = 'Windows'
        helper.upload(name, filepath, bucket)
        mock_cmd_helper.assert_called_once_with([
            'path_to_gsutil_py', '-q', 'cp',
            os.path.join('abc', 'def.json'), 'gs://foo/bar'
        ])
        mock_get_url_link.assert_called_once_with(name, bucket, True)
    mock_cmd_helper.reset_mock()
    mock_get_url_link.reset_mock()
    with self.subTest(name='Linux'):
      with mock.patch.object(helper,
                             'get_gsutil_script_path',
                             autospec=True,
                             return_value='path_to_gsutil'):
        mock_system.return_value = 'Linux'
        helper.upload(name, filepath, bucket)
        mock_cmd_helper.assert_called_once_with([
            'path_to_gsutil', '-q', 'cp',
            os.path.join('abc', 'def.json'), 'gs://foo/bar'
        ])
        mock_get_url_link.assert_called_once_with(name, bucket, True)

  @parameterized.expand([
      ('empty_bucket_name', '', 'bar', 'https://storage.cloud.google.com//bar'),
      ('empty_name', 'foo', '', 'https://storage.cloud.google.com/foo/'),
      ('normal', 'foo', 'bar', 'https://storage.cloud.google.com/foo/bar')
  ])
  def test_get_url_link(self, _, bucket, name, expected):
    got = helper.get_url_link(name, bucket, True)
    self.assertEqual(expected, got)

  @mock.patch('time.gmtime')
  def test_unique_name(self, mock_gmtime):
    basename = 'foo'
    suffix = '.json'
    expected = 'foo_2026_01_15_T12_30_00-UTC.json'
    mock_gmtime.return_value = time.struct_time(
        (2026, 1, 15, 12, 30, 0, 3, 15, 0))
    got = helper.unique_name(basename, suffix)
    self.assertEqual(expected, got)

  @parameterized.expand([
      ('empty_link', '', ['path_to_gsutil', '-q', 'cat', 'gs://']),
      ('valid gs_link', '/foo', ['path_to_gsutil', '-q', 'cat', 'gs://foo']),
      ('no_initial_slash', 'foo/bar',
       ['path_to_gsutil', '-q', 'cat', 'gs://foo/bar'])
  ])  # pylint: disable=no-self-use
  def test_read_from_link(self, _, link, expected_sequence):
    with mock.patch('platform.system', autospec=True, return_value='Linux'):
      with mock.patch.object(helper,
                             'get_gsutil_script_path',
                             autospec=True,
                             return_value='path_to_gsutil'):
        with mock.patch.object(cmd_helper, 'GetCmdOutput',
                               autospec=True) as mock_get_cmd_output:
          helper.read_from_link(link)
          mock_get_cmd_output.assert_called_once_with(expected_sequence)


if __name__ == '__main__':
  unittest.main()
