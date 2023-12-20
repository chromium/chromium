# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Functionalities to reliably reboot the device."""

import enum

from typing import Optional

class BootMode(enum.Enum):
    """Specifies boot mode for device."""
    REGULAR = enum.auto()
    RECOVERY = enum.auto()
    BOOTLOADER = enum.auto()


class StateTransitionError(Exception):
    """Raised when target does not transition to desired state."""


def boot_device(target_id: Optional[str],
                mode: BootMode,
                serial_num: Optional[str] = None,
                must_boot: bool = False) -> None:
    """Boot device into desired mode.

    Args:
        target_id: Optional target_id of device.
        mode: Desired boot mode.
        must_boot: Forces device to boot, regardless of current state.
    Raises:
        StateTransitionError: When final state of device is not desired.
    """
    # Avoid cycle dependency.
    # This file will be replaced with serial_boot_device quite soon, later one
    # should be much more reliable comparing to ffx target list and ssh. So
    # changing the file structure is not necessary in the current situation.
    # pylint: disable=cyclic-import, import-outside-toplevel
    # pylint: disable=wrong-import-position
    import serial_boot_device
    if not serial_boot_device.boot_device(target_id, serial_num, mode,
                                          must_boot):
        raise StateTransitionError(
            f'Could not get device to desired state {mode}.')
