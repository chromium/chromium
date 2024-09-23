# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests the connection of a target."""

import logging
import time

from typing import Optional

from boot_device import boot_device, BootMode
from common import run_ffx_command


def test_connection(target_id: Optional[str], wait_sec: int = 60) -> None:
    """Runs echo tests to verify that the device can be connected to.

    Devices may not be connectable right after being discovered by ffx, e.g.
    after a `ffx target wait`, so this function retries up to |wait_sec| before
    throwing an exception.
    """
    start_sec = time.time()
    while time.time() - start_sec < wait_sec:
        if run_ffx_command(cmd=('target', 'echo'),
                           target_id=target_id,
                           check=False).returncode == 0:
            return
        time.sleep(10)

    run_ffx_command(cmd=('target', 'echo'), target_id=target_id)


def test_device_connection(target_id: Optional[str]) -> None:
    """Runs test_connection against the target_id and restarts the device if
    it cannot be connected."""
    start_sec = time.time()
    while time.time() - start_sec < 1800:
        # pylint: disable=bare-except
        # First, test_connection with ffx target echo.
        try:
            test_connection(target_id=target_id, wait_sec=600)
            return
        except:
            # If anything wrong, reboot the device and try again.
            try:
                boot_device(target_id, BootMode.REGULAR, must_boot=True)
            except:
                # If unfortunately, the reboot failed, it's still worth
                # continuing the test rather than failing here.
                pass
            logging.warning(
                run_ffx_command(cmd=('target', 'wait'),
                                target_id=target_id,
                                check=False,
                                capture_output=True).stdout)
