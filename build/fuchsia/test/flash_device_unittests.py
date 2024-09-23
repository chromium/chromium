#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing flash_device.py."""

import os
import unittest
import unittest.mock as mock

import boot_device
import flash_device

_TEST_IMAGE_DIR = 'test/image/dir'
_TEST_PRODUCT = 'test_product'
_TEST_VERSION = 'test.version'


# pylint: disable=too-many-public-methods,protected-access
class FlashDeviceTest(unittest.TestCase):
    """Unittests for flash_device.py."""

    def setUp(self) -> None:
        context_mock = mock.Mock()
        context_mock.__enter__ = mock.Mock(return_value=None)
        context_mock.__exit__ = mock.Mock(return_value=None)
        ffx_mock = mock.Mock()
        ffx_mock.returncode = 0
        ffx_patcher = mock.patch('common.run_ffx_command',
                                 return_value=ffx_mock)
        sdk_hash_patcher = mock.patch('flash_device.get_sdk_hash',
                                      return_value=(_TEST_PRODUCT,
                                                    _TEST_VERSION))
        swarming_patcher = mock.patch('flash_device.running_unattended',
                                      return_value=False)
        time_sleep = mock.patch('time.sleep')
        self._ffx_mock = ffx_patcher.start()
        self._sdk_hash_mock = sdk_hash_patcher.start()
        self._swarming_mock = swarming_patcher.start()
        self._time_sleep = time_sleep.start()
        self.addCleanup(self._ffx_mock.stop)
        self.addCleanup(self._sdk_hash_mock.stop)
        self.addCleanup(self._swarming_mock.stop)
        self.addCleanup(self._time_sleep.stop)

    def test_update_required_on_ignore_returns_immediately(self) -> None:
        """Test |os_check|='ignore' skips all checks."""
        result, new_image_dir = flash_device._update_required(
            'ignore', 'some-image-dir', None)

        self.assertFalse(result)
        self.assertEqual(new_image_dir, 'some-image-dir')

    def test_update_required_raises_value_error_if_no_image_dir(self) -> None:
        """Test |os_check|!='ignore' checks that image dir is non-Falsey."""
        with self.assertRaises(ValueError):
            flash_device._update_required('update', None, None)

    def test_update_required_logs_missing_image_dir(self) -> None:
        """Test |os_check|!='ignore' warns if image dir does not exist."""
        with mock.patch('os.path.exists', return_value=False), \
                mock.patch('flash_device.find_image_in_sdk'), \
                mock.patch('flash_device._get_system_info'), \
                self.assertLogs() as logger:
            flash_device._update_required('update', 'some/image/dir', None)
            self.assertIn('image directory does not exist', logger.output[0])

    def test_update_required_searches_and_returns_sdk_if_image_found(self
                                                                     ) -> None:
        """Test |os_check|!='ignore' searches for image dir in SDK."""
        with mock.patch('os.path.exists', return_value=False), \
                mock.patch('flash_device.find_image_in_sdk') as mock_find, \
                mock.patch('flash_device._get_system_info'), \
                mock.patch('common.SDK_ROOT', 'path/to/sdk/dir'), \
                self.assertLogs():
            mock_find.return_value = 'path/to/image/dir'
            update_required, new_image_dir = flash_device._update_required(
                'update', 'product-bundle', None, None)
            self.assertTrue(update_required)
            self.assertEqual(new_image_dir, 'path/to/image/dir')
            mock_find.assert_called_once_with('product-bundle')

    def test_update_required_raises_file_not_found_error(self) -> None:
        """Test |os_check|!='ignore' raises FileNotFoundError if no path."""
        with mock.patch('os.path.exists', return_value=False), \
                mock.patch('flash_device.find_image_in_sdk',
                           return_value=None), \
                mock.patch('common.SDK_ROOT', 'path/to/sdk/dir'), \
                self.assertLogs(), \
                self.assertRaises(FileNotFoundError):
            flash_device._update_required('update', 'product-bundle', None)

    def test_update_ignore(self) -> None:
        """Test setting |os_check| to 'ignore'."""

        flash_device.update(_TEST_IMAGE_DIR, 'ignore', None)
        self.assertEqual(self._ffx_mock.call_count, 0)
        self.assertEqual(self._sdk_hash_mock.call_count, 0)

    def test_dir_unspecified_value_error(self) -> None:
        """Test ValueError raised when system_image_dir unspecified."""

        with self.assertRaises(ValueError):
            flash_device.update(None, 'check', None)

    def test_update_system_info_match(self) -> None:
        """Test no update when |os_check| is 'check' and system info matches."""

        with mock.patch('os.path.exists', return_value=True):
            self._ffx_mock.return_value.stdout = \
                '{"build": {"version": "%s", ' \
                '"product": "%s"}}' % (_TEST_VERSION, _TEST_PRODUCT)
            flash_device.update(_TEST_IMAGE_DIR, 'check', None)
            self.assertEqual(self._ffx_mock.call_count, 1)
            self.assertEqual(self._sdk_hash_mock.call_count, 1)

    def test_update_system_info_catches_boot_failure(self) -> None:
        """Test update when |os_check=check| catches boot_device exceptions."""

        self._swarming_mock.return_value = True
        with mock.patch('os.path.exists', return_value=True), \
                mock.patch('flash_device.boot_device') as mock_boot, \
                mock.patch('flash_device.get_system_info') as mock_sys_info, \
                mock.patch('flash_device.subprocess.run'):
            mock_boot.side_effect = boot_device.StateTransitionError(
                'Incorrect state')
            self._ffx_mock.return_value.stdout = \
                '{"build": {"version": "wrong.version", ' \
                '"product": "wrong.product"}}'
            flash_device.update(_TEST_IMAGE_DIR, 'check', None)
            mock_boot.assert_called_with(mock.ANY,
                                         boot_device.BootMode.REGULAR, None)
            self.assertEqual(self._ffx_mock.call_count, 1)

            # get_system_info should not even be called due to early exit.
            mock_sys_info.assert_not_called()

    def test_update_system_info_mismatch(self) -> None:
        """Test update when |os_check| is 'check' and system info does not
        match."""

        self._swarming_mock.return_value = True
        with mock.patch('os.path.exists', return_value=True), \
                mock.patch('flash_device.boot_device') as mock_boot, \
                mock.patch('flash_device.subprocess.run'):
            self._ffx_mock.return_value.stdout = \
                '{"build": {"version": "wrong.version", ' \
                '"product": "wrong.product"}}'
            flash_device.update(_TEST_IMAGE_DIR, 'check', None)
            mock_boot.assert_called_with(mock.ANY,
                                         boot_device.BootMode.REGULAR, None)
            self.assertEqual(self._ffx_mock.call_count, 2)

    def test_incorrect_target_info(self) -> None:
        """Test update when |os_check| is 'check' and system info was not
        retrieved."""
        with mock.patch('os.path.exists', return_value=True):
            self._ffx_mock.return_value.stdout = '{"unexpected": "badtitle"}'
            flash_device.update(_TEST_IMAGE_DIR, 'check', None)
            self.assertEqual(self._ffx_mock.call_count, 2)

    def test_update_with_serial_num(self) -> None:
        """Test update when |serial_num| is specified."""

        with mock.patch('time.sleep'), \
                mock.patch('os.path.exists', return_value=True), \
                mock.patch('flash_device.boot_device') as mock_boot:
            flash_device.update(_TEST_IMAGE_DIR, 'update', None, 'test_serial')
            mock_boot.assert_called_with(mock.ANY,
                                         boot_device.BootMode.BOOTLOADER,
                                         'test_serial')
        self.assertEqual(self._ffx_mock.call_count, 1)

    def test_reboot_failure(self) -> None:
        """Test update when |serial_num| is specified."""
        self._ffx_mock.return_value.returncode = 1
        with mock.patch('time.sleep'), \
                mock.patch('os.path.exists', return_value=True), \
                mock.patch('flash_device.running_unattended',
                           return_value=True), \
                mock.patch('flash_device.boot_device'):
            required, _ = flash_device._update_required(
                'check', _TEST_IMAGE_DIR, None)
            self.assertEqual(required, True)

    def test_update_on_swarming(self) -> None:
        """Test update on swarming bots."""

        self._swarming_mock.return_value = True
        with mock.patch('time.sleep'), \
             mock.patch('os.path.exists', return_value=True), \
             mock.patch('flash_device.boot_device') as mock_boot, \
             mock.patch('subprocess.run'):
            flash_device.update(_TEST_IMAGE_DIR, 'update', None, 'test_serial')
            mock_boot.assert_called_with(mock.ANY,
                                         boot_device.BootMode.BOOTLOADER,
                                         'test_serial')
        self.assertEqual(self._ffx_mock.call_count, 1)

    def test_main(self) -> None:
        """Tests |main| function."""

        with mock.patch('sys.argv',
                        ['flash_device.py', '--os-check', 'ignore']):
            with mock.patch.dict(os.environ, {}):
                flash_device.main()
        self.assertEqual(self._ffx_mock.call_count, 0)
# pylint: enable=too-many-public-methods,protected-access


if __name__ == '__main__':
    unittest.main()
