# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implements commands for running webpage tests."""

import os
import subprocess
import time

from contextlib import suppress
from typing import List, Optional, Tuple

import browser_runner
from common import catch_sigterm, get_ip_address, wait_for_sigterm
from test_runner import TestRunner

_DEVTOOLS_PORT_FILE = 'webpage_test_runner.devtools.port'


def capture_devtools_addr(proc: subprocess.Popen,
                          logs_dir: str) -> Tuple[str, int]:
    """Returns the devtools address and port initiated by the running |proc|.
    This function should only be used when the WebpageTestRunner is executed by
    a different process."""
    port_file = os.path.join(logs_dir, _DEVTOOLS_PORT_FILE)

    def try_reading_port():
        if not os.path.isfile(port_file):
            return None
        with open(port_file, encoding='utf-8') as inp:
            return inp.read().rsplit(':', 1)

    while True:
        result = try_reading_port()
        if result:
            return result
        proc.poll()
        assert not proc.returncode, 'Process stopped.'
        time.sleep(1)


class WebpageTestRunner(TestRunner):
    """Test runner for running GPU tests."""

    def __init__(self, out_dir: str, test_args: List[str],
                 target_id: Optional[str], logs_dir: Optional[str]) -> None:
        super().__init__(out_dir, test_args, ['web_engine_shell'], target_id)
        self._runner = browser_runner.BrowserRunner(
            browser_runner.WEB_ENGINE_SHELL, target_id, out_dir)
        if logs_dir:
            self.port_file = os.path.join(logs_dir, _DEVTOOLS_PORT_FILE)
        else:
            self.port_file = None

    def run_test(self):
        catch_sigterm()
        self._runner.start()
        device_ip = get_ip_address(self._target_id, ipv4_only=True)
        addr = device_ip.exploded
        if device_ip.version == 6:
            addr = '[' + addr + ']'
        addr += ':' + str(self._runner.devtools_port)
        if self.port_file:
            with open(self.port_file, 'w') as out:
                out.write(addr)
        else:
            print('DevTools is running on ' + addr)
        try:
            wait_for_sigterm('shutting down the webpage.')
        finally:
            self._runner.close()
            if self.port_file:
                with suppress(OSError):
                    os.remove(self.port_file)
        return subprocess.CompletedProcess(args='', returncode=0)
