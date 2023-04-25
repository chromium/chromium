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
import time

from typing import Optional, Tuple

import common
from common import BootMode, boot_device, check_ssh_config_file, \
    get_system_info, find_image_in_sdk, register_device_args
from compatible_utils import get_sdk_hash, get_ssh_keys, pave, \
    running_unattended, add_exec_to_file, get_host_arch
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

    # TODO(b/242191374): Remove when devices in swarming are no longer booted
    # into zedboot.
    if running_unattended():
        try:
            boot_device(target, BootMode.REGULAR, serial_num)
        except (subprocess.CalledProcessError, common.StateTransitionError):
            logging.warning('Could not boot device. Assuming in ZEDBOOT')
            return ('', '')
        wait_cmd = common.run_ffx_command(('target', 'wait', '-t', '180'),
                                          target,
                                          check=False)
        if wait_cmd.returncode != 0:
            return ('', '')

    return get_system_info(target)


def update_required(
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


def _add_exec_to_flash_binaries(system_image_dir: str) -> None:
    """Add exec to required flash files.

    The flash files may vary depending if a product-bundle or a prebuilt images
    directory is being used.
    Args:
      system_image_dir: string path to the directory containing the flash files.
    """
    pb_files = [
        'flash.sh',
        os.path.join(f'host_{get_host_arch()}', 'fastboot')
    ]
    image_files = ['flash.sh', f'fastboot.exe.linux-{get_host_arch()}']
    use_pb_files = os.path.exists(os.path.join(system_image_dir, pb_files[1]))
    for f in pb_files if use_pb_files else image_files:
        add_exec_to_file(os.path.join(system_image_dir, f))


def _run_flash_command(system_image_dir: str, target_id: Optional[str]):
    """Helper function for running `ffx target flash`."""

    _add_exec_to_flash_binaries(system_image_dir)
    # TODO(fxb/91843): Remove workaround when ffx has stable support for
    # multiple hardware devices connected via USB.
    if running_unattended():
        flash_cmd = [
            os.path.join(system_image_dir, 'flash.sh'),
            '--ssh-key=%s' % get_ssh_keys()
        ]
        # Target ID could be the nodename or the Serial number.
        if target_id:
            flash_cmd.extend(('-s', target_id))
        subprocess.run(flash_cmd, check=True, timeout=240)
        return

    manifest = os.path.join(system_image_dir, 'flash-manifest.manifest')
    common.run_ffx_command(
        ('target', 'flash', manifest, '--no-bootloader-reboot'),
        target_id=target_id,
        configs=[
            'fastboot.usb.disabled=true', 'ffx.fastboot.inline_target=true',
            'fastboot.reboot.reconnect_timeout=120'
        ])


def flash(system_image_dir: str,
          target: Optional[str],
          serial_num: Optional[str] = None) -> None:
    """Flash the device."""
    # Flash only with a file lock acquired.
    # This prevents multiple fastboot binaries from flashing concurrently,
    # which should increase the odds of flashing success.
    with lock(_FF_LOCK, timeout=_FF_LOCK_ACQ_TIMEOUT):
        if serial_num:
            boot_device(target, BootMode.BOOTLOADER, serial_num)
            for _ in range(10):
                time.sleep(10)
                if common.run_ffx_command(('target', 'list', serial_num),
                                          check=False).returncode == 0:
                    break
            _run_flash_command(system_image_dir, serial_num)
        else:
            _run_flash_command(system_image_dir, target)


def update(system_image_dir: str,
           os_check: str,
           target: Optional[str],
           serial_num: Optional[str] = None,
           should_pave: Optional[bool] = True) -> None:
    """Conditionally updates target given.

    Args:
        system_image_dir: string, path to image directory.
        os_check: <check|ignore|update>, which decides how to update the device.
        target: Node-name string indicating device that should be updated.
        serial_num: String of serial number of device that should be updated.
        should_pave: Optional bool on whether or not to pave or flash.
    """
    needs_update, actual_image_dir = update_required(os_check,
                                                     system_image_dir, target,
                                                     serial_num)

    system_image_dir = actual_image_dir
    if needs_update:
        check_ssh_config_file()
        if should_pave:
            if running_unattended():
                assert target, ('Target ID must be specified on swarming when'
                                ' paving.')
                # TODO(crbug.com/1405525): We should check the device state
                # before and after rebooting it to avoid unnecessary reboot or
                # undesired state.
                boot_device(target, BootMode.RECOVERY, serial_num)
            try:
                pave(system_image_dir, target)
            except subprocess.TimeoutExpired:
                # Fallback to flashing, just in case it might work.
                # This could recover the device and make it usable.
                # If it fails, device is unpaveable anyway, and should be taken
                # out of fleet - this will do that.
                flash(system_image_dir, target, serial_num)
        else:
            flash(system_image_dir, target, serial_num)
        # Always sleep after all updates.
        time.sleep(180)


def register_update_args(arg_parser: argparse.ArgumentParser,
                         default_os_check: Optional[str] = 'check',
                         default_pave: Optional[bool] = True) -> None:
    """Register common arguments for device updating."""
    serve_args = arg_parser.add_argument_group('update',
                                               'device updating arguments')
    serve_args.add_argument('--system-image-dir',
                            help='Specify the directory that contains the '
                            'Fuchsia image used to pave the device. Only '
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
    serve_args.add_argument('--pave',
                            action='store_true',
                            help='Performs a pave instead of a flash. '
                            'Device must already be in Zedboot')
    serve_args.add_argument('--no-pave',
                            action='store_false',
                            dest='pave',
                            help='Performs a flash instead of a pave '
                            '(experimental).')
    serve_args.set_defaults(pave=default_pave)


def main():
    """Stand-alone function for flashing a device."""
    parser = argparse.ArgumentParser()
    register_device_args(parser)
    register_update_args(parser, default_os_check='update', default_pave=False)
    args = parser.parse_args()
    update(args.system_image_dir, args.os_check, args.target_id,
           args.serial_num, args.pave)


if __name__ == '__main__':
    sys.exit(main())
