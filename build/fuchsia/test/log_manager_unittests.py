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
        self.assertEqual(mock_ffx.call_args_list[0][0][0],
                         ['log', '--raw', 'test_log_args'])
        self.assertEqual(mock_ffx.call_count, 1)

    @mock.patch('log_manager.run_continuous_ffx_command')
    def test_log_with_symbols(self, mock_ffx) -> None:
        """Test symbols are used when pkg_paths are set."""

        log = log_manager.LogManager(_LOGS_DIR)
        with mock.patch('os.path.isfile', return_value=True), \
                mock.patch('builtins.open'), \
                mock.patch('log_manager.run_symbolizer'):
            log_manager.start_system_log(log, False, pkg_paths=['test_pkg'])
            log.stop()
        self.assertEqual(mock_ffx.call_count, 1)
        self.assertEqual(mock_ffx.call_args_list[0][0][0], ['log', '--raw'])

    def test_no_logging_dir_exception(self) -> None:
        """Tests empty LogManager throws an exception on |open_log_file|."""

        log = log_manager.LogManager(None)
        with self.assertRaises(Exception):
            log.open_log_file('test_log_file')

    @mock.patch('log_manager.ScopedFfxConfig')
    @mock.patch('log_manager.run_ffx_command')
    def test_log_manager(self, mock_ffx, mock_scoped_config) -> None:
        """Tests LogManager as a context manager."""

        context_mock = mock.Mock()
        mock_scoped_config.return_value = context_mock
        context_mock.__enter__ = mock.Mock(return_value=None)
        context_mock.__exit__ = mock.Mock(return_value=None)
        with log_manager.LogManager(_LOGS_DIR):
            pass
        self.assertEqual(mock_ffx.call_count, 2)

    def test_main_exception(self) -> None:
        """Tests |main| function to throw exception on incompatible flags."""

        with mock.patch('sys.argv',
                        ['log_manager.py', '--packages', 'test_package']):
            with self.assertRaises(ValueError):
                log_manager.main()

    @mock.patch('log_manager.read_package_paths')
    @mock.patch('log_manager.start_system_log')
    def test_main(self, mock_system_log, mock_read_paths) -> None:
        """Tests |main| function."""

        with mock.patch('sys.argv', [
                'log_manager.py', '--packages', 'test_package', '--out-dir',
                'test_out_dir'
        ]):
            with mock.patch('log_manager.time.sleep',
                            side_effect=KeyboardInterrupt):
                log_manager.main()
        self.assertEqual(mock_system_log.call_count, 1)
        self.assertEqual(mock_read_paths.call_count, 1)


if __name__ == '__main__':
    unittest.main()
