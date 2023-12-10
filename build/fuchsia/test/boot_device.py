# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Functionalities to reliably reboot the device."""

import enum
import json
import logging
import subprocess
import time

from typing import Optional

from common import run_continuous_ffx_command, run_ffx_command, get_ssh_address
from compatible_utils import get_ssh_prefix


class TargetState(enum.Enum):
    """State of a target."""
    UNKNOWN = enum.auto()
    DISCONNECTED = enum.auto()
    PRODUCT = enum.auto()
    FASTBOOT = enum.auto()
    ZEDBOOT = enum.auto()


class BootMode(enum.Enum):
    """Specifies boot mode for device."""
    REGULAR = enum.auto()
    RECOVERY = enum.auto()
    BOOTLOADER = enum.auto()


_STATE_TO_BOOTMODE = {
    TargetState.PRODUCT: BootMode.REGULAR,
    TargetState.FASTBOOT: BootMode.BOOTLOADER,
    TargetState.ZEDBOOT: BootMode.RECOVERY
}

_BOOTMODE_TO_STATE = {value: key for key, value in _STATE_TO_BOOTMODE.items()}


class StateNotFoundError(Exception):
    """Raised when target's state cannot be found."""


class StateTransitionError(Exception):
    """Raised when target does not transition to desired state."""


def _state_string_to_state(state_str: str) -> TargetState:
    state_str = state_str.strip().lower()
    if state_str == 'product':
        return TargetState.PRODUCT
    if state_str == 'zedboot (r)':
        return TargetState.ZEDBOOT
    if state_str == 'fastboot':
        return TargetState.FASTBOOT
    if state_str == 'unknown':
        return TargetState.UNKNOWN
    if state_str == 'disconnected':
        return TargetState.DISCONNECTED

    raise NotImplementedError(f'State {state_str} not supported')


def _get_target_state(target_id: Optional[str],
                      serial_num: Optional[str],
                      num_attempts: int = 1) -> TargetState:
    """Return state of target or the default target.

    Args:
        target_id: Optional nodename of the target. If not given, default target
        is used.
        serial_num: Optional serial number of target. Only usable if device is
        in fastboot.
        num_attempts: Optional number of times to attempt getting status.

    Returns:
        TargetState of the given node, if found.

    Raises:
        StateNotFoundError: If target cannot be found, or default target is not
            defined if |target_id| is not given.
    """
    for i in range(num_attempts):
        targets = json.loads(
            run_ffx_command(cmd=('target', 'list'),
                            check=True,
                            capture_output=True,
                            json_out=True).stdout.strip())
        for target in targets:
            if target_id is None and target['is_default']:
                return _state_string_to_state(target['target_state'])
            if target_id == target['nodename']:
                return _state_string_to_state(target['target_state'])
            if serial_num == target['serial']:
                # Should only return Fastboot.
                return _state_string_to_state(target['target_state'])
        # Do not sleep for last attempt.
        if i < num_attempts - 1:
            time.sleep(10)

    # Could not find a state for given target.
    error_target = target_id
    if target_id is None:
        error_target = 'default target'

    raise StateNotFoundError(f'Could not find state for {error_target}.')


def boot_device(target_id: Optional[str],
                mode: BootMode,
                serial_num: Optional[str] = None,
                must_boot: bool = False) -> None:
    """Boot device into desired mode, with fallback to SSH on failure.

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
    if serial_boot_device.boot_device(target_id, serial_num, mode, must_boot):
        return

    # Skip boot call if already in the state and not skipping check.
    state = _get_target_state(target_id, serial_num, num_attempts=3)
    wanted_state = _BOOTMODE_TO_STATE.get(mode)
    if not must_boot:
        logging.debug('Current state %s. Want state %s', str(state),
                      str(wanted_state))
        must_boot = state != wanted_state

    if not must_boot:
        logging.debug('Skipping boot - already in good state')
        return

    def _wait_for_state_transition(current_state: TargetState):
        local_state = None
        # Check that we transition out of current state.
        for _ in range(30):
            try:
                local_state = _get_target_state(target_id, serial_num)
                if local_state != current_state:
                    # Changed states - can continue
                    break
            except StateNotFoundError:
                logging.debug('Device disconnected...')
                if current_state != TargetState.DISCONNECTED:
                    # Changed states - can continue
                    break
            finally:
                time.sleep(2)
        else:
            logging.warning(
                'Device did not change from initial state. Exiting early')
            return local_state or TargetState.DISCONNECTED

        # Now we want to transition to the new state.
        for _ in range(90):
            try:
                local_state = _get_target_state(target_id, serial_num)
                if local_state == wanted_state:
                    return local_state
            except StateNotFoundError:
                logging.warning('Could not find target state.'
                                ' Sleeping then retrying...')
            finally:
                time.sleep(2)
        return local_state or TargetState.DISCONNECTED

    _boot_device_ffx(target_id, serial_num, state, mode)
    state = _wait_for_state_transition(state)

    if state == TargetState.DISCONNECTED:
        raise StateNotFoundError('Target could not be found!')

    if state == wanted_state:
        return

    logging.warning(
        'Booting with FFX to %s did not succeed. Attempting with DM', mode)

    # Fallback to SSH, with no retry if we tried with ffx.:
    _boot_device_dm(target_id, serial_num, state, mode)
    state = _wait_for_state_transition(state)

    if state != wanted_state:
        raise StateTransitionError(
            f'Could not get device to desired state. Wanted {wanted_state},'
            f' got {state}')
    logging.debug('Got desired state: %s', state)


def _boot_device_ffx(target_id: Optional[str], serial_num: Optional[str],
                     current_state: TargetState, mode: BootMode):
    cmd = ['target', 'reboot']
    if mode == BootMode.REGULAR:
        logging.info('Triggering regular boot')
    elif mode == BootMode.RECOVERY:
        cmd.append('-r')
    elif mode == BootMode.BOOTLOADER:
        cmd.append('-b')
    else:
        raise NotImplementedError(f'BootMode {mode} not supported')

    logging.debug('FFX reboot with command [%s]', ' '.join(cmd))
    # TODO(crbug.com/1432405): We need to wait for the state transition or kill
    # the process if it fails.
    if current_state == TargetState.FASTBOOT:
        run_continuous_ffx_command(cmd=cmd,
                                   target_id=serial_num,
                                   configs=['product.reboot.use_dm=true'])
    else:
        run_continuous_ffx_command(cmd=cmd,
                                   target_id=target_id,
                                   configs=['product.reboot.use_dm=true'])


def _boot_device_dm(target_id: Optional[str], serial_num: Optional[str],
                    current_state: TargetState, mode: BootMode):
    # Can only use DM if device is in regular boot.
    if current_state != TargetState.PRODUCT:
        if mode == BootMode.REGULAR:
            raise StateTransitionError('Cannot boot to Regular via DM - '
                                       'FFX already failed to do so.')
        # Boot to regular.
        # TODO(crbug.com/1432405): After changing to run_continuous_ffx_command,
        # this behavior becomes invalid, we need to wait for the state
        # transition.
        _boot_device_ffx(target_id, serial_num, current_state,
                         BootMode.REGULAR)

    ssh_prefix = get_ssh_prefix(get_ssh_address(target_id))

    reboot_cmd = None

    if mode == BootMode.REGULAR:
        reboot_cmd = 'reboot'
    elif mode == BootMode.RECOVERY:
        reboot_cmd = 'reboot-recovery'
    elif mode == BootMode.BOOTLOADER:
        reboot_cmd = 'reboot-bootloader'
    else:
        raise NotImplementedError(f'BootMode {mode} not supported')

    # Boot commands can fail due to SSH connections timeout.
    full_cmd = ssh_prefix + ['--', 'dm', reboot_cmd]
    logging.debug('DM reboot with command [%s]', ' '.join(full_cmd))
    subprocess.run(full_cmd, check=False)
