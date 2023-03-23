#!/usr/bin/env vpython3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing ffx_emulator.py."""

import argparse
import unittest

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
                        'product_bundle': None,
                        'enable_graphics': False,
                        'hardware_gpu': False,
                        'logs_dir': '.',
                        'with_network': False,
                        'everlasting': True
                    }))._node_name, 'fuchsia-everlasting-emulator')

    def test_use_random_node_name(self) -> None:
        """FfxEmulator should not use a fixed node name."""
        # Allowing the test case to access FfxEmulator._node_name directly.
        # pylint: disable=protected-access
        self.assertNotEqual(
            FfxEmulator(
                argparse.Namespace(
                    **{
                        'product_bundle': None,
                        'enable_graphics': False,
                        'hardware_gpu': False,
                        'logs_dir': '.',
                        'with_network': False,
                        'everlasting': False
                    }))._node_name, 'fuchsia-everlasting-emulator')


if __name__ == '__main__':
    unittest.main()
