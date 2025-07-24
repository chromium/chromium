#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for build_information.py."""

# pylint: disable=protected-access

import os
import sys
import unittest
from unittest import mock

# vpython-provided modules
# pylint: disable=import-error
from pyfakefs import fake_filesystem_unittest
# pylint: enable=import-error

# pylint: disable=wrong-import-position
sys.path.insert(0,
                os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
from mcp_servers import build_information
# pylint: enable=wrong-import-position

CHROMIUM_ROOT = build_information.CHROMIUM_ROOT


class GetHostOsTest(unittest.TestCase):

    @mock.patch('sys.platform', 'linux')
    def test_get_host_os_linux(self):
        """Test get_host_os on linux."""
        self.assertEqual(build_information.get_host_os(),
                         build_information.ValidOs.LINUX)

    @mock.patch('sys.platform', 'cygwin')
    def test_get_host_os_cygwin_is_linux(self):
        """Test get_host_os on cygwin returns linux."""
        self.assertEqual(build_information.get_host_os(),
                         build_information.ValidOs.LINUX)

    @mock.patch('sys.platform', 'win32')
    def test_get_host_os_win(self):
        """Test get_host_os on windows."""
        self.assertEqual(build_information.get_host_os(),
                         build_information.ValidOs.WIN)

    @mock.patch('sys.platform', 'darwin')
    def test_get_host_os_mac(self):
        """Test get_host_os on mac."""
        self.assertEqual(build_information.get_host_os(),
                         build_information.ValidOs.MAC)

    @mock.patch('sys.platform', 'freebsd')
    def test_get_host_os_unknown(self):
        """Test get_host_os on an unknown os."""
        self.assertEqual(build_information.get_host_os(),
                         build_information.ValidOs.UNKNOWN)


class GetHostArchTest(unittest.TestCase):

    @mock.patch('mcp_servers.build_information._get_host_bits',
                return_value=build_information.Bitness.SIXTY_FOUR)
    @mock.patch('mcp_servers.build_information._get_host_architecture',
                return_value=build_information.Architecture.INTEL)
    def test_get_host_arch_x64(self, _mock_arch, _mock_bits):
        """Test get_host_arch for x64."""
        self.assertEqual(build_information.get_host_arch(),
                         build_information.ValidArch.X64)

    @mock.patch('mcp_servers.build_information._get_host_bits',
                return_value=build_information.Bitness.THIRTY_TWO)
    @mock.patch('mcp_servers.build_information._get_host_architecture',
                return_value=build_information.Architecture.INTEL)
    def test_get_host_arch_x86(self, _mock_arch, _mock_bits):
        """Test get_host_arch for x86."""
        self.assertEqual(build_information.get_host_arch(),
                         build_information.ValidArch.X86)

    @mock.patch('mcp_servers.build_information._get_host_bits',
                return_value=build_information.Bitness.SIXTY_FOUR)
    @mock.patch('mcp_servers.build_information._get_host_architecture',
                return_value=build_information.Architecture.ARM)
    def test_get_host_arch_arm64(self, _mock_arch, _mock_bits):
        """Test get_host_arch for arm64."""
        self.assertEqual(build_information.get_host_arch(),
                         build_information.ValidArch.ARM64)

    @mock.patch('mcp_servers.build_information._get_host_bits',
                return_value=build_information.Bitness.THIRTY_TWO)
    @mock.patch('mcp_servers.build_information._get_host_architecture',
                return_value=build_information.Architecture.ARM)
    def test_get_host_arch_arm(self, _mock_arch, _mock_bits):
        """Test get_host_arch for arm."""
        self.assertEqual(build_information.get_host_arch(),
                         build_information.ValidArch.ARM)

    @mock.patch('mcp_servers.build_information._get_host_bits')
    @mock.patch('mcp_servers.build_information._get_host_architecture',
                return_value=build_information.Architecture.UNKNOWN)
    def test_get_host_arch_unknown_arch(self, _mock_arch, _mock_bits):
        """Test get_host_arch for unknown architecture."""
        self.assertEqual(build_information.get_host_arch(),
                         build_information.ValidArch.UNKNOWN)

    @mock.patch('mcp_servers.build_information._get_host_bits',
                return_value=build_information.Bitness.UNKNOWN)
    @mock.patch('mcp_servers.build_information._get_host_architecture',
                return_value=build_information.Architecture.INTEL)
    def test_get_host_arch_intel_unknown_bits(self, _mock_arch, _mock_bits):
        """Test get_host_arch for intel with unknown bitness."""
        self.assertEqual(build_information.get_host_arch(),
                         build_information.ValidArch.UNKNOWN)

    @mock.patch('mcp_servers.build_information._get_host_bits',
                return_value=build_information.Bitness.UNKNOWN)
    @mock.patch('mcp_servers.build_information._get_host_architecture',
                return_value=build_information.Architecture.ARM)
    def test_get_host_arch_arm_unknown_bits(self, _mock_arch, _mock_bits):
        """Test get_host_arch for arm with unknown bitness."""
        self.assertEqual(build_information.get_host_arch(),
                         build_information.ValidArch.UNKNOWN)


class GetHostArchitectureTest(unittest.TestCase):

    @mock.patch('platform.processor', return_value='GenuineIntel')
    @mock.patch('platform.machine', return_value='x86_64')
    def test_get_host_architecture_intel_x64(self, _mock_machine,
                                             _mock_processor):
        """Test _get_host_architecture for intel x64."""
        self.assertEqual(build_information._get_host_architecture(),
                         build_information.Architecture.INTEL)

    @mock.patch('platform.processor', return_value='GenuineIntel')
    @mock.patch('platform.machine', return_value='x86')
    def test_get_host_architecture_intel_x86(self, _mock_machine,
                                             _mock_processor):
        """Test _get_host_architecture for intel x86."""
        self.assertEqual(build_information._get_host_architecture(),
                         build_information.Architecture.INTEL)

    @mock.patch('platform.processor', return_value='AuthenticAMD')
    @mock.patch('platform.machine', return_value='amd64')
    def test_get_host_architecture_intel_amd64(self, _mock_machine,
                                               _mock_processor):
        """Test _get_host_architecture for intel amd64."""
        self.assertEqual(build_information._get_host_architecture(),
                         build_information.Architecture.INTEL)

    @mock.patch('platform.processor', return_value='Apple M1')
    @mock.patch('platform.machine', return_value='arm64')
    def test_get_host_architecture_arm_native_arm64(self, _mock_machine,
                                                    _mock_processor):
        """Test _get_host_architecture for native arm64."""
        self.assertEqual(build_information._get_host_architecture(),
                         build_information.Architecture.ARM)

    @mock.patch('platform.processor', return_value='ARMv7')
    @mock.patch('platform.machine', return_value='arm')
    def test_get_host_architecture_arm_native_arm(self, _mock_machine,
                                                  _mock_processor):
        """Test _get_host_architecture for native arm."""
        self.assertEqual(build_information._get_host_architecture(),
                         build_information.Architecture.ARM)

    @mock.patch('platform.processor', return_value='... armv8 ...')
    @mock.patch('platform.machine', return_value='x86_64')
    def test_get_host_architecture_arm_emulated_x86(self, _mock_machine,
                                                    _mock_processor):
        """Test _get_host_architecture for emulated x86 on arm."""
        self.assertEqual(build_information._get_host_architecture(),
                         build_information.Architecture.ARM)

    @mock.patch('platform.processor', return_value='mips')
    @mock.patch('platform.machine', return_value='mips')
    def test_get_host_architecture_unknown(self, _mock_machine,
                                           _mock_processor):
        """Test _get_host_architecture for an unknown architecture."""
        self.assertEqual(build_information._get_host_architecture(),
                         build_information.Architecture.UNKNOWN)


class GetHostBitsTest(unittest.TestCase):

    @mock.patch('sys.maxsize', 2**32 + 1)
    def test_get_host_bits_64(self):
        """Test _get_host_bits for 64-bit."""
        self.assertEqual(build_information._get_host_bits(),
                         build_information.Bitness.SIXTY_FOUR)

    @mock.patch('sys.maxsize', 2**32 - 1)
    def test_get_host_bits_32(self):
        """Test _get_host_bits for 32-bit."""
        self.assertEqual(build_information._get_host_bits(),
                         build_information.Bitness.THIRTY_TWO)


class GetAllBuildDirectoriesTest(fake_filesystem_unittest.TestCase):

    def setUp(self):
        self.setUpPyfakefs()
        self.fs.create_dir(CHROMIUM_ROOT)

    def test_get_all_build_directories(self):
        """Test getting all build directories (standard and CrOS)."""
        self.fs.create_file(
            os.path.join(CHROMIUM_ROOT, 'out', 'Debug', 'args.gn'))
        self.fs.create_file(
            os.path.join(CHROMIUM_ROOT, 'out_cros', 'Debug', 'args.gn'))
        dirs = build_information.get_all_build_directories()
        self.assertEqual(
            set(dirs),
            {os.path.join('out', 'Debug'),
             os.path.join('out_cros', 'Debug')})


class GetValidBuildDirectoriesForConfigTest(fake_filesystem_unittest.TestCase):

    def setUp(self):
        self.setUpPyfakefs()
        self.fs.create_dir(CHROMIUM_ROOT)

        # Matching dir
        self.fs.create_file(os.path.join(CHROMIUM_ROOT, 'out', 'Linux_x64',
                                         'args.gn'),
                            contents='target_os = "linux"\ntarget_cpu = "x64"')
        # Mismatching os
        self.fs.create_file(os.path.join(CHROMIUM_ROOT, 'out', 'Win_x64',
                                         'args.gn'),
                            contents='target_os = "win"\ntarget_cpu = "x64"')
        # Mismatching cpu
        self.fs.create_file(
            os.path.join(CHROMIUM_ROOT, 'out', 'Linux_arm64', 'args.gn'),
            contents='target_os = "linux"\ntarget_cpu = "arm64"')
        # No args specified (should match)
        self.fs.create_file(os.path.join(CHROMIUM_ROOT, 'out', 'Default',
                                         'args.gn'),
                            contents='is_debug = true')
        # CrOS dir that matches
        self.fs.create_file(os.path.join(CHROMIUM_ROOT, 'out_cros',
                                         'linux_x64', 'args.gn'),
                            contents='target_os = "linux"\ntarget_cpu = "x64"')
        # Dir with no args.gn
        self.fs.create_dir(os.path.join(CHROMIUM_ROOT, 'out', 'NoArgs'))

    @mock.patch('mcp_servers.build_information.get_host_arch',
                return_value='x64')
    @mock.patch('mcp_servers.build_information.get_host_os',
                return_value='linux')
    def test_get_valid_build_information(self, _, __):
        """Test getting valid build directories for a specific config."""
        dirs = build_information.get_valid_build_directories_for_config(
            'linux', 'x64')
        self.assertEqual(
            set(dirs), {
                os.path.join('out', 'Linux_x64'),
                os.path.join('out', 'Default'),
                os.path.join('out_cros', 'linux_x64')
            })


class GetValidBuildDirectoriesForCurrentHostTest(unittest.TestCase):

    @mock.patch(
        'mcp_servers.build_information.get_valid_build_directories_for_config')
    @mock.patch('mcp_servers.build_information.get_host_arch')
    @mock.patch('mcp_servers.build_information.get_host_os')
    def test_valid_host_info(self, mock_get_os, mock_get_arch,
                             mock_get_valid_dirs):
        """Test getting dirs for current host with valid host info."""
        mock_get_os.return_value = build_information.ValidOs.LINUX
        mock_get_arch.return_value = build_information.ValidArch.X64
        mock_get_valid_dirs.return_value = ['out/Release']

        result = (
            build_information.get_valid_build_directories_for_current_host())

        mock_get_valid_dirs.assert_called_once_with('linux', 'x64')
        self.assertEqual(result, ['out/Release'])

    @mock.patch('mcp_servers.build_information.get_host_arch')
    @mock.patch('mcp_servers.build_information.get_host_os')
    def test_unknown_os(self, mock_get_os, mock_get_arch):
        """Test getting dirs for host with unknown OS returns empty list."""
        mock_get_os.return_value = build_information.ValidOs.UNKNOWN
        mock_get_arch.return_value = build_information.ValidArch.X64

        result = (
            build_information.get_valid_build_directories_for_current_host())
        self.assertEqual(result, [])

    @mock.patch('mcp_servers.build_information.get_host_arch')
    @mock.patch('mcp_servers.build_information.get_host_os')
    def test_unknown_arch(self, mock_get_os, mock_get_arch):
        """Test getting dirs for host with unknown arch returns empty list."""
        mock_get_os.return_value = build_information.ValidOs.LINUX
        mock_get_arch.return_value = build_information.ValidArch.UNKNOWN

        result = (
            build_information.get_valid_build_directories_for_current_host())
        self.assertEqual(result, [])


class GetStandardBuildDirectoriesTest(fake_filesystem_unittest.TestCase):

    def setUp(self):
        self.setUpPyfakefs()
        self.fs.create_dir(CHROMIUM_ROOT)

    def test_get_standard_build_directories(self):
        """Test getting standard build directories."""
        self.fs.create_file(
            os.path.join(CHROMIUM_ROOT, 'out', 'Debug', 'args.gn'))
        self.fs.create_file(
            os.path.join(CHROMIUM_ROOT, 'out_cros', 'Debug', 'args.gn'))
        dirs = build_information._get_standard_build_directories()
        self.assertEqual(set(dirs), {os.path.join('out', 'Debug')})


class GetCrosBuildDirectoriesTest(fake_filesystem_unittest.TestCase):

    def setUp(self):
        self.setUpPyfakefs()
        self.fs.create_dir(CHROMIUM_ROOT)

    def test_get_cros_build_directories(self):
        """Test getting CrOS build directories."""
        self.fs.create_file(
            os.path.join(CHROMIUM_ROOT, 'out', 'Debug', 'args.gn'))
        self.fs.create_file(
            os.path.join(CHROMIUM_ROOT, 'out_cros', 'Debug', 'args.gn'))
        dirs = build_information._get_cros_build_directories()
        self.assertEqual(set(dirs), {os.path.join('out_cros', 'Debug')})


class GetBuildDirectoriesUnderDirTest(fake_filesystem_unittest.TestCase):

    def setUp(self):
        self.setUpPyfakefs()
        self.fs.create_dir(CHROMIUM_ROOT)

    def test_get_build_directories_under_dir_no_directories(self):
        """Test behavior with no valid dirs."""
        self.fs.create_dir(os.path.join(CHROMIUM_ROOT, 'out'))
        self.assertEqual(
            build_information._get_build_directories_under_dir('out'), [])

    def test_get_build_directories_under_dir_multiple_valid(self):
        """Test behavior with multiple valid dirs."""
        self.fs.create_file(
            os.path.join(CHROMIUM_ROOT, 'out', 'Debug', 'args.gn'))
        self.fs.create_file(
            os.path.join(CHROMIUM_ROOT, 'out', 'Release', 'args.gn'))
        self.assertEqual(
            set(build_information._get_build_directories_under_dir('out')),
            {os.path.join('out', 'Debug'),
             os.path.join('out', 'Release')})

    def test_get_build_directories_under_dir_with_invalid(self):
        """Test behavior with a mix of valid/invalid dirs."""
        self.fs.create_dir(os.path.join(CHROMIUM_ROOT, 'out', 'Debug'))
        self.fs.create_file(
            os.path.join(CHROMIUM_ROOT, 'out', 'Release', 'args.gn'))
        self.assertEqual(
            build_information._get_build_directories_under_dir('out'),
            [os.path.join('out', 'Release')])

    def test_get_build_directories_under_dir_with_glob(self):
        """Test behavior with a glob pattern."""
        self.fs.create_file(
            os.path.join(CHROMIUM_ROOT, 'out_foo', 'Debug', 'args.gn'))
        self.fs.create_file(
            os.path.join(CHROMIUM_ROOT, 'out_bar', 'Release', 'args.gn'))
        self.fs.create_dir(os.path.join(CHROMIUM_ROOT, 'out_baz', 'Debug'))
        self.assertEqual(
            set(build_information._get_build_directories_under_dir('out_*')), {
                os.path.join('out_foo', 'Debug'),
                os.path.join('out_bar', 'Release')
            })


class DirectoryBuildsForConfigTest(fake_filesystem_unittest.TestCase):

    def setUp(self):
        self.setUpPyfakefs()
        self.fs.create_dir(CHROMIUM_ROOT)
        self.build_dir = os.path.join(CHROMIUM_ROOT, 'out', 'Release')
        self.fs.create_dir(self.build_dir)
        self.args_gn_path = os.path.join(self.build_dir, 'args.gn')

        self.os_patcher = mock.patch(
            'mcp_servers.build_information.get_host_os', return_value='linux')
        self.os_mock = self.os_patcher.start()
        self.addCleanup(self.os_patcher.stop)
        self.arch_patcher = mock.patch(
            'mcp_servers.build_information.get_host_arch', return_value='x64')
        self.arch_mock = self.arch_patcher.start()
        self.addCleanup(self.arch_patcher.stop)

    def test_no_args_gn_file(self):
        """Test directory without args.gn returns False."""
        self.assertFalse(
            build_information._directory_builds_for_config(
                self.build_dir, 'linux', 'x64'))

    def test_empty_args_gn_matching_defaults(self):
        """Test empty args.gn is true if defaults match requested config."""
        self.fs.create_file(self.args_gn_path, contents='')
        self.assertTrue(
            build_information._directory_builds_for_config(
                self.build_dir, 'linux', 'x64'))
        self.os_mock.assert_called_once()
        self.arch_mock.assert_called_once()

    def test_empty_args_gn_mismatching_os_default(self):
        """Test empty args.gn is false if default os mismatches."""
        self.os_mock.return_value = 'win'
        self.fs.create_file(self.args_gn_path, contents='')
        self.assertFalse(
            build_information._directory_builds_for_config(
                self.build_dir, 'linux', 'x64'))
        self.os_mock.assert_called_once()

    def test_empty_args_gn_mismatching_cpu_default(self):
        """Test empty args.gn is false if default cpu mismatches."""
        self.arch_mock.return_value = 'arm64'
        self.fs.create_file(self.args_gn_path, contents='')
        self.assertFalse(
            build_information._directory_builds_for_config(
                self.build_dir, 'linux', 'x64'))
        self.os_mock.assert_called_once()
        self.arch_mock.assert_called_once()

    def test_matching_os_and_cpu(self):
        """Test args.gn with matching os and cpu."""
        self.fs.create_file(self.args_gn_path,
                            contents='target_os = "linux"\ntarget_cpu = "x64"')
        self.assertTrue(
            build_information._directory_builds_for_config(
                self.build_dir, 'linux', 'x64'))

    def test_mismatching_os(self):
        """Test args.gn with mismatching os."""
        self.fs.create_file(self.args_gn_path,
                            contents='target_os = "win"\ntarget_cpu = "x64"')
        self.assertFalse(
            build_information._directory_builds_for_config(
                self.build_dir, 'linux', 'x64'))

    def test_mismatching_cpu(self):
        """Test args.gn with mismatching cpu."""
        self.fs.create_file(
            self.args_gn_path,
            contents='target_os = "linux"\ntarget_cpu = "arm64"')
        self.assertFalse(
            build_information._directory_builds_for_config(
                self.build_dir, 'linux', 'x64'))

    def test_only_os_matching_cpu_default(self):
        """Test only os in args.gn is true if default cpu matches."""
        self.fs.create_file(self.args_gn_path, contents='target_os = "linux"')
        self.assertTrue(
            build_information._directory_builds_for_config(
                self.build_dir, 'linux', 'x64'))
        self.arch_mock.assert_called_once()

    def test_only_os_mismatching_cpu_default(self):
        """Test only os in args.gn is false if default cpu mismatches."""
        self.arch_mock.return_value = 'arm64'
        self.fs.create_file(self.args_gn_path, contents='target_os = "linux"')
        self.assertFalse(
            build_information._directory_builds_for_config(
                self.build_dir, 'linux', 'x64'))
        self.arch_mock.assert_called_once()

    def test_only_cpu_matching_os_default(self):
        """Test only cpu in args.gn is true if default os matches."""
        self.fs.create_file(self.args_gn_path, contents='target_cpu = "x64"')
        self.assertTrue(
            build_information._directory_builds_for_config(
                self.build_dir, 'linux', 'x64'))
        self.os_mock.assert_called_once()

    def test_only_cpu_mismatching_os_default(self):
        """Test only cpu in args.gn is false if default os mismatches."""
        self.os_mock.return_value = 'win'
        self.fs.create_file(self.args_gn_path, contents='target_cpu = "x64"')
        self.assertFalse(
            build_information._directory_builds_for_config(
                self.build_dir, 'linux', 'x64'))
        self.os_mock.assert_called_once()

    def test_with_import_matching(self):
        """Test matching config from an imported .gni file."""
        gni_import_path = '//build/config.gni'
        gni_path = os.path.join(CHROMIUM_ROOT, 'build', 'config.gni')
        self.fs.create_file(gni_path, contents='target_os = "linux"')
        self.fs.create_file(
            self.args_gn_path,
            contents=f'import("{gni_import_path}")\ntarget_cpu = "x64"')
        self.assertTrue(
            build_information._directory_builds_for_config(
                self.build_dir, 'linux', 'x64'))

    def test_with_import_mismatching(self):
        """Test mismatching config from an imported .gni file."""
        gni_import_path = '//build/config.gni'
        gni_path = os.path.join(CHROMIUM_ROOT, 'build', 'config.gni')
        self.fs.create_file(gni_path, contents='target_os = "win"')
        self.fs.create_file(
            self.args_gn_path,
            contents=f'import("{gni_import_path}")\ntarget_cpu = "x64"')
        self.assertFalse(
            build_information._directory_builds_for_config(
                self.build_dir, 'linux', 'x64'))

    def test_with_nonexistent_import(self):
        """Test args.gn with a non-existent import returns False."""
        self.fs.create_file(self.args_gn_path,
                            contents='import("//build/nonexistent.gni")')
        self.assertFalse(
            build_information._directory_builds_for_config(
                self.build_dir, 'linux', 'x64'))

    def test_with_malformed_file(self):
        """Test that a malformed args.gn returns False."""
        self.fs.create_file(self.args_gn_path,
                            contents='target_os = "linux"\nasdf asdf')
        self.assertFalse(
            build_information._directory_builds_for_config(
                self.build_dir, 'linux', 'x64'))


if __name__ == '__main__':
    unittest.main()
