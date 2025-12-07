#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing log_manager.py."""

import sys
import unittest
import unittest.mock as mock

import log_manager

_LOGS_DIR = 'test_logs_dir'


class LogManagerTest(unittest.TestCase):
    """Unittests for log_manager.py."""

    @mock.patch('log_manager.run_continuous_ffx_command')
    def test_no_logs(self, mock_ffx) -> None:
        """Test |start_system_log| does nothing when logging is off."""

        log = log_manager.LogManager(None)
        log_manager.start_system_log(log, False)
        self.assertEqual(mock_ffx.call_count, 0)

    @mock.patch('log_manager.run_continuous_ffx_command')
    def test_log_to_stdout(self, mock_ffx) -> None:
        """Test |start_system_log| logs to stdout when log manager is off."""

        log = log_manager.LogManager(None)
        log_manager.start_system_log(log, True)
        self.assertEqual(mock_ffx.call_args_list[0][1]['stdout'], sys.stdout)
        self.assertEqual(mock_ffx.call_count, 1)

    @mock.patch('log_manager.run_continuous_ffx_command')
    @mock.patch('builtins.open')
    def test_log_to_file(self, mock_open, mock_ffx) -> None:
        """Test |start_system_log| logs to log file when log manager is on."""

        log = log_manager.LogManager(_LOGS_DIR)
        log_manager.start_system_log(log, False)
        self.assertEqual(mock_ffx.call_args_list[0][1]['stdout'],
                         mock_open.return_value)
        self.assertEqual(mock_ffx.call_count, 1)

    @mock.patch('log_manager.run_continuous_ffx_command')
    def test_log_with_log_args(self, mock_ffx) -> None:
        """Test log args are used when passed in to |start_system_log|."""

        log = log_manager.LogManager(None)
        log_manager.start_system_log(log, True, log_args=['test_log_args'])
        self.assertEqual(
            mock_ffx.call_args_list[0][0][0],
            ['log', '--symbolize', 'off', '--no-color', 'test_log_args'])
        self.assertEqual(mock_ffx.call_count, 1)

    @mock.patch('log_manager.run_continuous_ffx_command')
    def test_log_with_symbols(self, mock_ffx) -> None:
        """Test symbols are used when pkg_paths are set."""

        with mock.patch('os.path.isfile', return_value=True), \
             mock.patch('builtins.open'), \
             mock.patch('log_manager.run_symbolizer'), \
             log_manager.LogManager(_LOGS_DIR) as log:
            log_manager.start_system_log(log, False, pkg_paths=['test_pkg'])
        self.assertEqual(mock_ffx.call_count, 1)
        self.assertEqual(mock_ffx.call_args_list[0][0][0],
                         ['log', '--symbolize', 'off', '--no-color'])

    def test_no_logging_dir_exception(self) -> None:
        """Tests empty LogManager throws an exception on |open_log_file|."""

        log = log_manager.LogManager(None)
        with self.assertRaises(Exception):
            log.open_log_file('test_log_file')


if __name__ == '__main__':
    unittest.main()
