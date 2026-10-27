#!/usr/bin/env vpython3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing serial_boot_device.py."""

import importlib
import itertools
import json
import os
import shutil
import tempfile
import unittest
from unittest import mock

from subprocess import CompletedProcess

import serial_boot_device

from boot_device import BootMode


# pylint: disable=too-many-public-methods, missing-function-docstring
# pylint: disable=protected-access
@mock.patch('shutil.which', return_value='/bin')
class SerialBootDeviceTest(unittest.TestCase):
    """Unittests for serial_boot_device.py."""

    def setUp(self) -> None:
        os.environ['FUCHSIA_NODENAME'] = 'fuchsia-node-id'
        os.environ['FUCHSIA_FASTBOOT_SERNUM'] = 'fuchsia-serial-num'
        temp_dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, temp_dir)
        attempt_file = os.path.join(temp_dir, 'pdu-attempted')
        patch_attempt = mock.patch(
            'serial_boot_device.PDU_ATTEMPTED_FILE', attempt_file
        )
        patch_attempt.start()
        self.addCleanup(patch_attempt.stop)
        patch_pdu_host = mock.patch(
            'serial_boot_device.PDU_HOST', 'fuchsia-pdu-host'
        )
        patch_pdu_host.start()
        self.addCleanup(patch_pdu_host.stop)
        patch_pdu_outlet = mock.patch(
            'serial_boot_device.PDU_OUTLET', 'fuchsia-pdu-outlet'
        )
        patch_pdu_outlet.start()
        self.addCleanup(patch_pdu_outlet.stop)

    def test_does_not_boot_without_binaries(self, *_) -> None:
        with mock.patch('shutil.which', return_value=None):
            self.assertNotEqual(serial_boot_device.main('reboot'), 0)

    @mock.patch('serial_boot_device.is_in_fuchsia', side_effect=[True])
    @mock.patch(
        'subprocess.run',
        return_value=CompletedProcess(args=['/bin'], returncode=0),
    )
    @mock.patch('builtins.print')
    def test_check_health_in_fuchsia(self, mock_print, *_) -> None:
        self.assertEqual(serial_boot_device.main('health-check'), 0)
        result = json.loads(mock_print.call_args.args[0])
        self.assertEqual(result[0]['nodename'], 'fuchsia-node-id')
        self.assertEqual(result[0]['state'], 'healthy')

    @mock.patch('serial_boot_device.is_in_fastboot', side_effect=[True])
    @mock.patch('serial_boot_device.is_in_fuchsia', side_effect=[False])
    @mock.patch(
        'subprocess.run',
        return_value=CompletedProcess(args=['/bin'], returncode=0),
    )
    @mock.patch('builtins.print')
    def test_check_health_in_fastboot(self, mock_print, *_) -> None:
        self.assertEqual(serial_boot_device.main('health-check'), 0)
        result = json.loads(mock_print.call_args.args[0])
        self.assertEqual(result[0]['nodename'], 'fuchsia-node-id')
        self.assertEqual(result[0]['state'], 'healthy')

    @mock.patch('serial_boot_device.is_in_fastboot', side_effect=[False])
    @mock.patch('serial_boot_device.is_in_fuchsia', side_effect=[False])
    @mock.patch(
        'subprocess.run',
        return_value=CompletedProcess(args=['/bin'], returncode=0),
    )
    def test_check_health_undetectable(self, *_) -> None:
        self.assertNotEqual(serial_boot_device.main('health-check'), 0)

    @mock.patch('serial_boot_device.pdu_reboot', return_value=True)
    @mock.patch('serial_boot_device.is_in_fastboot', side_effect=[False])
    @mock.patch('serial_boot_device.is_in_fuchsia', side_effect=[False, True])
    @mock.patch(
        'subprocess.run',
        return_value=CompletedProcess(args=['/bin'], returncode=0),
    )
    @mock.patch('builtins.print')
    def test_check_health_undetectable_pdu_recovers_fuchsia(
        self, mock_print, _mock_run, _mock_fuchsia, _mock_fb, mock_pdu, *_
    ) -> None:
        self.assertEqual(serial_boot_device.main('health-check'), 0)
        mock_pdu.assert_called_once()
        result = json.loads(mock_print.call_args.args[0])
        self.assertEqual(result[0]['nodename'], 'fuchsia-node-id')
        self.assertEqual(result[0]['state'], 'healthy')

    @mock.patch('serial_boot_device.pdu_reboot', return_value=True)
    @mock.patch('serial_boot_device.is_in_fastboot', side_effect=[False, True])
    @mock.patch('serial_boot_device.is_in_fuchsia', side_effect=[False, False])
    @mock.patch(
        'subprocess.run',
        return_value=CompletedProcess(args=['/bin'], returncode=0),
    )
    @mock.patch('builtins.print')
    def test_check_health_undetectable_pdu_recovers_fastboot(
        self, mock_print, _mock_run, _mock_fuchsia, _mock_fb, mock_pdu, *_
    ) -> None:
        self.assertEqual(serial_boot_device.main('health-check'), 0)
        mock_pdu.assert_called_once()
        result = json.loads(mock_print.call_args.args[0])
        self.assertEqual(result[0]['nodename'], 'fuchsia-node-id')
        self.assertEqual(result[0]['state'], 'healthy')

    @mock.patch('serial_boot_device.pdu_reboot', return_value=False)
    @mock.patch('serial_boot_device.is_in_fastboot', side_effect=[False])
    @mock.patch('serial_boot_device.is_in_fuchsia', side_effect=[False])
    @mock.patch(
        'subprocess.run',
        return_value=CompletedProcess(args=['/bin'], returncode=0),
    )
    def test_check_health_undetectable_pdu_fails(
        self, _mock_run, _mock_fuchsia, _mock_fb, mock_pdu, *_
    ) -> None:
        self.assertNotEqual(serial_boot_device.main('health-check'), 0)
        mock_pdu.assert_called_once()

    @mock.patch('serial_boot_device.pdu_reboot', return_value=True)
    @mock.patch('serial_boot_device.is_in_fastboot', side_effect=[False, False])
    @mock.patch('serial_boot_device.is_in_fuchsia', side_effect=[False, False])
    @mock.patch(
        'subprocess.run',
        return_value=CompletedProcess(args=['/bin'], returncode=0),
    )
    def test_check_health_undetectable_pdu_still_undetectable(
        self, _mock_run, _mock_fuchsia, _mock_fb, mock_pdu, *_
    ) -> None:
        self.assertNotEqual(serial_boot_device.main('health-check'), 0)
        mock_pdu.assert_called_once()

    @mock.patch('serial_boot_device.is_in_fuchsia', side_effect=[False, True])
    @mock.patch('serial_boot_device.is_in_fastboot', side_effect=[False])
    @mock.patch(
        'subprocess.run',
        return_value=CompletedProcess(args=['/bin'], returncode=0),
    )
    def test_boot_undetectable(self, mock_run, *_) -> None:
        self.assertEqual(serial_boot_device.main('reboot'), 0)
        mock_run.assert_called_once_with(
            ['serialio', 'fuchsia-node-id', 'send', 'dm', 'reboot']
        )

    @mock.patch('serial_boot_device.is_in_fuchsia', side_effect=[False, True])
    @mock.patch('serial_boot_device.is_in_fastboot', side_effect=[False])
    @mock.patch(
        'subprocess.run',
        return_value=CompletedProcess(args=['/bin'], returncode=0),
    )
    def test_boot_undetectable_not_must_boot(self, mock_run, *_) -> None:
        self.assertTrue(
            serial_boot_device.boot_device(
                'fuchsia-node-id',
                'fuchsia-serial-num',
                BootMode.REGULAR,
                must_boot=False,
            )
        )
        mock_run.assert_called_once_with(
            ['serialio', 'fuchsia-node-id', 'send', 'dm', 'reboot']
        )

    @mock.patch('serial_boot_device.is_in_fuchsia', side_effect=[False])
    @mock.patch('serial_boot_device.is_in_fastboot', side_effect=[False, True])
    @mock.patch(
        'subprocess.run',
        return_value=CompletedProcess(args=['/bin'], returncode=0),
    )
    def test_boot_undetectable_to_fastboot(self, mock_run, *_) -> None:
        self.assertEqual(serial_boot_device.main('reboot-fastboot'), 0)
        mock_run.assert_called_once_with(
            ['serialio', 'fuchsia-node-id', 'send', 'dm', 'reboot-bootloader']
        )

    @mock.patch('serial_boot_device.is_in_fastboot', side_effect=[False])
    @mock.patch('serial_boot_device.is_in_fuchsia', side_effect=[True, True])
    @mock.patch(
        'subprocess.run',
        return_value=CompletedProcess(args=['/bin'], returncode=0),
    )
    def test_boot_from_fuchsia_to_fuchsia(self, mock_run, *_) -> None:
        self.assertEqual(serial_boot_device.main('reboot'), 0)
        mock_run.assert_called_once_with(
            ['serialio', 'fuchsia-node-id', 'send', 'dm', 'reboot']
        )

    @mock.patch('serial_boot_device.is_in_fastboot', side_effect=[False])
    @mock.patch('serial_boot_device.is_in_fuchsia', side_effect=[True, True])
    @mock.patch(
        'subprocess.run',
        return_value=CompletedProcess(args=['/bin'], returncode=0),
    )
    def test_boot_from_fuchsia_to_fuchsia_not_must_boot(
        self, mock_run, *_
    ) -> None:
        self.assertTrue(
            serial_boot_device.boot_device(
                'fuchsia-node-id',
                'fuchsia-serial-num',
                BootMode.REGULAR,
                must_boot=False,
            )
        )
        mock_run.assert_not_called()

    @mock.patch('serial_boot_device.is_in_fuchsia', side_effect=[True])
    @mock.patch('serial_boot_device.is_in_fastboot', side_effect=[True])
    @mock.patch(
        'subprocess.run',
        return_value=CompletedProcess(args=['/bin'], returncode=0),
    )
    def test_boot_from_fastboot_to_fuchsia(self, mock_run, *_) -> None:
        self.assertEqual(serial_boot_device.main('reboot'), 0)
        mock_run.assert_called_once_with(
            ['fastboot', 'reboot', '-s', 'fuchsia-serial-num'],
            capture_output=True,
            timeout=30,
        )

    @mock.patch('serial_boot_device.is_in_fuchsia', side_effect=[True])
    @mock.patch('serial_boot_device.is_in_fastboot', side_effect=[False, True])
    @mock.patch(
        'subprocess.run',
        return_value=CompletedProcess(args=['/bin'], returncode=0),
    )
    def test_boot_from_fuchsia_to_fastboot(self, mock_run, *_) -> None:
        self.assertEqual(serial_boot_device.main('reboot-fastboot'), 0)
        mock_run.assert_called_once_with(
            ['serialio', 'fuchsia-node-id', 'send', 'dm', 'reboot-bootloader']
        )

    @mock.patch('serial_boot_device.is_in_fuchsia', side_effect=[False])
    @mock.patch('serial_boot_device.is_in_fastboot', side_effect=[True, True])
    @mock.patch(
        'subprocess.run',
        return_value=CompletedProcess(args=['/bin'], returncode=0),
    )
    def test_boot_from_fastboot_to_fastboot(self, mock_run, *_) -> None:
        self.assertEqual(serial_boot_device.main('reboot-fastboot'), 0)
        mock_run.assert_not_called()

    def test_can_pdu_reboot(self, *_) -> None:
        try:
            with mock.patch.dict(
                os.environ,
                {'PDU_HOST': 'pdu-host.br', 'PDU_OUTLET': 'AA11'},
            ), mock.patch('os.path.exists', return_value=True):
                importlib.reload(serial_boot_device)
                self.assertTrue(serial_boot_device.CAN_PDU_REBOOT)
                self.assertEqual(
                    serial_boot_device.PDU_ATTEMPTED_FILE,
                    '/tmp/pdu-attempted-pdu-host.br-AA11',
                )
            with mock.patch.dict(os.environ, clear=True), mock.patch(
                'os.path.exists', return_value=False
            ):
                importlib.reload(serial_boot_device)
                self.assertFalse(serial_boot_device.CAN_PDU_REBOOT)
                self.assertEqual(
                    serial_boot_device.PDU_ATTEMPTED_FILE,
                    '/tmp/pdu-attempted-None-None',
                )
        finally:
            importlib.reload(serial_boot_device)

    def test_assert_tmp_writable(self, *_) -> None:
        try:
            with mock.patch('os.access', return_value=False):
                with self.assertRaises(AssertionError):
                    importlib.reload(serial_boot_device)
        finally:
            importlib.reload(serial_boot_device)

    @mock.patch('serial_boot_device.CAN_PDU_REBOOT', True)
    @mock.patch(
        'subprocess.run',
        return_value=CompletedProcess(args=['/bin'], returncode=0),
    )
    def test_pdu_reboot_success(self, mock_run, *_) -> None:
        self.assertTrue(serial_boot_device.pdu_reboot())
        mock_run.assert_called_once_with(
            ['/usr/local/bin/reboot_pdu_outlet.py'],
            capture_output=True,
        )
        self.assertTrue(os.path.exists(serial_boot_device.PDU_ATTEMPTED_FILE))

    @mock.patch('serial_boot_device.CAN_PDU_REBOOT', True)
    @mock.patch(
        'subprocess.run',
        return_value=CompletedProcess(args=['/bin'], returncode=0),
    )
    def test_pdu_reboot_skips_if_already_attempted(self, mock_run, *_) -> None:
        self.assertTrue(serial_boot_device.pdu_reboot())
        mock_run.assert_called_once()
        mock_run.reset_mock()
        self.assertFalse(serial_boot_device.pdu_reboot())
        mock_run.assert_not_called()

    @mock.patch('serial_boot_device.CAN_PDU_REBOOT', True)
    @mock.patch('os.open', side_effect=OSError('Permission denied'))
    def test_pdu_reboot_lock_creation_oserror_fails(self, *_) -> None:
        self.assertFalse(serial_boot_device.pdu_reboot())

    def test_clear_pdu_attempt(self, *_) -> None:
        with open(serial_boot_device.PDU_ATTEMPTED_FILE, 'w'):
            pass
        self.assertTrue(os.path.exists(serial_boot_device.PDU_ATTEMPTED_FILE))
        serial_boot_device._clear_pdu_attempt()
        self.assertFalse(os.path.exists(serial_boot_device.PDU_ATTEMPTED_FILE))
        # Should not raise if file already removed.
        serial_boot_device._clear_pdu_attempt()

    @mock.patch('serial_boot_device.CAN_PDU_REBOOT', True)
    @mock.patch(
        'subprocess.run',
        return_value=CompletedProcess(
            args=['/bin'], returncode=1, stdout=b'', stderr=b'error'
        ),
    )
    def test_pdu_reboot_failure(self, *_) -> None:
        self.assertFalse(serial_boot_device.pdu_reboot())
        self.assertFalse(os.path.exists(serial_boot_device.PDU_ATTEMPTED_FILE))

    @mock.patch('time.time', side_effect=itertools.count(0, 150))
    @mock.patch('serial_boot_device.pdu_reboot', return_value=True)
    @mock.patch('serial_boot_device.is_in_fastboot', return_value=False)
    @mock.patch(
        'serial_boot_device.is_in_fuchsia', side_effect=[False, False, True]
    )
    @mock.patch(
        'subprocess.run',
        return_value=CompletedProcess(args=['/bin'], returncode=0),
    )
    def test_boot_undetectable_with_pdu(
        self, mock_run, _mock_fuchsia, _mock_fb, mock_pdu, *_
    ) -> None:
        self.assertEqual(serial_boot_device.main('reboot'), 0)
        mock_run.assert_called_once_with(
            ['serialio', 'fuchsia-node-id', 'send', 'dm', 'reboot']
        )
        mock_pdu.assert_called_once()

    @mock.patch('time.time', side_effect=itertools.count(0, 150))
    @mock.patch('serial_boot_device.pdu_reboot', return_value=True)
    @mock.patch('serial_boot_device.is_in_fastboot', return_value=False)
    @mock.patch(
        'serial_boot_device.is_in_fuchsia', side_effect=[True, False, True]
    )
    @mock.patch(
        'subprocess.run',
        return_value=CompletedProcess(args=['/bin'], returncode=0),
    )
    def test_boot_soft_reboot_fails_escalates_to_pdu(
        self, mock_run, _mock_fuchsia, _mock_fb, mock_pdu, *_
    ) -> None:
        self.assertEqual(serial_boot_device.main('reboot'), 0)
        mock_run.assert_called_once_with(
            ['serialio', 'fuchsia-node-id', 'send', 'dm', 'reboot']
        )
        mock_pdu.assert_called_once()

    @mock.patch('time.time', side_effect=itertools.count(0, 150))
    @mock.patch('serial_boot_device.pdu_reboot', return_value=True)
    @mock.patch('serial_boot_device.is_in_fastboot', return_value=True)
    @mock.patch('serial_boot_device._run_fastboot', return_value=False)
    @mock.patch('serial_boot_device.is_in_fuchsia', side_effect=[False, True])
    def test_boot_from_fastboot_to_fuchsia_fails_then_pdu(
        self, _mock_fuchsia, _mock_fastboot, _mock_is_fb, mock_pdu, *_
    ) -> None:
        self.assertEqual(serial_boot_device.main('reboot'), 0)
        mock_pdu.assert_called_once()

    @mock.patch('time.sleep')
    @mock.patch('serial_boot_device.pdu_reboot', return_value=True)
    @mock.patch('serial_boot_device.is_in_fuchsia', return_value=True)
    @mock.patch(
        'serial_boot_device.is_in_fastboot', side_effect=[False, False, True]
    )
    @mock.patch(
        'subprocess.run',
        return_value=CompletedProcess(args=['/bin'], returncode=0),
    )
    def test_boot_to_fastboot_fails_then_pdu(
        self, mock_run, _mock_fb, _mock_fuchsia, mock_pdu, mock_sleep, *_
    ) -> None:
        self.assertEqual(serial_boot_device.main('reboot-fastboot'), 0)
        self.assertEqual(mock_run.call_count, 101)
        mock_run.assert_has_calls(
            [
                mock.call(
                    [
                        'serialio',
                        'fuchsia-node-id',
                        'send',
                        'dm',
                        'reboot-bootloader',
                    ]
                )
            ]
            + [mock.call(['serialio', 'fuchsia-node-id', 'send', 'f' * 10])]
            * 100
        )
        mock_pdu.assert_called_once()
        self.assertEqual(mock_sleep.call_count, 100)
        mock_sleep.assert_called_with(0.1)

    @mock.patch('time.time', side_effect=itertools.count(0, 150))
    @mock.patch('serial_boot_device.pdu_reboot', return_value=False)
    @mock.patch('serial_boot_device.is_in_fastboot', return_value=False)
    @mock.patch('serial_boot_device.is_in_fuchsia', side_effect=[True, False])
    @mock.patch(
        'subprocess.run',
        return_value=CompletedProcess(args=['/bin'], returncode=0),
    )
    def test_boot_soft_reboot_fails_and_pdu_fails(
        self, mock_run, _mock_fuchsia, _mock_fb, mock_pdu, *_
    ) -> None:
        self.assertEqual(serial_boot_device.main('reboot'), 1)
        mock_run.assert_called_once_with(
            ['serialio', 'fuchsia-node-id', 'send', 'dm', 'reboot']
        )
        mock_pdu.assert_called_once()

    @mock.patch('time.time', side_effect=itertools.count(0, 150))
    @mock.patch('serial_boot_device.CAN_PDU_REBOOT', False)
    @mock.patch('serial_boot_device.is_in_fastboot', return_value=False)
    @mock.patch('serial_boot_device.is_in_fuchsia', side_effect=[True, False])
    @mock.patch(
        'subprocess.run',
        return_value=CompletedProcess(args=['/bin'], returncode=0),
    )
    def test_boot_soft_reboot_fails_no_pdu(
        self, mock_run, _mock_fuchsia, _mock_fb, *_
    ) -> None:
        self.assertEqual(serial_boot_device.main('reboot'), 1)
        mock_run.assert_called_once_with(
            ['serialio', 'fuchsia-node-id', 'send', 'dm', 'reboot']
        )

    @mock.patch('serial_boot_device.CAN_PDU_REBOOT', False)
    def test_pdu_reboot_not_configured(self, *_) -> None:
        self.assertFalse(serial_boot_device.pdu_reboot())
        self.assertFalse(os.path.exists(serial_boot_device.PDU_ATTEMPTED_FILE))

    @mock.patch('serial_boot_device._serialio_send_and_wait', return_value=True)
    def test_is_in_fuchsia_clears_pdu_attempt(self, *_) -> None:
        with open(serial_boot_device.PDU_ATTEMPTED_FILE, 'w'):
            pass
        self.assertTrue(os.path.exists(serial_boot_device.PDU_ATTEMPTED_FILE))
        self.assertTrue(serial_boot_device.is_in_fuchsia('fuchsia-node-id'))
        self.assertFalse(os.path.exists(serial_boot_device.PDU_ATTEMPTED_FILE))

    @mock.patch(
        'serial_boot_device._serialio_send_and_wait', return_value=False
    )
    def test_is_in_fuchsia_false_does_not_clear_pdu_attempt(self, *_) -> None:
        with open(serial_boot_device.PDU_ATTEMPTED_FILE, 'w'):
            pass
        self.assertTrue(os.path.exists(serial_boot_device.PDU_ATTEMPTED_FILE))
        self.assertFalse(serial_boot_device.is_in_fuchsia('fuchsia-node-id'))
        self.assertTrue(os.path.exists(serial_boot_device.PDU_ATTEMPTED_FILE))

    @mock.patch('serial_boot_device._run_fastboot', return_value=True)
    def test_is_in_fastboot_clears_pdu_attempt(self, *_) -> None:
        with open(serial_boot_device.PDU_ATTEMPTED_FILE, 'w'):
            pass
        self.assertTrue(os.path.exists(serial_boot_device.PDU_ATTEMPTED_FILE))
        self.assertTrue(serial_boot_device.is_in_fastboot('fuchsia-serial-num'))
        self.assertFalse(os.path.exists(serial_boot_device.PDU_ATTEMPTED_FILE))

    @mock.patch('time.time', side_effect=itertools.count(0, 30))
    @mock.patch('serial_boot_device._run_fastboot', return_value=False)
    def test_is_in_fastboot_false_does_not_clear_pdu_attempt(self, *_) -> None:
        with open(serial_boot_device.PDU_ATTEMPTED_FILE, 'w'):
            pass
        self.assertTrue(os.path.exists(serial_boot_device.PDU_ATTEMPTED_FILE))
        self.assertFalse(
            serial_boot_device.is_in_fastboot('fuchsia-serial-num')
        )
        self.assertTrue(os.path.exists(serial_boot_device.PDU_ATTEMPTED_FILE))

    @mock.patch('serial_boot_device.is_in_fuchsia', return_value=True)
    def test_main_is_in_fuchsia_success(self, *_) -> None:
        self.assertEqual(serial_boot_device.main('is-in-fuchsia'), 0)

    @mock.patch('serial_boot_device.is_in_fuchsia', return_value=False)
    def test_main_is_in_fuchsia_failure(self, *_) -> None:
        self.assertEqual(serial_boot_device.main('is-in-fuchsia'), 1)

    @mock.patch('serial_boot_device.is_in_fastboot', return_value=True)
    def test_main_is_in_fastboot_success(self, *_) -> None:
        self.assertEqual(serial_boot_device.main('is-in-fastboot'), 0)

    @mock.patch('serial_boot_device.is_in_fastboot', return_value=False)
    def test_main_is_in_fastboot_failure(self, *_) -> None:
        self.assertEqual(serial_boot_device.main('is-in-fastboot'), 1)


if __name__ == '__main__':
    unittest.main()
