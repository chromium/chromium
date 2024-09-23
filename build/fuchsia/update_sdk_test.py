#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys
import unittest
from unittest import mock

from parameterized import parameterized
from subprocess import CompletedProcess

from update_sdk import _GetHostArch
from update_sdk import GetSDKOverrideGCSPath
from update_sdk import main as update_sdk_main

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__),
                                             'test')))

from common import SDK_ROOT


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


@mock.patch('update_sdk._GetHostArch', return_value='amd64')
@mock.patch('update_sdk.get_host_os', return_value='linux')
@mock.patch('subprocess.run',
            return_value=CompletedProcess(args=['/bin'], returncode=0))
@mock.patch('os.utime', return_value=None)
@mock.patch('update_sdk.make_clean_directory')
@mock.patch('update_sdk.DownloadAndUnpackFromCloudStorage')
class TestGetTarballPath(unittest.TestCase):

  def setUp(self):
    os.environ['FUCHSIA_SDK_OVERRIDE'] = 'gs://bucket/sdk'

  def tearDown(self):
    del os.environ['FUCHSIA_SDK_OVERRIDE']

  @mock.patch('argparse.ArgumentParser.parse_args',
              return_value=argparse.Namespace(version='1.1.1.1',
                                              verbose=False,
                                              file='core'))
  def testGetTarballPath(self, mock_arg, mock_download, *_):
    update_sdk_main()
    mock_download.assert_called_with('gs://bucket/sdk/linux-amd64/core.tar.gz',
                                     SDK_ROOT)

  @mock.patch('argparse.ArgumentParser.parse_args',
              return_value=argparse.Namespace(version='1.1.1.1',
                                              verbose=False,
                                              file='google'))
  def testOverrideFile(self, mock_arg, mock_download, *_):
    update_sdk_main()
    mock_download.assert_called_with(
        'gs://bucket/sdk/linux-amd64/google.tar.gz', SDK_ROOT)


if __name__ == '__main__':
  unittest.main()
