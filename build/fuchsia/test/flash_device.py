#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implements commands for flashing a Fuchsia device."""

import argparse
import logging
import os
import subprocess
import sys

from typing import Optional, Tuple

import common
from boot_device import BootMode, StateTransitionError, boot_device
from common import get_system_info, find_image_in_sdk, \
                   register_device_args
from compatible_utils import get_sdk_hash, running_unattended
from lockfile import lock

# Flash-file lock. Used to restrict number of flash operations per host.
# File lock should be marked as stale after 15 mins.
_FF_LOCK = os.path.join('/tmp', 'flash.lock')
_FF_LOCK_STALE_SECS = 60 * 15
_FF_LOCK_ACQ_TIMEOUT = _FF_LOCK_STALE_SECS


def _get_system_info(target: Optional[str],
                     serial_num: Optional[str]) -> Tuple[str, str]:
    """Retrieves installed OS version from device.

    Args:
        target: Target to get system info of.
        serial_num: Serial number of device to get system info of.
    Returns:
        Tuple of strings, containing (product, version number).
    """

    if running_unattended():
        try:
            boot_device(target, BootMode.REGULAR, serial_num)
        except (subprocess.CalledProcessError, StateTransitionError):
            logging.warning('Could not boot device. Assuming in fastboot')
            return ('', '')

    return get_system_info(target)


def _update_required(
        os_check,
        system_image_dir: Optional[str],
        target: Optional[str],
        serial_num: Optional[str] = None) -> Tuple[bool, Optional[str]]:
    """Returns True if a system update is required and path to image dir."""

    if os_check == 'ignore':
        return False, system_image_dir
    if not system_image_dir:
        raise ValueError('System image directory must be specified.')
    if not os.path.exists(system_image_dir):
        logging.warning(
            'System image directory does not exist. Assuming it\'s '
            'a product-bundle name and dynamically searching for '
            'image directory')
        path = find_image_in_sdk(system_image_dir)
        if not path:
            raise FileNotFoundError(
                f'System image directory {system_image_dir} could not'
                'be found')
        system_image_dir = path
    if (os_check == 'check'
            and get_sdk_hash(system_image_dir) == _get_system_info(
                target, serial_num)):
        return False, system_image_dir
    return True, system_image_dir


def _run_flash_command(system_image_dir: str, target_id: Optional[str]):
    """Helper function for running `ffx target flash`."""
    logging.info('Flashing %s to %s', system_image_dir, target_id)
    # Flash only with a file lock acquired.
    # This prevents multiple fastboot binaries from flashing concurrently,
    # which should increase the odds of flashing success.
    with lock(_FF_LOCK, timeout=_FF_LOCK_ACQ_TIMEOUT):
        # The ffx.fastboot.inline_target has negative impact when ffx
        # discovering devices in fastboot, so it's inlined here to limit its
        # scope. See the discussion in https://fxbug.dev/issues/317228141.
        logging.info(
            'Flash result %s',
            common.run_ffx_command(cmd=('target', 'flash', '-b',
                                        system_image_dir,
                                        '--no-bootloader-reboot'),
                                   target_id=target_id,
                                   configs=['ffx.fastboot.inline_target=true'],
                                   capture_output=True).stdout)


def update(system_image_dir: str,
           os_check: str,
           target: Optional[str],
           serial_num: Optional[str] = None) -> None:
    """Conditionally updates target given.

    Args:
        system_image_dir: string, path to image directory.
        os_check: <check|ignore|update>, which decides how to update the device.
        target: Node-name string indicating device that should be updated.
        serial_num: String of serial number of device that should be updated.
    """
    needs_update, actual_image_dir = _update_required(os_check,
                                                      system_image_dir, target,
                                                      serial_num)
    logging.info('update_required %s, actual_image_dir %s', needs_update,
                 actual_image_dir)
    if not needs_update:
        return
    if serial_num:
        boot_device(target, BootMode.BOOTLOADER, serial_num)
        _run_flash_command(system_image_dir, serial_num)
    else:
        _run_flash_command(system_image_dir, target)


def register_update_args(arg_parser: argparse.ArgumentParser,
                         default_os_check: Optional[str] = 'check') -> None:
    """Register common arguments for device updating."""
    serve_args = arg_parser.add_argument_group('update',
                                               'device updating arguments')
    serve_args.add_argument('--system-image-dir',
                            help='Specify the directory that contains the '
                            'Fuchsia image used to flash the device. Only '
                            'needs to be specified if "os_check" is not '
                            '"ignore".')
    serve_args.add_argument('--serial-num',
                            default=os.environ.get('FUCHSIA_FASTBOOT_SERNUM'),
                            help='Serial number of the device. Should be '
                            'specified for devices that do not have an image '
                            'flashed.')
    serve_args.add_argument('--os-check',
                            choices=['check', 'update', 'ignore'],
                            default=default_os_check,
                            help='Sets the OS version enforcement policy. If '
                            '"check", then the deployment process will halt '
                            'if the target\'s version does not match. If '
                            '"update", then the target device will '
                            'be reflashed. If "ignore", then the OS version '
                            'will not be checked.')


def main():
    """Stand-alone function for flashing a device."""
    parser = argparse.ArgumentParser()
    register_device_args(parser)
    register_update_args(parser, default_os_check='update')
    args = parser.parse_args()
    update(args.system_image_dir, args.os_check, args.target_id,
           args.serial_num)


if __name__ == '__main__':
    sys.exit(main())
