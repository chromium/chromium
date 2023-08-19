#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from unittest import mock

from parameterized import parameterized

from update_sdk import _GetHostArch
from update_sdk import _GetTarballPath
from update_sdk import GetSDKOverrideGCSPath


@mock.patch('platform.machine')
class TestGetHostArch(unittest.TestCase):
  @parameterized.expand([('x86_64', 'amd64'), ('AMD64', 'amd64'),
                         ('aarch64', 'arm64')])
  def testSupportedArchs(self, mock_machine, arch, expected):
    mock_machine.return_value = arch
    self.assertEqual(_GetHostArch(), expected)

  def testUnsupportedArch(self, mock_machine):
    mock_machine.return_value = 'bad_arch'
    with self.assertRaises(Exception):
      _GetHostArch()


@mock.patch('builtins.open')
@mock.patch('os.path.isfile')
class TestGetSDKOverrideGCSPath(unittest.TestCase):
  def testFileNotFound(self, mock_isfile, mock_open):
    mock_isfile.return_value = False

    actual = GetSDKOverrideGCSPath('this-file-does-not-exist.txt')
    self.assertIsNone(actual)

  def testDefaultPath(self, mock_isfile, mock_open):
    mock_isfile.return_value = False

    with mock.patch('os.path.dirname', return_value='./'):
      GetSDKOverrideGCSPath()

    mock_isfile.assert_called_with('./sdk_override.txt')

  def testRead(self, mock_isfile, mock_open):
    fake_path = '\n\ngs://fuchsia-artifacts/development/abc123/sdk\n\n'

    mock_isfile.return_value = True
    mock_open.side_effect = mock.mock_open(read_data=fake_path)

    actual = GetSDKOverrideGCSPath()
    self.assertEqual(actual, 'gs://fuchsia-artifacts/development/abc123/sdk')


@mock.patch('update_sdk._GetHostArch')
@mock.patch('update_sdk.get_host_os')
class TestGetTarballPath(unittest.TestCase):
  def testGetTarballPath(self, mock_get_host_os, mock_host_arch):
    mock_get_host_os.return_value = 'linux'
    mock_host_arch.return_value = 'amd64'

    actual = _GetTarballPath('gs://bucket/sdk')
    self.assertEqual(actual, 'gs://bucket/sdk/linux-amd64/core.tar.gz')


if __name__ == '__main__':
  unittest.main()
