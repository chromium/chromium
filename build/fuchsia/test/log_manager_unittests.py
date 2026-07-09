#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing log_manager.py."""

import os
import shutil
import sys
import tempfile
import threading
import time
import unittest
import unittest.mock as mock

import log_manager

_LOGS_DIR = 'test_logs_dir'


class FakeClock:
    """A fake clock for mocking time.time and time.sleep."""

    def __init__(self, start_time: float = 1000.0) -> None:
        self.current_time = start_time

    def time(self) -> float:
        """Returns the fake current time."""
        return self.current_time

    def sleep(self, seconds: float) -> None:
        """Advances the fake time instead of sleeping."""
        self.current_time += seconds


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

    def test_wait_for_pattern_disabled(self) -> None:
        """Test _wait_for_pattern returns immediately if logs_dir is None."""
        log = log_manager.LogManager(None, wait_for_pattern="some pattern")
        # pylint: disable=protected-access
        log._wait_for_pattern()
        self.assertIsNotNone(log)

    def test_wait_for_pattern_no_file(self) -> None:
        """Test _wait_for_pattern returns immediately if file doesn't exist."""
        log = log_manager.LogManager('some_non_existent_dir',
                                     wait_for_pattern="some pattern")
        # pylint: disable=protected-access
        log._wait_for_pattern()
        self.assertIsNotNone(log)

    def test_exception_propagation(self) -> None:
        """Test exception in context is not suppressed by LogManager."""

        with self.assertRaises(RuntimeError):
            with log_manager.LogManager(None):
                raise RuntimeError('Test exception')

    def test_context_manager_waits_for_pattern(self) -> None:
        """Test LogManager calls _wait_for_pattern on exit if pattern is set."""
        tmp_dir = tempfile.mkdtemp()
        try:
            log = log_manager.LogManager(tmp_dir,
                                         wait_for_pattern="some pattern")
            with mock.patch.object(log, '_wait_for_pattern') as mock_wait:
                with log:
                    pass
                self.assertEqual(mock_wait.call_count, 1)
        finally:
            shutil.rmtree(tmp_dir)

    def test_context_manager_propagates_exception_in_wait(self) -> None:
        """Test exception in _wait_for_pattern is propagated but cleanup
        still runs."""
        tmp_dir = tempfile.mkdtemp()
        try:
            log = log_manager.LogManager(tmp_dir,
                                         wait_for_pattern="some pattern")
            mock_proc = mock.Mock()
            log.add_log_process(mock_proc)

            with mock.patch.object(log,
                                   '_wait_for_pattern',
                                   side_effect=RuntimeError('Wait failed')):
                with self.assertRaises(RuntimeError):
                    with log:
                        pass
            mock_proc.kill.assert_called_once()
        finally:
            shutil.rmtree(tmp_dir)

    def test_wait_for_pattern_success(self) -> None:
        """Test _wait_for_pattern stops immediately when pattern is found."""
        # pylint: disable=protected-access
        tmp_dir = tempfile.mkdtemp()
        try:
            log = log_manager.LogManager(tmp_dir,
                                         wait_for_pattern="target pattern")
            system_log_path = os.path.join(tmp_dir, 'system_log')

            with open(system_log_path, 'w', encoding='utf-8') as f:
                f.write("initial line\n")

            ready_event = threading.Event()

            def writer():
                ready_event.wait()
                time.sleep(0.2)
                with open(system_log_path, 'a', encoding='utf-8') as f:
                    f.write("some random line\n")
                    f.flush()
                    time.sleep(0.2)
                    f.write("the target pattern is here\n")
                    f.flush()

            writer_thread = threading.Thread(target=writer)
            writer_thread.start()

            ready_event.set()
            start_time = time.time()
            log._wait_for_pattern()
            duration = time.time() - start_time

            self.assertGreaterEqual(duration, 0.35)
            self.assertLess(duration, 1.0)
            writer_thread.join()
        finally:
            shutil.rmtree(tmp_dir)

    def test_wait_for_pattern_split_line(self) -> None:
        """Test _wait_for_pattern reconstructs split lines successfully."""
        # pylint: disable=protected-access
        tmp_dir = tempfile.mkdtemp()
        try:
            log = log_manager.LogManager(
                tmp_dir, wait_for_pattern="partial line target")
            system_log_path = os.path.join(tmp_dir, 'system_log')

            with open(system_log_path, 'w', encoding='utf-8') as f:
                f.write("initial line\n")

            ready_event = threading.Event()

            def writer():
                ready_event.wait()
                time.sleep(0.2)
                with open(system_log_path, 'a', encoding='utf-8') as f:
                    # Write first part of the line without a newline
                    f.write("partial ")
                    f.flush()
                    time.sleep(0.2)
                    # Write second part of the line with a newline
                    f.write("line target\n")
                    f.flush()

            writer_thread = threading.Thread(target=writer)
            writer_thread.start()

            ready_event.set()
            start_time = time.time()
            log._wait_for_pattern()
            duration = time.time() - start_time

            self.assertGreaterEqual(duration, 0.35)
            self.assertLess(duration, 1.0)
            writer_thread.join()
        finally:
            shutil.rmtree(tmp_dir)

    def test_wait_for_pattern_timeout(self) -> None:
        """Test _wait_for_pattern times out if pattern is not found."""
        # pylint: disable=protected-access
        tmp_dir = tempfile.mkdtemp()
        try:
            log = log_manager.LogManager(
                tmp_dir, wait_for_pattern="non existent pattern")
            system_log_path = os.path.join(tmp_dir, 'system_log')

            with open(system_log_path, 'w', encoding='utf-8') as f:
                f.write("initial line\n")
                f.write("noise line\n")

            fake_clock = FakeClock()
            start_time = fake_clock.time()
            with mock.patch('time.time', side_effect=fake_clock.time), \
                 mock.patch('time.sleep', side_effect=fake_clock.sleep):
                log._wait_for_pattern()

            duration = fake_clock.time() - start_time
            self.assertGreaterEqual(duration, 30.0)
            self.assertLess(duration, 31.0)
        finally:
            shutil.rmtree(tmp_dir)


if __name__ == '__main__':
    unittest.main()
