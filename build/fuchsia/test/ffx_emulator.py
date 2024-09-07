# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Provide helpers for running Fuchsia's `ffx emu`."""

import argparse
import logging
import os
import random

from contextlib import AbstractContextManager

import monitors

from common import run_ffx_command, IMAGES_ROOT, INTERNAL_IMAGES_ROOT, \
                   DIR_SRC_ROOT
from compatible_utils import get_host_arch


class FfxEmulator(AbstractContextManager):
    """A helper for managing emulators."""
    # pylint: disable=too-many-branches
    def __init__(self, args: argparse.Namespace) -> None:
        if args.product:
            self._product = args.product
        else:
            if get_host_arch() == 'x64':
                self._product = 'terminal.x64'
            else:
                self._product = 'terminal.qemu-arm64'

        self._enable_graphics = args.enable_graphics
        self._logs_dir = args.logs_dir
        self._with_network = args.with_network
        if args.everlasting:
            # Do not change the name, it will break the logic.
            # ffx has a prefix-matching logic, so 'fuchsia-emulator' is not
            # usable to avoid breaking local development workflow. I.e.
            # developers can create an everlasting emulator and an ephemeral one
            # without interfering each other.
            self._node_name = 'fuchsia-everlasting-emulator'
            assert self._everlasting()
        else:
            self._node_name = 'fuchsia-emulator-' + str(random.randint(
                1, 9999))
        self._device_spec = args.device_spec

    def _everlasting(self) -> bool:
        return self._node_name == 'fuchsia-everlasting-emulator'

    def __enter__(self) -> str:
        """Start the emulator.

        Returns:
            The node name of the emulator.
        """
        logging.info('Starting emulator %s', self._node_name)
        prod, board = self._product.split('.', 1)
        image_dir = os.path.join(IMAGES_ROOT, prod, board)
        if not os.path.isdir(image_dir):
            image_dir = os.path.join(INTERNAL_IMAGES_ROOT, prod, board)
        emu_command = ['emu', 'start', image_dir, '--name', self._node_name]
        configs = ['emu.start.timeout=300']
        if not self._enable_graphics:
            emu_command.append('-H')
        if self._logs_dir:
            emu_command.extend(
                ('-l', os.path.join(self._logs_dir, 'emulator_log')))
        if self._with_network:
            emu_command.extend(['--net', 'tap'])
        else:
            emu_command.extend(['--net', 'user'])
        if self._everlasting():
            emu_command.extend(['--reuse-with-check'])
        if self._device_spec:
            emu_command.extend(['--device', self._device_spec])

        # fuchsia-sdk does not carry arm64 qemu binaries, so use overrides to
        # allow it using the qemu-arm64 being downloaded separately.
        if get_host_arch() == 'arm64':
            configs.append(
                'sdk.overrides.qemu_internal=' +
                os.path.join(DIR_SRC_ROOT, 'third_party', 'qemu-linux-arm64',
                             'bin', 'qemu-system-aarch64'))
            configs.append('sdk.overrides.uefi_internal=' +
                           os.path.join(DIR_SRC_ROOT, 'third_party', 'edk2',
                                        'qemu-arm64', 'QEMU_EFI.fd'))

        # Always use qemu for arm64 images, no matter it runs on arm64 hosts or
        # x64 hosts with simulation.
        if self._product.endswith('arm64'):
            emu_command.extend(['--engine', 'qemu'])

        with monitors.time_consumption('emulator', 'startup_time'):
            run_ffx_command(cmd=emu_command, timeout=310, configs=configs)

        return self._node_name

    def __exit__(self, exc_type, exc_value, traceback) -> bool:
        """Shutdown the emulator."""

        logging.info('Stopping the emulator %s', self._node_name)
        cmd = ['emu', 'stop', self._node_name]
        if self._everlasting():
            cmd.extend(['--persist'])
        # The emulator might have shut down unexpectedly, so this command
        # might fail.
        run_ffx_command(cmd=cmd, check=False)
        # Do not suppress exceptions.
        return False
