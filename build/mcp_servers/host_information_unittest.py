#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for host_information.py."""

import os
import sys
import unittest
from unittest import mock

# pylint: disable=wrong-import-position
sys.path.insert(0,
                os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
from mcp_servers import host_information
# pylint: enable=wrong-import-position


class GetHostOsTest(unittest.TestCase):

    @mock.patch('sys.platform', 'linux')
    def test_get_host_os_linux(self):
        """Test get_host_os on linux."""
        self.assertEqual(host_information.get_host_os(),
                         host_information.ValidOs.LINUX)

    @mock.patch('sys.platform', 'cygwin')
    def test_get_host_os_cygwin_is_linux(self):
        """Test get_host_os on cygwin returns linux."""
        self.assertEqual(host_information.get_host_os(),
                         host_information.ValidOs.LINUX)

    @mock.patch('sys.platform', 'win32')
    def test_get_host_os_win(self):
        """Test get_host_os on windows."""
        self.assertEqual(host_information.get_host_os(),
                         host_information.ValidOs.WIN)

    @mock.patch('sys.platform', 'darwin')
    def test_get_host_os_mac(self):
        """Test get_host_os on mac."""
        self.assertEqual(host_information.get_host_os(),
                         host_information.ValidOs.MAC)

    @mock.patch('sys.platform', 'freebsd')
    def test_get_host_os_unknown(self):
        """Test get_host_os on an unknown os."""
        self.assertEqual(host_information.get_host_os(),
                         host_information.ValidOs.UNKNOWN)


class GetHostArchTest(unittest.TestCase):

    @mock.patch('platform.processor', return_value='GenuineIntel')
    @mock.patch('platform.machine', return_value='x86_64')
    def test_get_host_architecture_intel_x64(self, _mock_machine,
                                             _mock_processor):
        """Test _get_host_architecture for intel x64."""
        self.assertEqual(host_information._get_host_architecture(),
                         host_information.Architecture.INTEL)

    @mock.patch('platform.processor', return_value='GenuineIntel')
    @mock.patch('platform.machine', return_value='x86')
    def test_get_host_architecture_intel_x86(self, _mock_machine,
                                             _mock_processor):
        """Test _get_host_architecture for intel x86."""
        self.assertEqual(host_information._get_host_architecture(),
                         host_information.Architecture.INTEL)

    @mock.patch('platform.processor', return_value='AuthenticAMD')
    @mock.patch('platform.machine', return_value='amd64')
    def test_get_host_architecture_intel_amd64(self, _mock_machine,
                                               _mock_processor):
        """Test _get_host_architecture for intel amd64."""
        self.assertEqual(host_information._get_host_architecture(),
                         host_information.Architecture.INTEL)

    @mock.patch('platform.processor', return_value='Apple M1')
    @mock.patch('platform.machine', return_value='arm64')
    def test_get_host_architecture_arm_native_arm64(self, _mock_machine,
                                                    _mock_processor):
        """Test _get_host_architecture for native arm64."""
        self.assertEqual(host_information._get_host_architecture(),
                         host_information.Architecture.ARM)

    @mock.patch('platform.processor', return_value='ARMv7')
    @mock.patch('platform.machine', return_value='arm')
    def test_get_host_architecture_arm_native_arm(self, _mock_machine,
                                                  _mock_processor):
        """Test _get_host_architecture for native arm."""
        self.assertEqual(host_information._get_host_architecture(),
                         host_information.Architecture.ARM)

    @mock.patch('platform.processor', return_value='... armv8 ...')
    @mock.patch('platform.machine', return_value='x86_64')
    def test_get_host_architecture_arm_emulated_x86(self, _mock_machine,
                                                    _mock_processor):
        """Test _get_host_architecture for emulated x86 on arm."""
        self.assertEqual(host_information._get_host_architecture(),
                         host_information.Architecture.ARM)

    @mock.patch('platform.processor', return_value='mips')
    @mock.patch('platform.machine', return_value='mips')
    def test_get_host_architecture_unknown(self, _mock_machine,
                                           _mock_processor):
        """Test _get_host_architecture for an unknown architecture."""
        self.assertEqual(host_information._get_host_architecture(),
                         host_information.Architecture.UNKNOWN)


class GetHostArchitectureTest(unittest.TestCase):

    @mock.patch('mcp_servers.host_information._get_host_bits',
                return_value=host_information.Bitness.SIXTY_FOUR)
    @mock.patch('mcp_servers.host_information._get_host_architecture',
                return_value=host_information.Architecture.INTEL)
    def test_get_host_arch_x64(self, _mock_arch, _mock_bits):
        """Test get_host_arch for x64."""
        self.assertEqual(host_information.get_host_arch(),
                         host_information.ValidArch.X64)

    @mock.patch('mcp_servers.host_information._get_host_bits',
                return_value=host_information.Bitness.THIRTY_TWO)
    @mock.patch('mcp_servers.host_information._get_host_architecture',
                return_value=host_information.Architecture.INTEL)
    def test_get_host_arch_x86(self, _mock_arch, _mock_bits):
        """Test get_host_arch for x86."""
        self.assertEqual(host_information.get_host_arch(),
                         host_information.ValidArch.X86)

    @mock.patch('mcp_servers.host_information._get_host_bits',
                return_value=host_information.Bitness.SIXTY_FOUR)
    @mock.patch('mcp_servers.host_information._get_host_architecture',
                return_value=host_information.Architecture.ARM)
    def test_get_host_arch_arm64(self, _mock_arch, _mock_bits):
        """Test get_host_arch for arm64."""
        self.assertEqual(host_information.get_host_arch(),
                         host_information.ValidArch.ARM64)

    @mock.patch('mcp_servers.host_information._get_host_bits',
                return_value=host_information.Bitness.THIRTY_TWO)
    @mock.patch('mcp_servers.host_information._get_host_architecture',
                return_value=host_information.Architecture.ARM)
    def test_get_host_arch_arm(self, _mock_arch, _mock_bits):
        """Test get_host_arch for arm."""
        self.assertEqual(host_information.get_host_arch(),
                         host_information.ValidArch.ARM)

    @mock.patch('mcp_servers.host_information._get_host_bits')
    @mock.patch('mcp_servers.host_information._get_host_architecture',
                return_value=host_information.Architecture.UNKNOWN)
    def test_get_host_arch_unknown_arch(self, _mock_arch, _mock_bits):
        """Test get_host_arch for unknown architecture."""
        self.assertEqual(host_information.get_host_arch(),
                         host_information.ValidArch.UNKNOWN)

    @mock.patch('mcp_servers.host_information._get_host_bits',
                return_value=host_information.Bitness.UNKNOWN)
    @mock.patch('mcp_servers.host_information._get_host_architecture',
                return_value=host_information.Architecture.INTEL)
    def test_get_host_arch_intel_unknown_bits(self, _mock_arch, _mock_bits):
        """Test get_host_arch for intel with unknown bitness."""
        self.assertEqual(host_information.get_host_arch(),
                         host_information.ValidArch.UNKNOWN)

    @mock.patch('mcp_servers.host_information._get_host_bits',
                return_value=host_information.Bitness.UNKNOWN)
    @mock.patch('mcp_servers.host_information._get_host_architecture',
                return_value=host_information.Architecture.ARM)
    def test_get_host_arch_arm_unknown_bits(self, _mock_arch, _mock_bits):
        """Test get_host_arch for arm with unknown bitness."""
        self.assertEqual(host_information.get_host_arch(),
                         host_information.ValidArch.UNKNOWN)


class GetHostBitsTest(unittest.TestCase):

    @mock.patch('sys.maxsize', 2**32 + 1)
    def test_get_host_bits_64(self):
        """Test _get_host_bits for 64-bit."""
        self.assertEqual(host_information._get_host_bits(),
                         host_information.Bitness.SIXTY_FOUR)

    @mock.patch('sys.maxsize', 2**32 - 1)
    def test_get_host_bits_32(self):
        """Test _get_host_bits for 32-bit."""
        self.assertEqual(host_information._get_host_bits(),
                         host_information.Bitness.THIRTY_TWO)


if __name__ == '__main__':
    unittest.main()
