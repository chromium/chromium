#!/usr/bin/env vpython3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing serial_boot_device.py."""

import json
import os
import unittest
import unittest.mock as mock

from subprocess import CompletedProcess

import serial_boot_device

from boot_device import BootMode


# pylint: disable=too-many-public-methods, missing-function-docstring
@mock.patch('shutil.which', return_value='/bin')
class SerialBootDeviceTest(unittest.TestCase):
    """Unittests for serial_boot_device.py."""
    def setUp(self) -> None:
        os.environ['FUCHSIA_NODENAME'] = 'fuchsia-node-id'
        os.environ['FUCHSIA_FASTBOOT_SERNUM'] = 'fuchsia-serial-num'

    def test_does_not_boot_without_binaries(self, *_) -> None:
        with mock.patch('shutil.which', return_value=None):
            self.assertNotEqual(serial_boot_device.main('reboot'), 0)

    @mock.patch('serial_boot_device.is_in_fuchsia', side_effect=[True])
    @mock.patch('subprocess.run',
                return_value=CompletedProcess(args=['/bin'], returncode=0))
    @mock.patch('builtins.print')
    def test_check_health_in_fuchsia(self, mock_print, *_) -> None:
        self.assertEqual(serial_boot_device.main('health-check'), 0)
        result = json.loads(mock_print.call_args.args[0])
        self.assertEqual(result[0]['nodename'], 'fuchsia-node-id')
        self.assertEqual(result[0]['state'], 'healthy')

    @mock.patch('serial_boot_device.is_in_fastboot', side_effect=[True])
    @mock.patch('serial_boot_device.is_in_fuchsia', side_effect=[False])
    @mock.patch('subprocess.run',
                return_value=CompletedProcess(args=['/bin'], returncode=0))
    @mock.patch('builtins.print')
    def test_check_health_in_fastboot(self, mock_print, *_) -> None:
        self.assertEqual(serial_boot_device.main('health-check'), 0)
        result = json.loads(mock_print.call_args.args[0])
        self.assertEqual(result[0]['nodename'], 'fuchsia-node-id')
        self.assertEqual(result[0]['state'], 'healthy')

    @mock.patch('serial_boot_device.is_in_fastboot', side_effect=[False])
    @mock.patch('serial_boot_device.is_in_fuchsia', side_effect=[False])
    @mock.patch('subprocess.run',
                return_value=CompletedProcess(args=['/bin'], returncode=0))
    def test_check_health_undetectable(self, *_) -> None:
        self.assertNotEqual(serial_boot_device.main('health-check'), 0)

    @mock.patch('serial_boot_device.is_in_fuchsia', side_effect=[False])
    @mock.patch('serial_boot_device.is_in_fastboot', side_effect=[False])
    @mock.patch('subprocess.run',
                return_value=CompletedProcess(args=['/bin'], returncode=0))
    def test_boot_undetectable(self, mock_run, *_) -> None:
        self.assertNotEqual(serial_boot_device.main('reboot'), 0)
        mock_run.assert_not_called()

    @mock.patch('serial_boot_device.is_in_fuchsia', side_effect=[True, True])
    @mock.patch('subprocess.run',
                return_value=CompletedProcess(args=['/bin'], returncode=0))
    def test_boot_from_fuchsia_to_fuchsia(self, mock_run, *_) -> None:
        self.assertEqual(serial_boot_device.main('reboot'), 0)
        mock_run.assert_called_once_with(
            ['serialio', 'fuchsia-node-id', 'send', 'dm', 'reboot'])

    @mock.patch('serial_boot_device.is_in_fuchsia', side_effect=[True])
    @mock.patch('subprocess.run',
                return_value=CompletedProcess(args=['/bin'], returncode=0))
    def test_boot_from_fuchsia_to_fuchsia_not_must_boot(self, mock_run,
                                                        *_) -> None:
        self.assertTrue(
            serial_boot_device.boot_device('fuchsia-node-id',
                                           'fuchsia-serial-num',
                                           BootMode.REGULAR,
                                           must_boot=False))
        mock_run.assert_not_called()

    @mock.patch('serial_boot_device.is_in_fuchsia', side_effect=[False, True])
    @mock.patch('serial_boot_device.is_in_fastboot', side_effect=[True])
    @mock.patch('subprocess.run',
                return_value=CompletedProcess(args=['/bin'], returncode=0))
    def test_boot_from_fastboot_to_fuchsia(self, mock_run, *_) -> None:
        self.assertEqual(serial_boot_device.main('reboot'), 0)
        mock_run.assert_called_once_with(
            ['fastboot', 'reboot', '-s', 'fuchsia-serial-num'],
            capture_output=True,
            timeout=30)

    @mock.patch('serial_boot_device.is_in_fuchsia', side_effect=[True])
    @mock.patch('serial_boot_device.is_in_fastboot', side_effect=[True])
    @mock.patch('subprocess.run',
                return_value=CompletedProcess(args=['/bin'], returncode=0))
    def test_boot_from_fuchsia_to_fastboot(self, mock_run, *_) -> None:
        self.assertEqual(serial_boot_device.main('reboot-fastboot'), 0)
        mock_run.assert_called_once_with(
            ['serialio', 'fuchsia-node-id', 'send', 'dm', 'reboot-bootloader'])

    @mock.patch('serial_boot_device.is_in_fuchsia', side_effect=[False])
    @mock.patch('serial_boot_device.is_in_fastboot', side_effect=[True])
    @mock.patch('subprocess.run',
                return_value=CompletedProcess(args=['/bin'], returncode=0))
    def test_boot_from_fastboot_to_fastboot(self, mock_run, *_) -> None:
        self.assertEqual(serial_boot_device.main('reboot-fastboot'), 0)
        mock_run.assert_not_called()


if __name__ == '__main__':
    unittest.main()
