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
import subprocess

from contextlib import AbstractContextManager

from common import check_ssh_config_file, find_image_in_sdk, get_system_info, \
                   run_ffx_command, SDK_ROOT
from compatible_utils import get_host_arch, get_sdk_hash

_EMU_COMMAND_RETRIES = 3


class FfxEmulator(AbstractContextManager):
    """A helper for managing emulators."""
    def __init__(self, args: argparse.Namespace) -> None:
        if args.product_bundle:
            self._product_bundle = args.product_bundle
        else:
            self._product_bundle = 'terminal.qemu-' + get_host_arch()

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

        # Set the download path parallel to Fuchsia SDK directory
        # permanently so that scripts can always find the product bundles.
        run_ffx_command(('config', 'set', 'pbms.storage.path',
                         os.path.join(SDK_ROOT, os.pardir, 'images')))

    def _everlasting(self) -> bool:
        return self._node_name == 'fuchsia-everlasting-emulator'

    def _start_emulator(self) -> None:
        """Start the emulator."""
        logging.info('Starting emulator %s', self._node_name)
        check_ssh_config_file()
        emu_command = [
            'emu', 'start', self._product_bundle, '--name', self._node_name
        ]
        if not self._enable_graphics:
            emu_command.append('-H')
        if self._hardware_gpu:
            emu_command.append('--gpu')
        if self._logs_dir:
            emu_command.extend(
                ('-l', os.path.join(self._logs_dir, 'emulator_log')))
        if self._with_network:
            emu_command.extend(('--net', 'tap'))
        else:
            emu_command.extend(('--net', 'user'))

        # TODO(https://crbug.com/1336776): remove when ffx has native support
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
            emu_command.extend(['--engine', 'qemu'])

        for i in range(_EMU_COMMAND_RETRIES):

            # If the ffx daemon fails to establish a connection with
            # the emulator after 85 seconds, that means the emulator
            # failed to be brought up and a retry is needed.
            # TODO(fxb/103540): Remove retry when start up issue is fixed.
            try:
                # TODO(fxb/125872): Debug is added for examining flakiness.
                configs = ['emu.start.timeout=90']
                if i > 0:
                    logging.warning(
                        'Emulator failed to start.')
                run_ffx_command(emu_command, timeout=100, configs=configs)
                break
            except (subprocess.TimeoutExpired, subprocess.CalledProcessError):
                run_ffx_command(('emu', 'stop'))

    def _shutdown_emulator(self) -> None:
        """Shutdown the emulator."""

        logging.info('Stopping the emulator %s', self._node_name)
        # The emulator might have shut down unexpectedly, so this command
        # might fail.
        run_ffx_command(('emu', 'stop', self._node_name), check=False)

    def __enter__(self) -> str:
        """Start the emulator if necessary.

        Returns:
            The node name of the emulator.
        """

        if self._everlasting():
            sdk_hash = get_sdk_hash(find_image_in_sdk(self._product_bundle))
            sys_info = get_system_info(self._node_name)
            if sdk_hash == sys_info:
                return self._node_name
            logging.info(
                ('The emulator version [%s] does not match the SDK [%s], '
                 'updating...'), sys_info, sdk_hash)

        self._start_emulator()
        return self._node_name

    def __exit__(self, exc_type, exc_value, traceback) -> bool:
        """Shutdown the emulator if necessary."""

        if not self._everlasting():
            self._shutdown_emulator()
        # Do not suppress exceptions.
        return False
