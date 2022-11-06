#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implements commands for flashing a Fuchsia device."""

import argparse
import json
import os
import subprocess
import sys
import time

from typing import Optional, Tuple

from common import register_device_args, run_ffx_command
from compatible_utils import get_sdk_hash, get_ssh_keys, running_unattended
from ffx_integration import ScopedFfxConfig


def _get_system_info(target: Optional[str]) -> Tuple[str, str]:
    """Retrieves installed OS version from device.

    Returns:
        Tuple of strings, containing (product, version number).
    """

    info_cmd = run_ffx_command(('target', 'show', '--json'),
                               target_id=target,
                               capture_output=True,
                               check=False)
    if info_cmd.returncode == 0:
        info_json = json.loads(info_cmd.stdout.strip())
        for info in info_json:
            if info['title'] == 'Build':
                return (info['child'][1]['value'], info['child'][0]['value'])

    # If the information was not retrieved, return empty strings to indicate
    # unknown system info.
    return ('', '')


def update_required(os_check, system_image_dir: Optional[str],
                    target: Optional[str]) -> bool:
    """Returns True if a system updated is required."""

    if os_check == 'ignore':
        return False
    if not system_image_dir:
        raise ValueError('System image directory must be specified.')
    if (os_check == 'check'
            and get_sdk_hash(system_image_dir) == _get_system_info(target)):
        return False
    return True


def _run_flash_command(system_image_dir: str, target_id: Optional[str]):
    """Helper function for running `ffx target flash`."""

    # TODO(fxb/91843): Remove workaround when ffx has stable support for
    # multiple hardware devices connected via USB.
    if running_unattended():
        flash_cmd = [
            os.path.join(system_image_dir, 'flash.sh'),
            '--ssh-key=%s' % get_ssh_keys(),
        ]
        if target_id:
            flash_cmd.extend(('-s', target_id))
        subprocess.run(flash_cmd, check=True, timeout=240)
        return

    manifest = os.path.join(system_image_dir, 'flash-manifest.manifest')
    run_ffx_command(('target', 'flash', manifest, '--no-bootloader-reboot'),
                    target_id=target_id,
                    configs=[
                        'fastboot.usb.disabled=true',
                        'ffx.fastboot.inline_target=true'
                    ])


def flash(system_image_dir: str,
          os_check: str,
          target: Optional[str],
          serial_num: Optional[str] = None) -> None:
    """Flash the device."""

    if update_required(os_check, system_image_dir, target):
        with ScopedFfxConfig('fastboot.reboot.reconnect_timeout', '120'):
            if serial_num:
                with ScopedFfxConfig('discovery.zedboot.enabled', 'true'):
                    run_ffx_command(('target', 'reboot', '-b'),
                                    target,
                                    check=False)
                for _ in range(10):
                    time.sleep(10)
                    if run_ffx_command(('target', 'list', serial_num),
                                       check=False).returncode == 0:
                        break
                _run_flash_command(system_image_dir, serial_num)
            else:
                _run_flash_command(system_image_dir, target)
        run_ffx_command(('target', 'wait'), target)


def register_flash_args(arg_parser: argparse.ArgumentParser,
                        default_os_check: Optional[str] = 'check') -> None:
    """Register common arguments for device flashing."""

    serve_args = arg_parser.add_argument_group('flash',
                                               'device flashing arguments')
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


def main():
    """Stand-alone function for flashing a device."""
    parser = argparse.ArgumentParser()
    register_device_args(parser)
    register_flash_args(parser)
    args = parser.parse_args()
    flash(args.system_image_dir, args.os_check, args.target_id,
          args.serial_num)


if __name__ == '__main__':
    sys.exit(main())
