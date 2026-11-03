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
        self.assertEqual(
            FfxEmulator(
                argparse.Namespace(
                    **{
                        'product': None,
                        'enable_graphics': False,
                        'logs_dir': '.',
                        'with_network': False,
                        'everlasting': True,
                        'device_spec': '',
                    }
                )
            )._node_name,
            'fuchsia-everlasting-emulator',
        )

    def test_use_random_node_name(self) -> None:
        """FfxEmulator should not use a fixed node name."""
        # Allowing the test case to access FfxEmulator._node_name directly.
        self.assertNotEqual(
            FfxEmulator(
                argparse.Namespace(
                    **{
                        'product': None,
                        'enable_graphics': False,
                        'logs_dir': '.',
                        'with_network': False,
                        'everlasting': False,
                        'device_spec': '',
                    }
                )
            )._node_name,
            'fuchsia-everlasting-emulator',
        )

    @mock.patch('ffx_emulator.run_ffx_command')
    def test_use_none_device_spec(self, mock_ffx) -> None:
        """FfxEmulator should use the default device spec if spec is None."""
        FfxEmulator(
            argparse.Namespace(
                **{
                    'product': None,
                    'enable_graphics': False,
                    'logs_dir': '.',
                    'with_network': False,
                    'everlasting': False,
                    'device_spec': None,
                }
            )
        ).__enter__()
        self.assertIn(
            ' '.join(['--net', 'user']),
            ' '.join(mock_ffx.call_args.kwargs['cmd']),
        )
        self.assertNotIn('--device', mock_ffx.call_args.kwargs['cmd'])

    @mock.patch('ffx_emulator.run_ffx_command')
    def test_use_empty_device_spec(self, mock_ffx) -> None:
        """FfxEmulator should use the default device spec if spec is empty."""
        FfxEmulator(
            argparse.Namespace(
                **{
                    'product': None,
                    'enable_graphics': False,
                    'logs_dir': '.',
                    'with_network': False,
                    'everlasting': False,
                    'device_spec': '',
                }
            )
        ).__enter__()
        self.assertIn(
            ' '.join(['--net', 'user']),
            ' '.join(mock_ffx.call_args.kwargs['cmd']),
        )
        self.assertNotIn('--device', mock_ffx.call_args.kwargs['cmd'])

    @mock.patch('ffx_emulator.run_ffx_command')
    def test_use_large_device_spec(self, mock_ffx) -> None:
        """FfxEmulator should use the large device spec if specified."""
        FfxEmulator(
            argparse.Namespace(
                **{
                    'product': None,
                    'enable_graphics': False,
                    'logs_dir': '.',
                    'with_network': False,
                    'everlasting': False,
                    'device_spec': 'x64-emu-large',
                }
            )
        ).__enter__()
        self.assertIn(
            ' '.join(['--device', 'x64-emu-large']),
            ' '.join(mock_ffx.call_args.kwargs['cmd']),
        )

    @mock.patch('ffx_emulator.run_ffx_command')
    def test_default_gpu_on_x64(self, mock_ffx) -> None:
        """FfxEmulator should use lavapipe on x64."""
        FfxEmulator(
            argparse.Namespace(
                **{
                    'product': 'terminal.x64',
                    'enable_graphics': False,
                    'logs_dir': '.',
                    'with_network': False,
                    'everlasting': False,
                    'device_spec': '',
                }
            )
        ).__enter__()
        self.assertIn(
            ' '.join(['--gpu', 'lavapipe']),
            ' '.join(mock_ffx.call_args.kwargs['cmd']),
        )

    @mock.patch('ffx_emulator.run_ffx_command')
    def test_no_gpu_on_arm64(self, mock_ffx) -> None:
        """FfxEmulator should not pass --gpu on arm64."""
        FfxEmulator(
            argparse.Namespace(
                **{
                    'product': 'terminal.qemu-arm64',
                    'enable_graphics': False,
                    'logs_dir': '.',
                    'with_network': False,
                    'everlasting': False,
                    'device_spec': '',
                }
            )
        ).__enter__()
        self.assertNotIn('--gpu', mock_ffx.call_args.kwargs['cmd'])


if __name__ == '__main__':
    unittest.main()
