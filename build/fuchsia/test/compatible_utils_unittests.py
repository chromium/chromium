#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing compatible_utils.py."""

import io
import json
import os
import stat
import tempfile
import unittest
import unittest.mock as mock

import compatible_utils

# Allow access to constants for testing.
# pylint: disable=protected-access

@unittest.skipIf(os.name == 'nt', 'Fuchsia tests not supported on Windows')
class CompatibleUtilsTest(unittest.TestCase):
    """Test compatible_utils.py methods."""

    def test_running_unattended_returns_true_if_headless_set(self) -> None:
        """Test |running_unattended| returns True if CHROME_HEADLESS is set."""
        with mock.patch('os.environ', {compatible_utils._CHROME_HEADLESS: 0}):
            self.assertTrue(compatible_utils.running_unattended())

        with mock.patch('os.environ', {'FOO_HEADLESS': 0}):
            self.assertFalse(compatible_utils.running_unattended())

    def test_get_host_arch(self) -> None:
        """Test |get_host_arch| gets the host architecture and throws
        exceptions on errors."""
        supported_arches = ['x86_64', 'AMD64', 'aarch64']
        with mock.patch('platform.machine', side_effect=supported_arches):
            self.assertEqual(compatible_utils.get_host_arch(), 'x64')
            self.assertEqual(compatible_utils.get_host_arch(), 'x64')
            self.assertEqual(compatible_utils.get_host_arch(), 'arm64')

        with mock.patch('platform.machine', return_value=['fake-arch']), \
                self.assertRaises(NotImplementedError):
            compatible_utils.get_host_arch()

    def test_add_exec_to_file(self) -> None:
        """Test |add_exec_to_file| adds executable bit to file."""
        with tempfile.NamedTemporaryFile() as f:
            original_stat = os.stat(f.name).st_mode
            self.assertFalse(original_stat & stat.S_IXUSR)

            compatible_utils.add_exec_to_file(f.name)

            new_stat = os.stat(f.name).st_mode
            self.assertTrue(new_stat & stat.S_IXUSR)

    def test_map_filter_filter_file_throws_value_error_if_wrong_path(self
                                                                     ) -> None:
        """Test |map_filter_file| throws ValueError if path is missing
        FILTER_DIR."""
        with self.assertRaises(ValueError):
            compatible_utils.map_filter_file_to_package_file('foo')

        with self.assertRaises(ValueError):
            compatible_utils.map_filter_file_to_package_file('some/other/path')

        with self.assertRaises(ValueError):
            compatible_utils.map_filter_file_to_package_file('filters/file')

        # No error.
        compatible_utils.map_filter_file_to_package_file(
            'testing/buildbot/filters/some.filter')

    def test_map_filter_filter_replaces_filter_dir_with_pkg_path(self) -> None:
        """Test |map_filter_file| throws ValueError if path is missing
        FILTER_DIR."""
        self.assertEqual(
            '/pkg/testing/buildbot/filters/some.filter',
            compatible_utils.map_filter_file_to_package_file(
                'foo/testing/buildbot/filters/some.filter'))

    def test_get_sdk_hash_success(self) -> None:
        """Test |get_sdk_hash| reads product_bundle.json."""
        with mock.patch('builtins.open',
                        return_value=io.StringIO(
                            json.dumps({'product_version': '12345'}))):
            self.assertEqual(
                compatible_utils.get_sdk_hash(
                    'third_party/fuchsia-sdk/images-internal/sherlock-release/'
                    'smart_display_max_eng_arrested/'),
                ('smart_display_max_eng_arrested', '12345'))

    def test_get_sdk_hash_normalize_path(self) -> None:
        """Test |get_sdk_hash| uses path as product."""
        with mock.patch('builtins.open',
                        return_value=io.StringIO(
                            json.dumps({'product_version': '23456'}))):
            self.assertEqual(
                compatible_utils.get_sdk_hash(
                    'third_party/fuchsia-sdk/images-internal/sherlock-release/'
                    'smart_display_max_eng_arrested'),
                ('smart_display_max_eng_arrested', '23456'))

    def test_get_sdk_hash_not_found(self) -> None:
        """Test |get_sdk_hash| fails if the product_bundle.json does not exist.
        """
        with mock.patch('builtins.open', side_effect=IOError()):
            self.assertRaises(IOError, compatible_utils.get_sdk_hash,
                              'some/image/dir')

    def test_install_symbols(self):
        """Test |install_symbols|."""
        def trim_noop_prefixes(path):
            """Helper function to trim no-op path name prefixes that are
            introduced by os.path.realpath on some platforms. These break
            the unit tests, but have no actual effect on behavior."""
            # These must all end in the path separator character for the
            # string length computation to be correct on all platforms.
            noop_prefixes = ['/private/']
            for prefix in noop_prefixes:
                if path.startswith(prefix):
                    return path[len(prefix) - 1:]
            return path

        with tempfile.TemporaryDirectory() as fuchsia_out_dir:
            build_id = 'test_build_id'
            symbol_file = os.path.join(fuchsia_out_dir, '.build-id',
                                       build_id[:2], build_id[2:] + '.debug')
            id_path = os.path.join(fuchsia_out_dir, 'ids.txt')
            try:
                binary_relpath = 'path/to/binary'
                with open(id_path, 'w') as f:
                    f.write(f'{build_id} {binary_relpath}')
                compatible_utils.install_symbols([id_path], fuchsia_out_dir)
                self.assertTrue(os.path.islink(symbol_file))
                self.assertEqual(
                    trim_noop_prefixes(os.path.realpath(symbol_file)),
                    os.path.join(fuchsia_out_dir, binary_relpath))

                new_binary_relpath = 'path/to/new/binary'
                with open(id_path, 'w') as f:
                    f.write(f'{build_id} {new_binary_relpath}')
                compatible_utils.install_symbols([id_path], fuchsia_out_dir)
                self.assertTrue(os.path.islink(symbol_file))
                self.assertEqual(
                    trim_noop_prefixes(os.path.realpath(symbol_file)),
                    os.path.join(fuchsia_out_dir, new_binary_relpath))
            finally:
                os.remove(id_path)


    def test_ssh_keys(self):
        """Ensures the get_ssh_keys won't return a None."""
        self.assertIsNotNone(compatible_utils.get_ssh_keys())


    def test_force_running_unattended(self) -> None:
        """Test |force_running_unattended|."""
        # force switching the states twice no matter which state we start in.
        for _ in range(2):
            compatible_utils.force_running_attended()
            self.assertFalse(compatible_utils.running_unattended())
            compatible_utils.force_running_unattended()
            self.assertTrue(compatible_utils.running_unattended())


if __name__ == '__main__':
    unittest.main()
