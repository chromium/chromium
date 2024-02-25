#!/usr/bin/env vpython3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing ffx_emulator.py."""

import argparse
import unittest
import unittest.mock as mock

from ffx_emulator import FfxEmulator


class FfxEmulatorTest(unittest.TestCase):
    """Unittests for ffx_emulator.py"""
    def test_use_fixed_node_name(self) -> None:
        """FfxEmulator should use a fixed node name."""
        # Allowing the test case to access FfxEmulator._node_name directly.
        # pylint: disable=protected-access
        self.assertEqual(
            FfxEmulator(
                argparse.Namespace(
                    **{
                        'product': None,
                        'enable_graphics': False,
                        'hardware_gpu': False,
                        'logs_dir': '.',
                        'with_network': False,
                        'everlasting': True,
                        'device_spec': ''
                    }))._node_name, 'fuchsia-everlasting-emulator')

    def test_use_random_node_name(self) -> None:
        """FfxEmulator should not use a fixed node name."""
        # Allowing the test case to access FfxEmulator._node_name directly.
        # pylint: disable=protected-access
        self.assertNotEqual(
            FfxEmulator(
                argparse.Namespace(
                    **{
                        'product': None,
                        'enable_graphics': False,
                        'hardware_gpu': False,
                        'logs_dir': '.',
                        'with_network': False,
                        'everlasting': False,
                        'device_spec': ''
                    }))._node_name, 'fuchsia-everlasting-emulator')

    @mock.patch('ffx_emulator.run_ffx_command')
    def test_use_none_device_spec(self, mock_ffx) -> None:
        """FfxEmulator should use the default device spec if spec is None."""
        FfxEmulator(
            argparse.Namespace(
                **{
                    'product': None,
                    'enable_graphics': False,
                    'hardware_gpu': False,
                    'logs_dir': '.',
                    'with_network': False,
                    'everlasting': False,
                    'device_spec': None
                })).__enter__()
        self.assertIn(' '.join(['--net', 'user']),
                      ' '.join(mock_ffx.call_args.kwargs['cmd']))
        self.assertNotIn('--device', mock_ffx.call_args.kwargs['cmd'])

    @mock.patch('ffx_emulator.run_ffx_command')
    def test_use_empty_device_spec(self, mock_ffx) -> None:
        """FfxEmulator should use the default device spec if spec is empty."""
        FfxEmulator(
            argparse.Namespace(
                **{
                    'product': None,
                    'enable_graphics': False,
                    'hardware_gpu': False,
                    'logs_dir': '.',
                    'with_network': False,
                    'everlasting': False,
                    'device_spec': ''
                })).__enter__()
        self.assertIn(' '.join(['--net', 'user']),
                      ' '.join(mock_ffx.call_args.kwargs['cmd']))
        self.assertNotIn('--device', mock_ffx.call_args.kwargs['cmd'])

    @mock.patch('ffx_emulator.run_ffx_command')
    def test_use_large_device_spec(self, mock_ffx) -> None:
        """FfxEmulator should use large device spec."""
        FfxEmulator(
            argparse.Namespace(
                **{
                    'product': None,
                    'enable_graphics': False,
                    'hardware_gpu': False,
                    'logs_dir': '.',
                    'with_network': False,
                    'everlasting': False,
                    'device_spec': 'large'
                })).__enter__()
        self.assertIn(' '.join(['--device', 'large']),
                      ' '.join(mock_ffx.call_args.kwargs['cmd']))


if __name__ == '__main__':
    unittest.main()
