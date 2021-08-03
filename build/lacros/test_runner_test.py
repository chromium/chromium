#!/usr/bin/env vpython
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import subprocess
import sys
import tempfile
import time
import unittest

import mock
from parameterized import parameterized

import test_runner


class TestRunnerTest(unittest.TestCase):
  def setUp(self):
    logging.disable(logging.CRITICAL)
    time.sleep = mock.Mock()

  def tearDown(self):
    logging.disable(logging.NOTSET)

  @parameterized.expand([
      'url_unittests',
      './url_unittests',
      'out/release/url_unittests',
      './out/release/url_unittests',
  ])
  @mock.patch.object(os.path, 'isfile', return_value=True)
  @mock.patch.object(test_runner, '_DownloadAshChromeIfNecessary')
  @mock.patch.object(subprocess, 'Popen', return_value=mock.Mock())
  # Tests that the test runner doesn't attempt to download ash-chrome if not
  # required.
  def test_do_not_require_ash_chrome(self, command, mock_popen, mock_download,
                                     _):
    args = ['script_name', 'test', command]
    with mock.patch.object(sys, 'argv', args):
      test_runner.Main()
      self.assertEqual(1, mock_popen.call_count)
      mock_popen.assert_called_with([command])
      self.assertFalse(mock_download.called)

  @parameterized.expand([
      'browser_tests',
      'components_browsertests',
      'content_browsertests',
      'lacros_chrome_browsertests',
  ])
  @mock.patch.object(os,
                     'listdir',
                     return_value=['wayland-0', 'wayland-0.lock'])
  @mock.patch.object(tempfile,
                     'mkdtemp',
                     side_effect=['/tmp/xdg', '/tmp/ash-data'])
  @mock.patch.object(os.environ, 'copy', side_effect=[{}, {}])
  @mock.patch.object(os.path, 'exists', return_value=True)
  @mock.patch.object(os.path, 'isfile', return_value=True)
  @mock.patch.object(test_runner,
                     '_GetLatestVersionOfAshChrome',
                     return_value='793554')
  @mock.patch.object(test_runner, '_DownloadAshChromeIfNecessary')
  @mock.patch.object(subprocess, 'Popen', return_value=mock.Mock())
  # Tests that the test runner downloads and spawns ash-chrome if ash-chrome is
  # required.
  def test_require_ash_chrome(self, command, mock_popen, mock_download, *_):
    args = ['script_name', 'test', command]
    with mock.patch.object(sys, 'argv', args):
      test_runner.Main()
      mock_download.assert_called_with('793554')
      self.assertEqual(2, mock_popen.call_count)

      ash_chrome_args = mock_popen.call_args_list[0][0][0]
      self.assertTrue(ash_chrome_args[0].endswith(
          'build/lacros/prebuilt_ash_chrome/793554/test_ash_chrome'))
      expected_ash_chrome_args = [
          '--user-data-dir=/tmp/ash-data',
          '--enable-wayland-server',
          '--no-startup-window',
      ]
      if command == 'lacros_chrome_browsertests':
        expected_ash_chrome_args.append(
            '--lacros-mojo-socket-for-testing=/tmp/ash-data/lacros.sock')
      self.assertListEqual(expected_ash_chrome_args, ash_chrome_args[1:])
      ash_chrome_env = mock_popen.call_args_list[0][1].get('env', {})
      self.assertDictEqual({'XDG_RUNTIME_DIR': '/tmp/xdg'}, ash_chrome_env)

      test_args = mock_popen.call_args_list[1][0][0]
      if command == 'lacros_chrome_browsertests':
        self.assertListEqual([
            command,
            '--lacros-mojo-socket-for-testing=/tmp/ash-data/lacros.sock'
        ], test_args)
      else:
        self.assertListEqual([command], test_args)

      test_env = mock_popen.call_args_list[1][1].get('env', {})
      self.assertDictEqual(
          {
              'XDG_RUNTIME_DIR': '/tmp/xdg',
              'EGL_PLATFORM': 'surfaceless'
          }, test_env)


  @mock.patch.object(os,
                     'listdir',
                     return_value=['wayland-0', 'wayland-0.lock'])
  @mock.patch.object(os.path, 'exists', return_value=True)
  @mock.patch.object(os.path, 'isfile', return_value=True)
  @mock.patch.object(test_runner,
                     '_GetLatestVersionOfAshChrome',
                     return_value='793554')
  @mock.patch.object(test_runner, '_DownloadAshChromeIfNecessary')
  @mock.patch.object(subprocess, 'Popen', return_value=mock.Mock())
  # Tests that when a ash-chrome version is specified, that version is used
  # instead of the latest one.
  def test_specify_ash_chrome_version(self, mock_popen, mock_download, *_):
    args = [
        'script_name', 'test', 'browser_tests', '--ash-chrome-version', '781122'
    ]
    with mock.patch.object(sys, 'argv', args):
      test_runner.Main()
      mock_download.assert_called_with('781122')

  @mock.patch.object(os,
                     'listdir',
                     return_value=['wayland-0', 'wayland-0.lock'])
  @mock.patch.object(os.path, 'exists', return_value=True)
  @mock.patch.object(os.path, 'isfile', return_value=True)
  @mock.patch.object(test_runner, '_DownloadAshChromeIfNecessary')
  @mock.patch.object(subprocess, 'Popen', return_value=mock.Mock())
  # Tests that if a ash-chrome version is specified, uses ash-chrome to run
  # tests anyway even if |_TARGETS_REQUIRE_ASH_CHROME| indicates an ash-chrome
  # is not required.
  def test_overrides_do_not_require_ash_chrome(self, mock_popen, mock_download,
                                               *_):
    args = [
        'script_name', 'test', './url_unittests', '--ash-chrome-version',
        '793554'
    ]
    with mock.patch.object(sys, 'argv', args):
      test_runner.Main()
      mock_download.assert_called_with('793554')
      self.assertEqual(2, mock_popen.call_count)

  @mock.patch.object(os,
                     'listdir',
                     return_value=['wayland-0', 'wayland-0.lock'])
  @mock.patch.object(os.path, 'exists', return_value=True)
  @mock.patch.object(os.path, 'isfile', return_value=True)
  @mock.patch.object(test_runner, '_GetLatestVersionOfAshChrome')
  @mock.patch.object(test_runner, '_DownloadAshChromeIfNecessary')
  @mock.patch.object(subprocess, 'Popen', return_value=mock.Mock())
  # Tests that when an ash-chrome path is specified, the test runner doesn't try
  # to download prebuilt ash-chrome.
  def test_specify_ash_chrome_path(self, mock_popen, mock_download,
                                   mock_get_latest_version, *_):
    args = [
        'script_name',
        'test',
        'browser_tests',
        '--ash-chrome-path',
        '/ash/test_ash_chrome',
    ]
    with mock.patch.object(sys, 'argv', args):
      test_runner.Main()
      self.assertFalse(mock_get_latest_version.called)
      self.assertFalse(mock_download.called)

  @mock.patch.object(os.path, 'isfile', return_value=True)
  @mock.patch.object(test_runner, '_DownloadAshChromeIfNecessary')
  @mock.patch.object(subprocess, 'Popen', return_value=mock.Mock())
  # Tests that arguments not known to the test runner are forwarded to the
  # command that invokes tests.
  def test_command_arguments(self, mock_popen, mock_download, _):
    args = [
        'script_name', 'test', './url_unittests', '--gtest_filter=Suite.Test'
    ]
    with mock.patch.object(sys, 'argv', args):
      test_runner.Main()
      mock_popen.assert_called_with(
          ['./url_unittests', '--gtest_filter=Suite.Test'])
      self.assertFalse(mock_download.called)

  @mock.patch.dict(os.environ, {'ASH_WRAPPER': 'gdb --args'}, clear=False)
  @mock.patch.object(os,
                     'listdir',
                     return_value=['wayland-0', 'wayland-0.lock'])
  @mock.patch.object(tempfile,
                     'mkdtemp',
                     side_effect=['/tmp/xdg', '/tmp/ash-data'])
  @mock.patch.object(os.environ, 'copy', side_effect=[{}, {}])
  @mock.patch.object(os.path, 'exists', return_value=True)
  @mock.patch.object(os.path, 'isfile', return_value=True)
  @mock.patch.object(test_runner,
                     '_GetLatestVersionOfAshChrome',
                     return_value='793554')
  @mock.patch.object(test_runner, '_DownloadAshChromeIfNecessary')
  @mock.patch.object(subprocess, 'Popen', return_value=mock.Mock())
  # Tests that, when the ASH_WRAPPER environment variable is set, it forwards
  # the commands to the invocation of ash.
  def test_ash_wrapper(self, mock_popen, *_):
    args = [
        'script_name', 'test', './browser_tests', '--gtest_filter=Suite.Test'
    ]
    with mock.patch.object(sys, 'argv', args):
      test_runner.Main()
      ash_args = mock_popen.call_args_list[0][0][0]
      self.assertTrue(ash_args[2].endswith('test_ash_chrome'))
      self.assertEqual(['gdb', '--args'], ash_args[:2])


if __name__ == '__main__':
  unittest.main()
