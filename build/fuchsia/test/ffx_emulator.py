# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Provide helpers for running Fuchsia's `ffx emu`."""

import argparse
import ast
import logging
import os
import json
import random

from contextlib import AbstractContextManager

from common import run_ffx_command, IMAGES_ROOT, INTERNAL_IMAGES_ROOT, SDK_ROOT
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
        self._hardware_gpu = args.hardware_gpu
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
        if not self._enable_graphics:
            emu_command.append('-H')
        if self._hardware_gpu:
            emu_command.append('--gpu')
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

        # TODO(https://fxbug.dev/99321): remove when ffx has native support
        # for starting emulator on arm64 host.
        if get_host_arch() == 'arm64':

            arm64_qemu_dir = os.path.join(SDK_ROOT, 'tools', 'arm64',
                                          'qemu_internal')

            # The arm64 emulator binaries are downloaded separately, so add
            # a symlink to the expected location inside the SDK.
            if not os.path.isdir(arm64_qemu_dir):
                os.symlink(
                    os.path.join(SDK_ROOT, '..', '..', 'qemu-linux-arm64'),
                    arm64_qemu_dir)

            # Add the arm64 emulator binaries to the SDK's manifest.json file.
            sdk_manifest = os.path.join(SDK_ROOT, 'meta', 'manifest.json')
            with open(sdk_manifest, 'r+') as f:
                data = json.load(f)
                for part in data['parts']:
                    if part['meta'] == 'tools/x64/qemu_internal-meta.json':
                        part['meta'] = 'tools/arm64/qemu_internal-meta.json'
                        break
                f.seek(0)
                json.dump(data, f)
                f.truncate()

            # Generate a meta file for the arm64 emulator binaries using its
            # x64 counterpart.
            qemu_arm64_meta_file = os.path.join(SDK_ROOT, 'tools', 'arm64',
                                                'qemu_internal-meta.json')
            qemu_x64_meta_file = os.path.join(SDK_ROOT, 'tools', 'x64',
                                              'qemu_internal-meta.json')
            with open(qemu_x64_meta_file) as f:
                data = str(json.load(f))
            qemu_arm64_meta = data.replace(r'tools/x64', 'tools/arm64')
            with open(qemu_arm64_meta_file, "w+") as f:
                json.dump(ast.literal_eval(qemu_arm64_meta), f)

        # Always use qemu for arm64 images, no matter it runs on arm64 hosts or
        # x64 hosts with simulation.
        if self._product.endswith('arm64'):
            emu_command.extend(['--engine', 'qemu'])

        run_ffx_command(cmd=emu_command,
                        timeout=310,
                        configs=['emu.start.timeout=300'])

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
