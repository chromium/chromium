# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implements commands for running webpage tests."""

import argparse
import logging
import time

from typing import List, Optional

from common import run_continuous_ffx_command
from test_runner import TestRunner


class WebpageTestRunner(TestRunner):
    """Test runner for running GPU tests."""

    def __init__(self, out_dir: str, test_args: List[str],
                 target_id: Optional[str]) -> None:
        parser = argparse.ArgumentParser()
        parser.add_argument(
            '--browser',
            choices=['web-engine-shell', 'chrome'],
            help='The browser to use for Telemetry based tests.')
        args, _ = parser.parse_known_args(test_args)

        if args.browser == 'web-engine-shell':
            packages = ['web_engine_shell']
        else:
            packages = ['chrome']

        super().__init__(out_dir, test_args, packages, target_id)

    def run_test(self):
        browser_cmd = [
            'test',
            'run',
            '-t',
            '3600',  # Keep the webpage running for an hour.
            f'fuchsia-pkg://fuchsia.com/{self._packages[0]}#meta/'
            f'{self._packages[0]}.cm'
        ]
        browser_cmd.extend([
            '--', '--web-engine-package-name=web_engine_with_webui',
            '--use-web-instance', '--enable-web-instance-tmp', '--with-webui'
        ])
        if self._test_args:
            browser_cmd.extend(self._test_args)
        logging.info('Starting %s', self._packages[0])
        try:
            browser_proc = run_continuous_ffx_command(browser_cmd)
            while True:
                time.sleep(10000)
        except KeyboardInterrupt:
            logging.info('Ctrl-C received; shutting down the webpage.')
            browser_proc.kill()
        except SystemExit:
            logging.info('SIGTERM received; shutting down the webpage.')
            browser_proc.kill()
        return browser_proc
