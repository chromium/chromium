#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Provides a class for managing emulators."""

import argparse
import logging
import sys
import time

from common import register_log_args
from ffx_integration import FfxEmulator


def register_emulator_args(parser: argparse.ArgumentParser) -> None:
    """Register emulator specific arguments."""
    femu_args = parser.add_argument_group('emulator',
                                          'emulator startup arguments.')
    femu_args.add_argument('--enable-graphics',
                           action='store_true',
                           default=False,
                           help='Start emulator with graphics instead of '
                           'headless.')
    femu_args.add_argument(
        '--hardware-gpu',
        action='store_true',
        default=False,
        help='Use host GPU hardware instead of Swiftshader.')
    femu_args.add_argument(
        '--product-bundle',
        help='Specify a product bundle used for booting the '
        'emulator. Defaults to the terminal product.')
    femu_args.add_argument('--with-network',
                           action='store_true',
                           default=False,
                           help='Run emulator with emulated nic via tun/tap.')


def create_emulator_from_args(args: argparse.Namespace) -> FfxEmulator:
    """Helper method for initializing an FfxEmulator class with parsed
    arguments."""
    return FfxEmulator(args.enable_graphics, args.hardware_gpu,
                       args.product_bundle, args.with_network, args.logs_dir)


def main():
    """Stand-alone function for starting an emulator."""
    parser = argparse.ArgumentParser()
    register_emulator_args(parser)
    register_log_args(parser)
    args = parser.parse_args()
    with create_emulator_from_args(args):
        try:
            while True:
                time.sleep(10000)
        except KeyboardInterrupt:
            logging.info('Ctrl-C received; shutting down the emulator.')
        except SystemExit:
            logging.info('SIGTERM received; shutting down the emulator.')


if __name__ == '__main__':
    sys.exit(main())
