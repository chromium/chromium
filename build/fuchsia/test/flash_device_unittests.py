#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing flash_device.py."""

import os
import unittest
import unittest.mock as mock

import flash_device

_TEST_IMAGE_DIR = 'test/image/dir'
_TEST_PRODUCT = 'test_product'
_TEST_VERSION = 'test.version'


class FlashDeviceTest(unittest.TestCase):
    """Unittests for flash_device.py."""

    def setUp(self) -> None:
        context_mock = mock.Mock()
        context_mock.__enter__ = mock.Mock(return_value=None)
        context_mock.__exit__ = mock.Mock(return_value=None)
        self._config_patcher = mock.patch('flash_device.ScopedFfxConfig',
                                          return_value=context_mock)
        ffx_mock = mock.Mock()
        ffx_mock.returncode = 0
        self._ffx_patcher = mock.patch('flash_device.run_ffx_command',
                                       return_value=ffx_mock)
        self._sdk_hash_patcher = mock.patch('flash_device.get_sdk_hash',
                                            return_value=(_TEST_PRODUCT,
                                                          _TEST_VERSION))
        self._config_mock = self._config_patcher.start()
        self._ffx_mock = self._ffx_patcher.start()
        self._sdk_hash_mock = self._sdk_hash_patcher.start()
        self.addCleanup(self._ffx_mock.stop)
        self.addCleanup(self._sdk_hash_mock.stop)

    def test_flash_ignore(self) -> None:
        """Test setting |os_check| to 'ignore'."""

        flash_device.flash(_TEST_IMAGE_DIR, 'ignore', None)
        self.assertEqual(self._ffx_mock.call_count, 0)
        self.assertEqual(self._sdk_hash_mock.call_count, 0)

    def test_dir_unspecified_value_error(self) -> None:
        """Test ValueError raised when system_image_dir unspecified."""

        with self.assertRaises(ValueError):
            flash_device.flash(None, 'check', None)

    def test_flash_system_info_match(self) -> None:
        """Test no flash when |os_check| is 'check' and system info matches."""

        self._ffx_mock.return_value.stdout = \
            '[{"title": "Build", "child": [{"value": "%s"}, ' \
            '{"value": "%s"}]}]' % (_TEST_VERSION, _TEST_PRODUCT)
        flash_device.flash(_TEST_IMAGE_DIR, 'check', None)
        self.assertEqual(self._ffx_mock.call_count, 1)
        self.assertEqual(self._sdk_hash_mock.call_count, 1)

    def test_flash_system_info_mismatch(self) -> None:
        """Test flash when |os_check| is 'check' and system info does not
        match."""

        self._ffx_mock.return_value.stdout = \
            '[{"title": "Build", "child": [{"value": "wrong.version"}, ' \
            '{"value": "wrong_product"}]}]'
        flash_device.flash(_TEST_IMAGE_DIR, 'check', None)
        self.assertEqual(self._ffx_mock.call_count, 3)

    def test_incorrect_target_info(self) -> None:
        """Test flash when |os_check| is 'check' and system info was not
        retrieved."""

        self._ffx_mock.return_value.stdout = '[{"title": "badtitle"}]'
        flash_device.flash(_TEST_IMAGE_DIR, 'check', None)
        self.assertEqual(self._ffx_mock.call_count, 3)

    def test_flash_with_serial_num(self) -> None:
        """Test flash when |serial_num| is specified."""

        with mock.patch('time.sleep'):
            flash_device.flash(_TEST_IMAGE_DIR, 'update', None, 'test_serial')
        self.assertEqual(self._ffx_mock.call_count, 4)

    def test_flash_on_swarming(self) -> None:
        """Test flash on swarming bots."""

        with mock.patch('time.sleep'), \
             mock.patch('flash_device.running_unattended',
                        return_value = True), \
             mock.patch('subprocess.run'):
            flash_device.flash(_TEST_IMAGE_DIR, 'update', None, 'test_serial')
        self.assertEqual(self._ffx_mock.call_count, 3)

    def test_main(self) -> None:
        """Tests |main| function."""

        with mock.patch('sys.argv',
                        ['flash_device.py', '--os-check', 'ignore']):
            with mock.patch.dict(os.environ, {}):
                flash_device.main()
        self.assertEqual(self._ffx_mock.call_count, 0)


if __name__ == '__main__':
    unittest.main()
