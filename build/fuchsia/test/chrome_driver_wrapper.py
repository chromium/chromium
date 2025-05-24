# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" Starts a web engine shell on an existing fuchsia device, and returns a
    ChromeDriver instance to control it."""

import logging
import os
import subprocess

from contextlib import AbstractContextManager
from typing import List

# From vpython wheel.
# pylint: disable=import-error
from selenium import webdriver
from selenium.webdriver import ChromeOptions
from selenium.webdriver.chrome.service import Service
from selenium.webdriver.common.by import By
# pylint: enable=import-error

from common import get_ffx_isolate_dir, get_free_local_port
from isolate_daemon import IsolateDaemon
from run_webpage_test import capture_devtools_addr

LOG_DIR = os.environ.get('ISOLATED_OUTDIR', '/tmp')


class ChromeDriverWrapper(AbstractContextManager):
    """Manages the web engine shell on the device and the chromedriver
    communicating with it. This class expects the chromedriver exists at
    clang_x64/stripped/chromedriver in output dir."""

    def __init__(self, extra_args: List[str] = None):
        # The reference of the webdriver.Chrome instance.
        self._driver = None

        # Creates the isolate dir for daemon to ensure it can be shared across
        # the processes. Note, it has no effect if isolate_dir has already been
        # set.
        self._isolate_dir = IsolateDaemon.IsolateDir()

        # The process of the run_test.py webpage.
        self._proc: subprocess.Popen = None

        # Extra arguments sent to run_test.py webpage process.
        self._extra_args = extra_args or []

    def __enter__(self):
        """Starts the run_test.py and the chromedriver connecting to it, must be
        executed before other commands."""
        self._isolate_dir.__enter__()
        logging.warning('ffx daemon is running in %s', get_ffx_isolate_dir())

        self._proc = subprocess.Popen([
            os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         'run_test.py'), 'webpage', '--out-dir=.',
            '--browser=web-engine-shell', '--device', f'--logs-dir={LOG_DIR}'
        ] + self._extra_args,
                                      env={
                                          **os.environ, 'CHROME_HEADLESS': '1'
                                      })
        address, port = capture_devtools_addr(self._proc, LOG_DIR)
        logging.warning('DevTools is now running on %s:%s', address, port)

        options = ChromeOptions()
        options.debugger_address = f'{address}:{str(port)}'
        # The port webdriver running on is not very interesting, the _driver
        # instance will be used directly. So a random free local port is used.
        self._driver = webdriver.Chrome(options=options,
                                        service=Service(
                                            os.path.join(
                                                'clang_x64', 'stripped',
                                                'chromedriver'),
                                            get_free_local_port()))
        self._driver.__enter__()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> bool:
        """Stops the run_test.py and the chromedriver, cannot perform other
        commands afterward."""
        try:
            return self._driver.__exit__(exc_type, exc_val, exc_tb)
        finally:
            self._proc.terminate()
            self._proc.wait()
            self._isolate_dir.__exit__(exc_type, exc_val, exc_tb)

    def __getattr__(self, name):
        """Forwards function calls to the underlying |_driver| instance."""
        return getattr(self._driver, name)

    # Explicitly override find_element_by_id to avoid importing selenium
    # packages in the caller files.
    # The find_element_by_id in webdriver.Chrome is deprecated.
    #   DeprecationWarning: find_element_by_* commands are deprecated. Please
    #   use find_element() instead
    def find_element_by_id(self, id_str):
        """Returns the element in the page with id |id_str|."""
        return self._driver.find_element(By.ID, id_str)
