#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Reads log data from a device."""

import argparse
import os
import subprocess
import sys
import time

from contextlib import AbstractContextManager
from typing import Iterable, Optional, TextIO

from common import SDK_ROOT, read_package_paths, register_common_args, \
                   register_device_args, run_continuous_ffx_command, \
                   run_ffx_command
from ffx_integration import ScopedFfxConfig


class LogManager(AbstractContextManager):
    """Handles opening and closing file streams for logging purposes."""

    def __init__(self, logs_dir: Optional[str]) -> None:
        self._logs_dir = logs_dir

        # A dictionary with the log file path as the key and a file stream as
        # value.
        self._log_files = {}
        self._log_procs = []
        self._scoped_ffx_log = None

        if self._logs_dir:
            self._scoped_ffx_log = ScopedFfxConfig('log.dir', self._logs_dir)

    def __enter__(self):
        if self._scoped_ffx_log:
            self._scoped_ffx_log.__enter__()
            run_ffx_command(('daemon', 'stop'), check=False)

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

        if not self._logs_dir:
            raise Exception('Logging directory is not specified.')
        log_file_path = os.path.join(self._logs_dir, log_file_name)
        log_file = open(log_file_path, 'w', buffering=1)
        self._log_files[log_file_path] = log_file
        return log_file

    def stop(self):
        """Stop all active logging instances."""

        for proc in self._log_procs:
            proc.kill()
        for log in self._log_files.values():
            log.close()

    def __exit__(self, exc_type, exc_value, traceback):
        self.stop()
        if self._scoped_ffx_log:
            self._scoped_ffx_log.__exit__(exc_type, exc_value, traceback)

            # Allow command to fail while ffx team investigates the issue.
            run_ffx_command(('daemon', 'stop'), check=False)


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
    log_cmd = ['log', '--no-symbols']
    if log_args:
        log_cmd.extend(log_args)
    if symbol_paths:
        log_proc = run_continuous_ffx_command(log_cmd,
                                              target_id,
                                              stdout=subprocess.PIPE)
        log_manager.add_log_process(log_proc)
        symbolize_cmd = ([
            'debug', 'symbolize', '--', '--omit-module-lines',
            '--build-id-dir',
            os.path.join(SDK_ROOT, '.build-id')
        ])
        for symbol_path in symbol_paths:
            symbolize_cmd.extend(['--ids-txt', symbol_path])
        log_manager.add_log_process(
            run_continuous_ffx_command(symbolize_cmd,
                                       target_id,
                                       stdin=log_proc.stdout,
                                       stdout=system_log,
                                       stderr=subprocess.STDOUT))
    else:
        log_manager.add_log_process(
            run_continuous_ffx_command(log_cmd, target_id, stdout=system_log))


def main():
    """Stand-alone function for fetching system logs and print to terminal.
    Runs until the process is killed or interrupted (i.e. user presses CTRL-C).
    """

    parser = argparse.ArgumentParser()
    register_common_args(parser)
    register_device_args(parser)
    parser.add_argument('--packages',
                        action='append',
                        help='Name of the packages to symbolize.')
    manager_args, system_log_args = parser.parse_known_args()
    if manager_args.packages and not manager_args.out_dir:
        raise ValueError('--out-dir must be specified to symbolize packages.')
    package_paths = []
    if manager_args.packages:
        for package in manager_args.packages:
            package_paths.extend(
                read_package_paths(manager_args.out_dir, package))
    with LogManager(None) as log_manager:
        try:
            start_system_log(log_manager, True, package_paths, system_log_args,
                             manager_args.target_id)
            while True:
                time.sleep(10000)
        except (KeyboardInterrupt, SystemExit):
            pass


if __name__ == '__main__':
    sys.exit(main())
