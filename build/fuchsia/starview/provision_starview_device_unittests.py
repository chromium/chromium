#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing provision_starview_device.py."""

import os
import subprocess
import unittest
from unittest import mock

import provision_starview_device


@unittest.skipIf(os.name == 'nt', 'Fuchsia tests not supported on Windows')
class ProvisionStarviewDeviceTest(unittest.TestCase):
    """Tests for provision_starview_device.py."""

    def test_resolve_devices_explicit_and_env(self) -> None:
        self.assertEqual(
            provision_starview_device.resolve_devices(
                '/usr/bin/adb', devices=('dev-1', 'dev-2')
            ),
            ['dev-1', 'dev-2'],
        )
        with mock.patch.dict(os.environ, {'ANDROID_SERIAL': 'env-1'}):
            self.assertEqual(
                provision_starview_device.resolve_devices('/usr/bin/adb'),
                ['env-1'],
            )

    @mock.patch('provision_starview.run_adb')
    def test_resolve_devices_from_adb_devices(
        self, mock_run_adb: mock.MagicMock
    ) -> None:
        mock_run_adb.return_value = subprocess.CompletedProcess(
            [],
            0,
            'List of devices attached\n'
            'usb-1\tdevice\n'
            'usb-2\toffline\n'
            '127.0.0.1:6525\tdevice\n',
            '',
        )
        with mock.patch.dict(os.environ, {}, clear=True):
            self.assertEqual(
                provision_starview_device.resolve_devices('/usr/bin/adb'),
                ['usb-1', '127.0.0.1:6525'],
            )

    @mock.patch('provision_starview.run_adb')
    def test_resolve_devices_none_found(
        self, mock_run_adb: mock.MagicMock
    ) -> None:
        mock_run_adb.return_value = subprocess.CompletedProcess(
            [], 0, 'List of devices attached\n\n', ''
        )
        with mock.patch.dict(os.environ, {}, clear=True):
            with self.assertRaises(RuntimeError):
                provision_starview_device.resolve_devices('/usr/bin/adb')

    @mock.patch('provision_starview.run_shell')
    def test_force_stop_packages(self, mock_shell: mock.MagicMock) -> None:
        mock_shell.side_effect = [
            'package:org.chromium.chrome\n'
            'package:com.android.settings\n'
            'not-a-package\n',
            '',
        ]

        provision_starview_device.force_stop_packages('/usr/bin/adb', 'usb-123')

        mock_shell.assert_has_calls(
            [
                mock.call(
                    '/usr/bin/adb',
                    'usb-123',
                    ['pm', 'list', 'packages', 'org.chromium'],
                ),
                mock.call(
                    '/usr/bin/adb',
                    'usb-123',
                    ['am', 'force-stop', 'org.chromium.chrome'],
                ),
            ]
        )

    @mock.patch('provision_starview_device.force_stop_packages')
    @mock.patch('provision_starview.run_shell')
    @mock.patch('provision_starview.provision_device')
    def test_provision_physical_device(
        self,
        mock_provision: mock.MagicMock,
        mock_shell: mock.MagicMock,
        mock_force_stop: mock.MagicMock,
    ) -> None:
        provision_starview_device.provision_physical_device(
            'usb-123', adb_path='/usr/bin/adb'
        )

        mock_provision.assert_called_once_with(
            'usb-123', adb_path='/usr/bin/adb'
        )
        mock_shell.assert_called_once_with(
            '/usr/bin/adb',
            'usb-123',
            'settings put system screen_brightness_mode 0; '
            'settings put system screen_brightness 1',
            check=False,
        )
        mock_force_stop.assert_called_once_with('/usr/bin/adb', 'usb-123')


if __name__ == '__main__':
    unittest.main()
