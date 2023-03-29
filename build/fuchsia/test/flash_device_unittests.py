#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing flash_device.py."""

import os
import subprocess
import unittest
import unittest.mock as mock

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
        config_patcher = mock.patch('flash_device.ScopedFfxConfig',
                                    return_value=context_mock)
        ffx_mock = mock.Mock()
        ffx_mock.returncode = 0
        ffx_patcher = mock.patch('common.run_ffx_command',
                                 return_value=ffx_mock)
        sdk_hash_patcher = mock.patch('flash_device.get_sdk_hash',
                                      return_value=(_TEST_PRODUCT,
                                                    _TEST_VERSION))
        swarming_patcher = mock.patch('flash_device.running_unattended',
                                      return_value=False)
        check_patcher = mock.patch('flash_device.check_ssh_config_file')
        self._config_mock = config_patcher.start()
        self._ffx_mock = ffx_patcher.start()
        self._sdk_hash_mock = sdk_hash_patcher.start()
        self._check_patcher_mock = check_patcher.start()
        self._swarming_mock = swarming_patcher.start()
        self.addCleanup(self._config_mock.stop)
        self.addCleanup(self._ffx_mock.stop)
        self.addCleanup(self._sdk_hash_mock.stop)
        self.addCleanup(self._check_patcher_mock.stop)
        self.addCleanup(self._swarming_mock.stop)

    def test_update_required_on_ignore_returns_immediately(self) -> None:
        """Test |os_check|='ignore' skips all checks."""
        result, new_image_dir = flash_device.update_required(
            'ignore', 'some-image-dir', None)

        self.assertFalse(result)
        self.assertEqual(new_image_dir, 'some-image-dir')

    def test_update_required_raises_value_error_if_no_image_dir(self) -> None:
        """Test |os_check|!='ignore' checks that image dir is non-Falsey."""
        with self.assertRaises(ValueError):
            flash_device.update_required('update', None, None)

    def test_update_required_logs_missing_image_dir(self) -> None:
        """Test |os_check|!='ignore' warns if image dir does not exist."""
        with mock.patch('os.path.exists', return_value=False), \
                mock.patch('flash_device.find_image_in_sdk'), \
                mock.patch('flash_device._get_system_info'), \
                self.assertLogs() as logger:
            flash_device.update_required('update', 'some/image/dir', None)
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
            update_required, new_image_dir = flash_device.update_required(
                'update', 'product-bundle', None)
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
            flash_device.update_required('update', 'product-bundle', None)

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
                '[{"title": "Build", "child": [{"value": "%s"}, ' \
                '{"value": "%s"}]}]' % (_TEST_VERSION, _TEST_PRODUCT)
            flash_device.update(_TEST_IMAGE_DIR, 'check', None)
            self.assertEqual(self._ffx_mock.call_count, 1)
            self.assertEqual(self._sdk_hash_mock.call_count, 1)

    def test_update_system_info_mismatch(self) -> None:
        """Test update when |os_check| is 'check' and system info does not
        match."""

        self._swarming_mock.return_value = True
        with mock.patch('os.path.exists', return_value=True), \
                mock.patch('flash_device._add_exec_to_flash_binaries'), \
                mock.patch('flash_device.subprocess.run'):
            self._ffx_mock.return_value.stdout = \
                '[{"title": "Build", "child": [{"value": "wrong.version"}, ' \
                '{"value": "wrong_product"}]}]'
            flash_device.update(_TEST_IMAGE_DIR,
                                'check',
                                None,
                                should_pave=False)
            self.assertEqual(self._ffx_mock.call_count, 4)

    def test_update_system_info_mismatch_adds_exec_to_flash_binaries(self
                                                                     ) -> None:
        """Test update adds exec bit to flash binaries if flashing."""

        with mock.patch('os.path.exists', return_value=True), \
                mock.patch('flash_device.get_host_arch',
                           return_value='foo_arch'), \
                mock.patch('flash_device.add_exec_to_file') as add_exec:
            self._ffx_mock.return_value.stdout = \
                '[{"title": "Build", "child": [{"value": "wrong.version"}, ' \
                '{"value": "wrong_product"}]}]'
            flash_device.update(_TEST_IMAGE_DIR,
                                'check',
                                None,
                                should_pave=False)
            add_exec.assert_has_calls([
                mock.call(os.path.join(_TEST_IMAGE_DIR, 'flash.sh')),
                mock.call(
                    os.path.join(_TEST_IMAGE_DIR, 'host_foo_arch', 'fastboot'))
            ],
                                      any_order=True)

    def test_update_adds_exec_to_flash_binaries_depending_on_location(
            self) -> None:
        """Test update adds exec bit to flash binaries if flashing."""

        # First exists is for image dir, second is for fastboot binary.
        # Missing this fastboot binary means that the test will default to a
        # different path.
        with mock.patch('os.path.exists', side_effect=[True, False]), \
                mock.patch('flash_device.get_host_arch',
                           return_value='foo_arch'), \
                mock.patch('flash_device.add_exec_to_file') as add_exec:
            self._ffx_mock.return_value.stdout = \
                '[{"title": "Build", "child": [{"value": "wrong.version"}, ' \
                '{"value": "wrong_product"}]}]'
            flash_device.update(_TEST_IMAGE_DIR,
                                'check',
                                None,
                                should_pave=False)
            add_exec.assert_has_calls([
                mock.call(os.path.join(_TEST_IMAGE_DIR, 'flash.sh')),
                mock.call(
                    os.path.join(_TEST_IMAGE_DIR,
                                 'fastboot.exe.linux-foo_arch'))
            ],
                                      any_order=True)

    def test_incorrect_target_info(self) -> None:
        """Test update when |os_check| is 'check' and system info was not
        retrieved."""
        with mock.patch('os.path.exists', return_value=True), \
                mock.patch('flash_device._add_exec_to_flash_binaries'):
            self._ffx_mock.return_value.stdout = '[{"title": "badtitle"}]'
            flash_device.update(_TEST_IMAGE_DIR,
                                'check',
                                None,
                                should_pave=False)
            self.assertEqual(self._ffx_mock.call_count, 3)

    def test_update_with_serial_num(self) -> None:
        """Test update when |serial_num| is specified."""

        with mock.patch('time.sleep'), \
                mock.patch('os.path.exists', return_value=True), \
                mock.patch('flash_device._add_exec_to_flash_binaries'):
            flash_device.update(_TEST_IMAGE_DIR,
                                'update',
                                None,
                                'test_serial',
                                should_pave=False)
        self.assertEqual(self._ffx_mock.call_count, 4)

    def test_reboot_failure(self) -> None:
        """Test update when |serial_num| is specified."""
        self._ffx_mock.return_value.returncode = 1
        with mock.patch('time.sleep'), \
                mock.patch('os.path.exists', return_value=True), \
                mock.patch('flash_device.running_unattended',
                           return_value=True):
            required, _ = flash_device.update_required('check',
                                                       _TEST_IMAGE_DIR, None)
            self.assertEqual(required, True)

    # pylint: disable=no-self-use
    def test_update_calls_paving_if_specified(self) -> None:
        """Test update calls pave if specified."""
        with mock.patch('time.sleep'), \
                mock.patch('os.path.exists', return_value=True), \
                mock.patch('flash_device.running_unattended',
                           return_value=True), \
                mock.patch('flash_device.pave') as mock_pave:
            flash_device.update(_TEST_IMAGE_DIR,
                                'update',
                                'some-target-id',
                                should_pave=True)
            mock_pave.assert_called_once_with(_TEST_IMAGE_DIR,
                                              'some-target-id')

    # pylint: enable=no-self-use

    def test_update_raises_error_if_unattended_with_no_target(self) -> None:
        """Test update calls pave if specified."""

        self._swarming_mock.return_value = True
        with mock.patch('time.sleep'), \
            mock.patch('flash_device.pave'), \
            mock.patch('os.path.exists', return_value=True):
            self.assertRaises(AssertionError,
                              flash_device.update,
                              _TEST_IMAGE_DIR,
                              'update',
                              None,
                              should_pave=True)

    def test_update_on_swarming(self) -> None:
        """Test update on swarming bots."""

        self._swarming_mock.return_value = True
        with mock.patch('time.sleep'), \
             mock.patch('os.path.exists', return_value=True), \
             mock.patch('flash_device._add_exec_to_flash_binaries'), \
             mock.patch('subprocess.run'):
            flash_device.update(_TEST_IMAGE_DIR,
                                'update',
                                None,
                                'test_serial',
                                should_pave=False)
        self.assertEqual(self._ffx_mock.call_count, 3)

    # pylint: disable=no-self-use
    def test_update_with_pave_timeout_defaults_to_flash(self) -> None:
        """Test update falls back to flash if pave fails."""
        with mock.patch('time.sleep'), \
                mock.patch('os.path.exists', return_value=True), \
                mock.patch('flash_device.running_unattended',
                           return_value=True), \
                mock.patch('flash_device.pave') as mock_pave, \
                mock.patch('flash_device.flash') as mock_flash:
            mock_pave.side_effect = subprocess.TimeoutExpired(
                cmd='/some/cmd',
                timeout=0,
            )
            flash_device.update(_TEST_IMAGE_DIR,
                                'update',
                                'some-target-id',
                                should_pave=True)
            mock_pave.assert_called_once_with(_TEST_IMAGE_DIR,
                                              'some-target-id')
            mock_flash.assert_called_once_with(_TEST_IMAGE_DIR,
                                               'some-target-id', None)

    def test_remove_stale_removes_stale_file_lock(self) -> None:
        """Test remove_stale_flash_file_lock removes stale file lock."""
        with mock.patch('time.time') as mock_time, \
             mock.patch('os.remove') as mock_remove, \
             mock.patch('os.stat') as mock_stat:
            mock_time.return_value = 60 * 20
            # Set st_mtime
            mock_stat.return_value = os.stat_result((0, ) * 8 + (100, 0))
            flash_device._remove_stale_flash_file_lock()
            mock_stat.assert_called_once_with(flash_device._FF_LOCK)
            mock_remove.assert_called_once_with(flash_device._FF_LOCK)

    def test_remove_stale_does_not_remove_non_stale_file(self) -> None:
        """Test remove_stale_flash_file_lock does not remove fresh file."""
        with mock.patch('time.time') as mock_time, \
             mock.patch('os.remove') as mock_remove, \
             mock.patch('os.stat') as mock_stat:
            mock_time.return_value = 60 * 10
            # Set st_mtime
            mock_stat.return_value = os.stat_result((0, ) * 8 + (100, 0))
            flash_device._remove_stale_flash_file_lock()
            mock_remove.assert_not_called()

    def test_remove_stale_does_not_raise_file_not_found(self) -> None:
        """Test remove_stale_flash_file_lock does not raise FileNotFound."""
        with mock.patch('time.time'), \
             mock.patch('os.remove'), \
             mock.patch('os.stat') as mock_stat:
            mock_stat.side_effect = FileNotFoundError
            flash_device._remove_stale_flash_file_lock()
            mock_stat.assert_called_once_with(flash_device._FF_LOCK)

    # pylint: enable=no-self-use

    def test_main(self) -> None:
        """Tests |main| function."""

        with mock.patch(
                'sys.argv',
            ['flash_device.py', '--os-check', 'ignore', '--no-pave']):
            with mock.patch.dict(os.environ, {}):
                flash_device.main()
        self.assertEqual(self._ffx_mock.call_count, 0)
# pylint: enable=too-many-public-methods,protected-access


if __name__ == '__main__':
    unittest.main()
