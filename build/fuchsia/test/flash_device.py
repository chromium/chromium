#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implements commands for flashing a Fuchsia device."""

import argparse
import json
import os
import sys

from typing import Optional, Tuple

from common import register_device_args, run_ffx_command
from compatible_utils import get_sdk_hash


def _get_system_info(target: Optional[str]) -> Tuple[str, str]:
    """Retrieves installed OS version from device.

    Returns:
        Tuple of strings, containing (product, version number).
    """

    info_json = json.loads(
        run_ffx_command(('target', 'show', '--json'),
                        target_id=target,
                        capture_output=True).stdout.strip())
    for info in info_json:
        if info['title'] == 'Build':
            return (info['child'][1]['value'], info['child'][0]['value'])

    # If the information was not retrieved, return empty strings to indicate
    # unknown system info.
    return ('', '')


def flash(system_image_dir: str, os_check: str, target: Optional[str]) -> None:
    """Flash the device."""

    if os_check == 'ignore':
        return
    if not system_image_dir:
        raise ValueError('System image directory must be specified.')
    if (os_check == 'check'
            and get_sdk_hash(system_image_dir) == _get_system_info(target)):
        return
    manifest = os.path.join(system_image_dir, 'flash-manifest.manifest')
    run_ffx_command(('target', 'flash', manifest), target)
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
    serve_args.add_argument('--os-check',
                            choices=['check', 'update', 'ignore'],
                            default=default_os_check,
                            help='Sets the OS version enforcement policy. If '
                            '"check", then the deployment process will halt '
                            'if the target\'s version does not match. If '
                            '"update", then the target device will '
                            'automatically be repaved. If "ignore", then the '
                            'OS version won\'t be checked.')


def main():
    """Stand-alone function for flashing a device."""
    parser = argparse.ArgumentParser()
    register_device_args(parser)
    register_flash_args(parser)
    args = parser.parse_args()
    flash(args.system_image_dir, args.os_check, args.target_id)


if __name__ == '__main__':
    sys.exit(main())
