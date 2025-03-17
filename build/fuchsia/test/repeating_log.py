# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" Writes logs to indicate a long-run process to avoid being killed by luci.
"""

import logging

from contextlib import AbstractContextManager
from threading import Timer


class RepeatingLog(AbstractContextManager):
    """Starts writing the log once a while after the initial wait. Luci swarming
       considers a long run job being dead if it does not output for a while.
       It's an issue for tasks like video analysis which may take up to several
       minutes. So this is a simple way to workaround the restriction."""

    def __init__(self, msg: str):
        self.msg = msg
        self.counter = 0
        self.timer = None

    def __enter__(self):
        self._schedule()

    def _schedule(self) -> None:
        self.timer = Timer(15, self._log_and_schedule)
        self.timer.start()

    def _log_and_schedule(self) -> None:
        self.counter += 1
        # Use warning to avoid being ignored by the log-level.
        logging.warning('[After %s seconds] - %s', self.counter * 15, self.msg)
        self._schedule()

    def __exit__(self, exc_type, exc_value, traceback):
        if self.timer:
            self.timer.cancel()
