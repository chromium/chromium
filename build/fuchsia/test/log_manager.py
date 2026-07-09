# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Reads log data from a device."""

import os
import subprocess
import logging
import re
import sys
import time

from contextlib import AbstractContextManager
from typing import Iterable, Optional, TextIO

from common import run_continuous_ffx_command
from ffx_integration import run_symbolizer


class LogManager(AbstractContextManager):
    """Handles opening and closing file streams for logging purposes."""

    def __init__(self,
                 logs_dir: Optional[str],
                 wait_for_pattern: Optional[str] = None) -> None:
        """
        Args:
            logs_dir: Directory where logs will be written.
            wait_for_pattern: A regex pattern to wait for before exiting the
                context manager. Note: The pattern cannot contain newlines or
                span across multiple lines, as the log stream is processed and
                cleared line-by-line.
        """
        self._logs_dir = logs_dir
        self._wait_for_log_pattern = wait_for_pattern

        # A dictionary with the log file path as the key and a file stream as
        # value.
        self._log_files = {}
        self._log_procs = []
        self._scoped_ffx_log = None

    def __enter__(self):
        return self

    def is_logging_enabled(self) -> bool:
        """Check whether logging is turned on."""

        return self._logs_dir is not None

    def add_log_process(self, process: subprocess.Popen) -> None:
        """Register a logging process to LogManager to be killed at LogManager
        teardown."""

        self._log_procs.append(process)

    def open_log_file(self, log_file_name: str) -> TextIO:
        """Open a file stream with log_file_name in the logs directory."""

        assert self._logs_dir, 'Logging directory is not specified.'
        log_file_path = os.path.join(self._logs_dir, log_file_name)
        log_file = open(log_file_path, 'w', buffering=1)
        self._log_files[log_file_path] = log_file
        return log_file

    def _wait_for_pattern(self) -> None:
        """Wait until pattern appears in system_log."""
        if not self._wait_for_log_pattern:
            return
        if not self._logs_dir:
            return
        system_log_path = os.path.join(self._logs_dir, 'system_log')
        if not os.path.exists(system_log_path):
            return

        logging.info('Waiting for pattern "%s" in system log...',
                     self._wait_for_log_pattern)
        compiled_pattern = re.compile(self._wait_for_log_pattern)
        start_time = time.time()
        # Read in text mode with line buffering. We accumulate data into
        # pending_data to handle cases where readline() returns a partial line
        # (i.e. does not end with a newline) due to concurrent non-atomic
        # writes.
        pending_data = ''
        with open(system_log_path,
                  'r',
                  buffering=1,
                  encoding='utf-8',
                  errors='ignore') as f:
            while time.time() - start_time < 30.0:
                line = f.readline()
                if line:
                    pending_data += line
                    if compiled_pattern.search(pending_data):
                        logging.info('Found pattern "%s" in system log.',
                                     self._wait_for_log_pattern)
                        return
                    if pending_data.endswith('\n'):
                        pending_data = ''
                else:
                    time.sleep(0.1)
            logging.warning('Pattern "%s" not found in system log within 30s.',
                            self._wait_for_log_pattern)

    def __exit__(self, exc_type, exc_value, traceback):
        """Stop all active logging instances."""
        try:
            if self.is_logging_enabled():
                self._wait_for_pattern()
        finally:
            for proc in self._log_procs:
                proc.kill()
            for log in self._log_files.values():
                log.close()
        return False


def start_system_log(log_manager: LogManager,
                     log_to_stdout: bool,
                     pkg_paths: Optional[Iterable[str]] = None,
                     log_args: Optional[Iterable[str]] = None,
                     target_id: Optional[str] = None) -> None:
    """
    Start system logging.

    Args:
        log_manager: A LogManager class that manages the log file and process.
        log_to_stdout: If set to True, print logs directly to stdout.
        pkg_paths: Path to the packages
        log_args: Arguments forwarded to `ffx log` command.
        target_id: Specify a target to use.
    """

    if not log_manager.is_logging_enabled() and not log_to_stdout:
        return
    symbol_paths = None
    if pkg_paths:
        symbol_paths = []

        # Locate debug symbols for each package.
        for pkg_path in pkg_paths:
            assert os.path.isfile(pkg_path), '%s does not exist' % pkg_path
            symbol_paths.append(
                os.path.join(os.path.dirname(pkg_path), 'ids.txt'))

    if log_to_stdout:
        system_log = sys.stdout
    else:
        system_log = log_manager.open_log_file('system_log')
    log_cmd = ['log', '--symbolize', 'off', '--no-color']
    if log_args:
        log_cmd.extend(log_args)
    if symbol_paths:
        log_proc = run_continuous_ffx_command(log_cmd,
                                              target_id,
                                              stdout=subprocess.PIPE)
        log_manager.add_log_process(log_proc)
        log_manager.add_log_process(
            run_symbolizer(symbol_paths, log_proc.stdout, system_log))
    else:
        log_manager.add_log_process(
            run_continuous_ffx_command(log_cmd, target_id, stdout=system_log))
