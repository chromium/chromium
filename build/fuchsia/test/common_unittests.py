#!/usr/bin/env vpython3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing common.py."""

import os
import tempfile
import unittest
import unittest.mock as mock

from types import SimpleNamespace

import common


# Tests should use their names to explain the meaning of the tests rather than
# relying on the extra docstrings.
# pylint: disable=missing-function-docstring
@unittest.skipIf(os.name == 'nt', 'Fuchsia tests not supported on Windows')
class CommonTest(unittest.TestCase):
    """Test common.py methods."""
    def test_find_in_dir_returns_file_or_dir_if_searching(self) -> None:
        """Test |find_in_dir| returns files if searching for file, or None."""
        # Make the directory structure.
        with tempfile.TemporaryDirectory() as tmp_dir:
            with tempfile.NamedTemporaryFile(dir=tmp_dir) as tmp_file, \
                tempfile.TemporaryDirectory(dir=tmp_dir) as inner_tmp_dir:

                # Structure is now:
                # temp_dir/
                # temp_dir/inner_dir1
                # temp_dir/tempfile1
                # File is not a dir, so returns None.
                self.assertIsNone(
                    common.find_in_dir(os.path.basename(tmp_file.name),
                                       parent_dir=tmp_dir))

                # Repeat for directory.
                self.assertEqual(
                    common.find_in_dir(inner_tmp_dir, parent_dir=tmp_dir),
                    inner_tmp_dir)

    def test_find_image_in_sdk_searches_images_in_product_bundle(self):
        """Test |find_image_in_sdk| searches for 'images' if product-bundle."""
        with tempfile.TemporaryDirectory() as tmp_dir:
            os.makedirs(os.path.join(tmp_dir, 'sdk'), exist_ok=True)
            os.makedirs(os.path.join(tmp_dir, 'images', 'workstation-product',
                                     'images'),
                        exist_ok=True)
            with mock.patch('common.SDK_ROOT', os.path.join(tmp_dir, 'sdk')):
                self.assertEqual(
                    common.find_image_in_sdk('workstation-product'),
                    os.path.join(tmp_dir, 'images', 'workstation-product',
                                 'images'))

    def test_images_root_should_not_end_with_path_sep(self):
        """INTERNAL_IMAGES_ROOT appends -internal at the end of the IMAGES_ROOT,
        so the later one should not end with a /, otherwise the folder name will
        become 'images/-internal'."""
        # Avoid the logic being bypassed.
        self.assertIsNone(os.environ.get('FUCHSIA_INTERNAL_IMAGES_ROOT'))
        self.assertFalse(common.IMAGES_ROOT.endswith(os.path.sep))

    @mock.patch('common.run_ffx_command')
    def test_get_system_info_parse_version_and_product(self, ffx_mock):
        ffx_mock.return_value = SimpleNamespace(
            returncode=0, stdout='{"build": {"version": "v", "product": "p"}}')
        self.assertEqual(common.get_system_info(), ('p', 'v'))

    @mock.patch('common.run_ffx_command')
    def test_get_system_info_parse_version_only(self, ffx_mock):
        ffx_mock.return_value = SimpleNamespace(
            returncode=0, stdout='{"build": {"version": "v"}}')
        self.assertEqual(common.get_system_info(), ('', 'v'))

    @mock.patch('common.run_ffx_command')
    def test_get_system_info_ffx_error(self, ffx_mock):
        ffx_mock.return_value = SimpleNamespace(returncode=100,
                                                stdout='{"build": {}}')
        self.assertEqual(common.get_system_info(), ('', ''))

    @mock.patch('common.run_ffx_command')
    def test_get_system_info_never_returns_none(self, ffx_mock):
        ffx_mock.return_value = SimpleNamespace(returncode=0,
                                                stdout='{"build": {}}')
        self.assertEqual(common.get_system_info(), ('', ''))

    @mock.patch('common.run_ffx_command')
    def test_get_system_info_ignore_no_build(self, ffx_mock):
        ffx_mock.return_value = SimpleNamespace(
            returncode=0, stdout='{"thisisnotbuild": {}}')
        self.assertEqual(common.get_system_info(), ('', ''))

    @mock.patch('common.run_ffx_command')
    def test_get_system_info_ignore_bad_build_type(self, ffx_mock):
        ffx_mock.return_value = SimpleNamespace(returncode=0,
                                                stdout='{"build": []}')
        self.assertEqual(common.get_system_info(), ('', ''))

    @mock.patch('common.run_ffx_command')
    def test_get_system_info_ignore_bad_build_type2(self, ffx_mock):
        ffx_mock.return_value = SimpleNamespace(returncode=0,
                                                stdout='{"build": "hello"}')
        self.assertEqual(common.get_system_info(), ('', ''))

    @mock.patch('common.run_ffx_command')
    def test_get_system_info_not_a_json(self, ffx_mock):
        ffx_mock.return_value = SimpleNamespace(returncode=0, stdout='hello')
        self.assertEqual(common.get_system_info(), ('', ''))

if __name__ == '__main__':
    unittest.main()
