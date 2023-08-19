#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Provides a class for managing emulators."""

import argparse
import logging
import sys

from contextlib import AbstractContextManager

from common import catch_sigterm, register_log_args, wait_for_sigterm
from ffx_emulator import FfxEmulator


def register_emulator_args(parser: argparse.ArgumentParser,
                           enable_graphics: bool = False) -> None:
    """Register emulator specific arguments."""
    femu_args = parser.add_argument_group('emulator',
                                          'emulator startup arguments.')
    femu_args.add_argument('--custom-image',
                           dest='product_bundle',
                           help='Backwards compatible flag that specifies an '
                           'image used for booting up the emulator.')
    if enable_graphics:
        femu_args.add_argument('--disable-graphics',
                               action='store_false',
                               dest='enable_graphics',
                               help='Start emulator in headless mode.')
    else:
        femu_args.add_argument('--enable-graphics',
                               action='store_true',
                               help='Start emulator with graphics.')
    femu_args.add_argument(
        '--hardware-gpu',
        action='store_true',
        help='Use host GPU hardware instead of Swiftshader.')
    femu_args.add_argument(
        '--product',
        help='Specify a product bundle used for booting the '
        'emulator. Defaults to the terminal product.')
    femu_args.add_argument('--with-network',
                           action='store_true',
                           help='Run emulator with emulated nic via tun/tap.')
    femu_args.add_argument('--everlasting',
                           action='store_true',
                           help='If the emulator should be long-living.')


def create_emulator_from_args(
        args: argparse.Namespace) -> AbstractContextManager:
    """Helper method for initializing an FfxEmulator class with parsed
    arguments."""
    return FfxEmulator(args)


def main():
    """Stand-alone function for starting an emulator."""

    catch_sigterm()
    logging.basicConfig(level=logging.INFO)
    parser = argparse.ArgumentParser()
    register_emulator_args(parser, True)
    register_log_args(parser)
    parser.add_argument('--target-id-only',
                        action='store_true',
                        help='Write only the target emulator id to the ' \
                             'stdout. It is usually useful in the unattended ' \
                             'environment.')
    args = parser.parse_args()
    with create_emulator_from_args(args) as target_id:
        if args.target_id_only:
            print(target_id, flush=True)
        else:
            logging.info(
                'Emulator successfully started. You can now run Chrome '
                'Fuchsia tests with --target-id=%s to target this emulator.',
                target_id)
        wait_for_sigterm('shutting down the emulator.')


if __name__ == '__main__':
    sys.exit(main())
