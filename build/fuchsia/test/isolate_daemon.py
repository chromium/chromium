# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Sets up the isolate daemon environment to run test on the bots."""

import os
import tempfile

from typing import Optional

from contextlib import AbstractContextManager

from common import has_ffx_isolate_dir, set_ffx_isolate_dir, \
                   is_daemon_running, start_ffx_daemon, stop_ffx_daemon
from ffx_integration import ScopedFfxConfig
from modification_waiter import ModificationWaiter


class IsolateDaemon(AbstractContextManager):
    """Sets up the environment of an isolate ffx daemon."""

    class IsolateDir(AbstractContextManager):
        """Sets up the ffx isolate dir to a temporary folder if it's not set."""
        def __init__(self):
            if has_ffx_isolate_dir():
                self._temp_dir = None
            else:
                self._temp_dir = tempfile.TemporaryDirectory()

        def __enter__(self):
            if self._temp_dir:
                set_ffx_isolate_dir(self._temp_dir.__enter__())
            return self

        def __exit__(self, exc_type, exc_value, traceback):
            if self._temp_dir:
                try:
                    self._temp_dir.__exit__(exc_type, exc_value, traceback)
                except OSError:
                    # Ignore the errors when cleaning up the temporary folder.
                    pass
            return False

    def __init__(self, logs_dir: Optional[str]):
        assert not has_ffx_isolate_dir() or not is_daemon_running()
        self._inits = [
            self.IsolateDir(),
            ModificationWaiter(logs_dir),
            # Keep the alphabetical order.
            ScopedFfxConfig('ffx.isolated', 'true'),
            ScopedFfxConfig('daemon.autostart', 'false'),
            # fxb/126212: The timeout rate determines the timeout for each file
            # transfer based on the size of the file / this rate (in MB).
            # Decreasing the rate to 1 (from 5) increases the timeout in
            # swarming, where large files can take longer to transfer.
            ScopedFfxConfig('fastboot.flash.timeout_rate', '1'),
            ScopedFfxConfig('fastboot.reboot.reconnect_timeout', '120'),
            ScopedFfxConfig('fastboot.usb.disabled', 'true'),
            ScopedFfxConfig('log.level', 'debug'),
            ScopedFfxConfig('repository.server.listen', '"[::]:0"'),
        ]
        if logs_dir:
            self._inits.append(ScopedFfxConfig('log.dir', logs_dir))

    # Updating configurations to meet the requirement of isolate.
    def __enter__(self):
        # This environment variable needs to be set before stopping ffx daemon
        # to avoid sending unnecessary analytics.
        os.environ['FUCHSIA_ANALYTICS_DISABLED'] = '1'
        stop_ffx_daemon()
        for init in self._inits:
            init.__enter__()
        start_ffx_daemon()
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        for init in self._inits:
            init.__exit__(exc_type, exc_value, traceback)
        stop_ffx_daemon()
