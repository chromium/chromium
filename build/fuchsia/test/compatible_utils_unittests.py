#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing compatible_utils.py."""

import io
import os
import stat
import tempfile
import unittest
import unittest.mock as mock

import compatible_utils


@unittest.skipIf(os.name == 'nt', 'Fuchsia tests not supported on Windows')
class CompatibleUtilsTest(unittest.TestCase):
    """Test compatible_utils.py methods."""

    def test_running_unattended_returns_true_if_headless_set(self) -> None:
        """Test |running_unattended| returns True if CHROME_HEADLESS is set."""
        with mock.patch('os.environ', {'SWARMING_SERVER': 0}):
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

    # pylint: disable=no-self-use
    def test_pave_adds_exec_to_binary_files(self) -> None:
        """Test |pave| calls |add_exec_to_file| on necessary files."""
        with mock.patch('os.path.exists', return_value=True), \
                mock.patch('compatible_utils.add_exec_to_file') as mock_exec, \
                mock.patch('platform.machine', return_value='x86_64'), \
                mock.patch('subprocess.run'):
            compatible_utils.pave('some/path/to/dir', 'some-target')

            mock_exec.assert_has_calls([
                mock.call('some/path/to/dir/pave.sh'),
                mock.call('some/path/to/dir/host_x64/bootserver')
            ],
                                       any_order=True)

    def test_pave_adds_exec_to_binary_files_if_pb_set_not_found(self) -> None:
        """Test |pave| calls |add_exec_to_file| on necessary files.

        Checks if current product-bundle files exist. If not, defaults to
        prebuilt-images set.
        """
        with mock.patch('os.path.exists', return_value=False), \
                mock.patch('compatible_utils.add_exec_to_file') as mock_exec, \
                mock.patch('platform.machine', return_value='x86_64'), \
                mock.patch('subprocess.run'):
            compatible_utils.pave('some/path/to/dir', 'some-target')

            mock_exec.assert_has_calls([
                mock.call('some/path/to/dir/pave.sh'),
                mock.call('some/path/to/dir/bootserver.exe.linux-x64')
            ],
                                       any_order=True)

    def test_pave_adds_target_id_if_given(self) -> None:
        """Test |pave| adds target-id to the arguments."""
        with mock.patch('os.path.exists', return_value=False), \
                mock.patch('compatible_utils.add_exec_to_file'), \
                mock.patch('platform.machine', return_value='x86_64'), \
                mock.patch('compatible_utils.get_ssh_keys',
                           return_value='authorized-keys-file'), \
                mock.patch('subprocess.run') as mock_subproc:
            mock_subproc.reset_mock()
            compatible_utils.pave('some/path/to/dir', 'some-target')

            mock_subproc.assert_called_once_with([
                'some/path/to/dir/pave.sh', '--authorized-keys',
                'authorized-keys-file', '-1', '-n', 'some-target'
            ],
                                                 check=True,
                                                 text=True,
                                                 timeout=300)

    # pylint: disable=no-self-use

    def test_parse_host_port_splits_address_and_strips_brackets(self) -> None:
        """Test |parse_host_port| splits ipv4 and ipv6 addresses correctly."""
        self.assertEqual(compatible_utils.parse_host_port('hostname:55'),
                         ('hostname', 55))
        self.assertEqual(compatible_utils.parse_host_port('192.168.42.40:443'),
                         ('192.168.42.40', 443))
        self.assertEqual(
            compatible_utils.parse_host_port('[2001:db8::1]:8080'),
            ('2001:db8::1', 8080))

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

    def test_get_sdk_hash_fallsback_to_args_file_if_buildargs_dne(self
                                                                  ) -> None:
        """Test |get_sdk_hash| checks if buildargs.gn exists.

        If it does not, fallsback to args.gn. This should raise an exception
        as it does not exist.
        """
        with mock.patch('os.path.exists', return_value=False) as mock_exists, \
                self.assertRaises(compatible_utils.VersionNotFoundError):
            compatible_utils.get_sdk_hash('some/image/dir')
        mock_exists.assert_has_calls([
            mock.call('some/image/dir/buildargs.gn'),
            mock.call('some/image/dir/args.gn')
        ])

    def test_get_sdk_hash_parse_contents_of_args_file(self) -> None:
        """Test |get_sdk_hash| parses buildargs contents correctly."""
        build_args_test_contents = """
build_info_board = "chromebook-x64"
build_info_product = "workstation_eng"
build_info_version = "10.20221114.2.1"
universe_package_labels += []
"""
        with mock.patch('os.path.exists', return_value=True), \
                mock.patch('builtins.open',
                           return_value=io.StringIO(build_args_test_contents)):
            self.assertEqual(compatible_utils.get_sdk_hash('some/dir'),
                             ('workstation_eng', '10.20221114.2.1'))

    def test_get_sdk_hash_raises_error_if_keys_missing(self) -> None:
        """Test |get_sdk_hash| raises VersionNotFoundError if missing keys"""
        build_args_test_contents = """
import("//boards/chromebook-x64.gni")
import("//products/workstation_eng.gni")
cxx_rbe_enable = true
host_labels += [ "//bundles/infra/build" ]
universe_package_labels += []
"""
        with mock.patch('os.path.exists', return_value=True), \
                mock.patch(
                    'builtins.open',
                    return_value=io.StringIO(build_args_test_contents)), \
                self.assertRaises(compatible_utils.VersionNotFoundError):
            compatible_utils.get_sdk_hash('some/dir')

    def test_get_sdk_hash_raises_error_if_contents_empty(self) -> None:
        """Test |get_sdk_hash| raises VersionNotFoundError if no contents."""
        with mock.patch('os.path.exists', return_value=True), \
                mock.patch('builtins.open', return_value=io.StringIO("")), \
                self.assertRaises(compatible_utils.VersionNotFoundError):
            compatible_utils.get_sdk_hash('some/dir')

    def trim_noop_prefixes(self, path):
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

    def test_install_symbols(self):

        """Test |install_symbols|."""

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
                    self.trim_noop_prefixes(os.path.realpath(symbol_file)),
                    os.path.join(fuchsia_out_dir, binary_relpath))

                new_binary_relpath = 'path/to/new/binary'
                with open(id_path, 'w') as f:
                    f.write(f'{build_id} {new_binary_relpath}')
                compatible_utils.install_symbols([id_path], fuchsia_out_dir)
                self.assertTrue(os.path.islink(symbol_file))
                self.assertEqual(
                    self.trim_noop_prefixes(os.path.realpath(symbol_file)),
                    os.path.join(fuchsia_out_dir, new_binary_relpath))
            finally:
                os.remove(id_path)


if __name__ == '__main__':
    unittest.main()
