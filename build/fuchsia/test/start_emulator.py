# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Provides a class for managing emulators."""

import argparse

from contextlib import AbstractContextManager

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
        '--product',
        help='Specify a product bundle used for booting the '
        'emulator. Defaults to the terminal product.')
    femu_args.add_argument('--with-network',
                           action='store_true',
                           help='Run emulator with emulated nic via tun/tap.')
    femu_args.add_argument('--everlasting',
                           action='store_true',
                           help='If the emulator should be long-living.')
    femu_args.add_argument(
        '--device-spec',
        help='Configure the virtual device to use. They are usually defined in '
        'the product-bundle/virtual_devices/manifest.json. If this flag is not '
        'provided or is an empty string, ffx emu will decide the recommended '
        'spec.')


def create_emulator_from_args(
        args: argparse.Namespace) -> AbstractContextManager:
    """Helper method for initializing an FfxEmulator class with parsed
    arguments."""
    return FfxEmulator(args)
