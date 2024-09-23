# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Wrapper of ChromeDriver binary."""

import os

from contextlib import AbstractContextManager
from typing import Tuple

# From vpython wheel.
# pylint: disable=import-error
from selenium import webdriver
from selenium.webdriver import ChromeOptions
from selenium.webdriver.chrome.service import Service
# pylint: enable=import-error

from common import get_free_local_port


class ChromeDriverWrapper(AbstractContextManager):
    """Manages chromedriver and communicates with the web-engine via
    chromedriver on the device. This class expects the chromedriver exists at
    clang_x64/stripped/chromedriver in output dir."""

    def __init__(self, target: Tuple[str, int]):
        # The device / target to run the commands webdriver against.
        self._target = target
        # The reference of the webdriver.Chrome instance.
        self._driver = None
        # The port which chromedriver is listening to.
        self._port = 0

    def __enter__(self):
        """Starts the chromedriver, must be executed before other commands."""
        self._port = get_free_local_port()
        options = ChromeOptions()
        options.debugger_address = f'{self._target[0]}:{str(self._target[1])}'
        self._driver = webdriver.Chrome(options=options,
                                        service=Service(
                                            os.path.join(
                                                'clang_x64', 'stripped',
                                                'chromedriver'), self._port))
        self._driver.__enter__()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> bool:
        """Stops the chromedriver, cannot perform other commands afterward."""
        return self._driver.__exit__(exc_type, exc_val, exc_tb)

    def get(self, url: str):
        """Forwards to webdriver.get to open a page."""
        return self._driver.get(url)
