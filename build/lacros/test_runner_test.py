#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
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

  @mock.patch.object(os.path,
                     'dirname',
                     return_value='chromium/src/build/lacros')
  def test_expand_filter_file(self, _):
    args = ['--some_flag="flag"']
    test_runner._ExpandFilterFileIfNeeded('browser_tests', args)
    self.assertTrue(args[1].endswith(
        'chromium/src/'
        'testing/buildbot/filters/linux-lacros.browser_tests.filter'))
    self.assertTrue(args[1].startswith('--test-launcher-filter-file='))

    args = ['--some_flag="flag"']
    test_runner._ExpandFilterFileIfNeeded('random_tests', args)
    self.assertEqual(len(args), 1)

    args = ['--test-launcher-filter-file=new/filter']
    test_runner._ExpandFilterFileIfNeeded('browser_tests', args)
    self.assertEqual(len(args), 1)
    self.assertTrue(args[0].endswith('new/filter'))

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
      'browser_tests', 'components_browsertests', 'content_browsertests',
      'browser_tests --enable-pixel-output-in-tests'
  ])
  @mock.patch.object(os,
                     'listdir',
                     return_value=['wayland-exo', 'wayland-exo.lock'])
  @mock.patch.object(tempfile,
                     'mkdtemp',
                     side_effect=['/tmp/xdg', '/tmp/ash-data', '/tmp/unique'])
  @mock.patch.object(os.environ, 'copy', side_effect=[{}, {}])
  @mock.patch.object(os.path, 'exists', return_value=True)
  @mock.patch.object(os.path, 'isfile', return_value=True)
  @mock.patch.object(os.path, 'abspath', return_value='/a/b/filter')
  @mock.patch.object(test_runner,
                     '_GetLatestVersionOfAshChrome',
                     return_value='793554')
  @mock.patch.object(test_runner, '_DownloadAshChromeIfNecessary')
  @mock.patch.object(subprocess, 'Popen', return_value=mock.Mock())
  # Tests that the test runner downloads and spawns ash-chrome if ash-chrome is
  # required.
  def test_require_ash_chrome(self, command, mock_popen, mock_download, *_):
    command_parts = command.split()
    args = ['script_name', 'test']
    args.extend(command_parts)
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
          '--disable-input-event-activation-protection',
          '--disable-lacros-keep-alive',
          '--disable-login-lacros-opening',
          '--enable-field-trial-config',
          '--enable-logging=stderr',
          '--enable-features=LacrosSupport,LacrosPrimary,LacrosOnly',
          '--enable-lacros-for-testing',
          '--ash-ready-file-path=/tmp/ash-data/ash_ready.txt',
          '--wayland-server-socket=wayland-exo',
      ]
      if '--enable-pixel-output-in-tests' not in command_parts:
        expected_ash_chrome_args.append('--disable-gl-drawing-for-tests')
      self.assertListEqual(expected_ash_chrome_args, ash_chrome_args[1:])
      ash_chrome_env = mock_popen.call_args_list[0][1].get('env', {})
      self.assertDictEqual({'XDG_RUNTIME_DIR': '/tmp/xdg'}, ash_chrome_env)

      test_args = mock_popen.call_args_list[1][0][0]
      self.assertListEqual(test_args[:len(command_parts)], command_parts)

      test_env = mock_popen.call_args_list[1][1].get('env', {})
      self.assertDictEqual(
          {
              'WAYLAND_DISPLAY': 'wayland-exo',
              'XDG_RUNTIME_DIR': '/tmp/xdg',
              'EGL_PLATFORM': 'surfaceless'
          }, test_env)

  @mock.patch.object(os,
                     'listdir',
                     return_value=['wayland-exo', 'wayland-exo.lock'])
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
                     return_value=['wayland-exo', 'wayland-exo.lock'])
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
                     return_value=['wayland-exo', 'wayland-exo.lock'])
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
                     return_value=['wayland-exo', 'wayland-exo.lock'])
  @mock.patch.object(tempfile,
                     'mkdtemp',
                     side_effect=['/tmp/xdg', '/tmp/ash-data', '/tmp/unique'])
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


  # Test when ash is newer, test runner skips running tests and returns 0.
  @mock.patch.object(os.path, 'exists', return_value=True)
  @mock.patch.object(os.path, 'isfile', return_value=True)
  @mock.patch.object(test_runner, '_FindLacrosMajorVersion', return_value=91)
  def test_version_skew_ash_newer(self, *_):
    args = [
        'script_name', 'test', './browser_tests', '--gtest_filter=Suite.Test',
        '--ash-chrome-path-override=\
lacros_version_skew_tests_v92.0.100.0/test_ash_chrome'
    ]
    with mock.patch.object(sys, 'argv', args):
      self.assertEqual(test_runner.Main(), 0)

  @mock.patch.object(os.path, 'exists', return_value=True)
  def test_lacros_version_from_chrome_version(self, *_):
    version_data = '''\
MAJOR=95
MINOR=0
BUILD=4615
PATCH=0\
'''
    open_lib = '__builtin__.open'
    if sys.version_info[0] >= 3:
      open_lib = 'builtins.open'
    with mock.patch(open_lib,
                    mock.mock_open(read_data=version_data)) as mock_file:
      version = test_runner._FindLacrosMajorVersion()
      self.assertEqual(95, version)

  @mock.patch.object(os.path, 'exists', return_value=True)
  def test_lacros_version_from_metadata(self, *_):
    metadata_json = '''
{
  "content": {
    "version": "92.1.4389.2"
  },
  "metadata_version": 1
}
    '''
    open_lib = '__builtin__.open'
    if sys.version_info[0] >= 3:
      open_lib = 'builtins.open'
    with mock.patch(open_lib,
                    mock.mock_open(read_data=metadata_json)) as mock_file:
      version = test_runner._FindLacrosMajorVersionFromMetadata()
      self.assertEqual(92, version)
      mock_file.assert_called_with('metadata.json', 'r')


if __name__ == '__main__':
  unittest.main()
