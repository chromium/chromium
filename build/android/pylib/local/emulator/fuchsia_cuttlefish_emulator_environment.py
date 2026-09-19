# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import subprocess
import sys

from devil.android import device_utils
from pylib import constants
from pylib.local.device import local_device_environment

_DEFAULT_ADB_PORT = 6525

CUTTLEFISH_SCRIPT = os.path.join(
    constants.DIR_SOURCE_ROOT,
    'build',
    'fuchsia',
    'starview',
    'run_cuttlefish.py',
)


_CUTTLEFISH_DIR = os.path.abspath(
    os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        os.pardir,
        os.pardir,
        os.pardir,
        os.pardir,
        os.pardir,
        'cuttlefish',
    )
)


# TODO(crbug.com/561302001): Fuchsia/Starview tests should explicitly pass
# an emulator flag instead of inferring Cuttlefish from script existence
# and environment variables.
def IsSupported():
    return (
        bool(os.environ.get('ISOLATED_OUTDIR'))
        and os.path.exists(CUTTLEFISH_SCRIPT)
        and os.path.exists(_CUTTLEFISH_DIR)
    )


class FuchsiaCuttlefishEmulatorEnvironment(
    local_device_environment.LocalDeviceEnvironment
):
    def __init__(self, args, output_manager, error_func):
        super().__init__(args, output_manager, error_func)
        self._instance = CuttlefishInstance(_DEFAULT_ADB_PORT)

    # override
    def SetUp(self):
        self._device_serials = [self._instance.Start()]
        super().SetUp()

    # override
    def TearDown(self):
        try:
            super().TearDown()
        finally:
            self._instance.Stop()


class CuttlefishInstance:
    """Manages the lifecycle of a Cuttlefish emulator instance."""

    def __init__(self, adb_port=_DEFAULT_ADB_PORT, adb_path=None):
        self._adb_port = adb_port
        self._adb_path = adb_path
        self._proc = None
        self._orig_wait_until_fully_booted = None

    def Start(self):
        assert os.path.exists(CUTTLEFISH_SCRIPT), (
            f'Cuttlefish script not found at {CUTTLEFISH_SCRIPT}'
        )
        cmd = [
            sys.executable,
            CUTTLEFISH_SCRIPT,
            '--adb-port',
            str(self._adb_port),
            '--headless',
        ]
        if self._adb_path:
            cmd.extend(['--adb-path', str(self._adb_path)])
        self._proc = subprocess.Popen(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True
        )

        ready = False
        for line in iter(self._proc.stdout.readline, ''):
            logging.info('[Cuttlefish] %s', line.strip())
            if 'Press Ctrl+C to terminate the emulator.' in line:
                ready = True
                break

        if not ready:
            raise RuntimeError(
                'Cuttlefish process exited prematurely with code '
                f'{self._proc.poll()}'
            )

        self._orig_wait_until_fully_booted = (
            device_utils.DeviceUtils.WaitUntilFullyBooted
        )

        def _starnix_wait_until_fully_booted(device_self, *args, **kwargs):
            try:
                # pylint: disable=protected-access
                device_self._cache['external_storage'] = '/data/local/tmp'
                device_self._cache['current_user'] = 0
                device_self._cache['needs_su'] = False
            except Exception:  # pylint: disable=broad-except
                pass

            try:
                self._orig_wait_until_fully_booted(device_self, *args, **kwargs)
            except Exception as e:  # pylint: disable=broad-except
                if 'is_sd_card_ready' in str(e):
                    logging.warning(
                        'Ignoring is_sd_card_ready timeout on Starnix '
                        'device: %s',
                        e,
                    )
                else:
                    raise

        device_utils.DeviceUtils.WaitUntilFullyBooted = (
            _starnix_wait_until_fully_booted
        )

        return f'127.0.0.1:{self._adb_port}'

    def Stop(self):
        if self._orig_wait_until_fully_booted:
            device_utils.DeviceUtils.WaitUntilFullyBooted = (
                self._orig_wait_until_fully_booted
            )
            self._orig_wait_until_fully_booted = None
        if self._proc:
            logging.info('Terminating Cuttlefish process...')
            self._proc.terminate()
            try:
                self._proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self._proc.kill()
            self._proc = None
