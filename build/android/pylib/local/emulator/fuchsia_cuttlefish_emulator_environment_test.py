#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import io
import os
import sys
import unittest
from unittest import mock

sys.path.append(
    os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
)
import devil_chromium  # pylint: disable=unused-import
from devil.android import device_utils
from pylib.local.emulator import (
    fuchsia_cuttlefish_emulator_environment as fc_env,
)


class FuchsiaCuttlefishEmulatorEnvironmentTest(unittest.TestCase):
    def test_is_supported(self):
        with (
            mock.patch.dict(os.environ, {'ISOLATED_OUTDIR': '/tmp/out'}),
            mock.patch('os.path.exists', return_value=True),
        ):
            self.assertTrue(fc_env.IsSupported())

        with (
            mock.patch.dict(os.environ, {}, clear=True),
            mock.patch('os.path.exists', return_value=True),
        ):
            self.assertFalse(fc_env.IsSupported())

    def test_cuttlefish_instance_success(self):
        fake_stdout = io.StringIO(
            'Booting QEMU...\n'
            'Guest ADB is ready.\n'
            'Press Ctrl+C to terminate the emulator.\n'
            'Still running...\n'
        )
        mock_proc = mock.Mock()
        mock_proc.stdout = fake_stdout
        mock_proc.poll.return_value = None

        orig_wait = device_utils.DeviceUtils.WaitUntilFullyBooted

        with (
            mock.patch('os.path.exists', return_value=True),
            mock.patch(
                'subprocess.Popen', return_value=mock_proc
            ) as mock_popen,
        ):
            instance = fc_env.CuttlefishInstance(
                adb_port=6520, adb_path='/mock/adb'
            )
            serial = instance.Start()
            self.assertEqual(serial, '127.0.0.1:6520')
            # WaitUntilFullyBooted should be monkeypatched after Start.
            self.assertNotEqual(
                device_utils.DeviceUtils.WaitUntilFullyBooted, orig_wait
            )
            mock_popen.assert_called_once_with(
                [
                    sys.executable,
                    fc_env.CUTTLEFISH_SCRIPT,
                    '--adb-port',
                    '6520',
                    '--headless',
                    '--adb-path',
                    '/mock/adb',
                ],
                stdout=mock.ANY,
                stderr=mock.ANY,
                text=True,
            )

            instance.Stop()
            # After Stop, proc should be terminated and WaitUntilFullyBooted
            # restored.
            mock_proc.terminate.assert_called_once()
            self.assertEqual(
                device_utils.DeviceUtils.WaitUntilFullyBooted, orig_wait
            )

    def test_cuttlefish_instance_premature_exit(self):
        fake_stdout = io.StringIO('Error: failed to find image\n')
        mock_proc = mock.Mock()
        mock_proc.stdout = fake_stdout
        mock_proc.poll.return_value = 1

        orig_wait = device_utils.DeviceUtils.WaitUntilFullyBooted

        with (
            mock.patch('os.path.exists', return_value=True),
            mock.patch('subprocess.Popen', return_value=mock_proc),
        ):
            instance = fc_env.CuttlefishInstance()
            with self.assertRaisesRegex(
                RuntimeError, 'exited prematurely with code 1'
            ):
                instance.Start()

        self.assertEqual(
            device_utils.DeviceUtils.WaitUntilFullyBooted, orig_wait
        )

    def test_environment_setup_teardown(self):
        mock_instance = mock.Mock()
        mock_instance.Start.return_value = '127.0.0.1:6525'
        with (
            mock.patch.object(
                fc_env, 'CuttlefishInstance', return_value=mock_instance
            ),
            mock.patch.object(
                fc_env.local_device_environment.LocalDeviceEnvironment,
                '__init__',
                return_value=None,
            ),
            mock.patch.object(
                fc_env.local_device_environment.LocalDeviceEnvironment,
                'SetUp',
            ),
            mock.patch.object(
                fc_env.local_device_environment.LocalDeviceEnvironment,
                'TearDown',
            ),
        ):
            env = fc_env.FuchsiaCuttlefishEmulatorEnvironment(
                mock.Mock(), mock.Mock(), mock.Mock()
            )
            env.SetUp()
            mock_instance.Start.assert_called_once()
            # pylint: disable=protected-access
            self.assertEqual(env._device_serials, ['127.0.0.1:6525'])
            # pylint: enable=protected-access

            env.TearDown()
            mock_instance.Stop.assert_called_once()


if __name__ == '__main__':
    unittest.main()
