#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing provision_starview.py."""

import os
import subprocess
import unittest
import unittest.mock as mock

import provision_starview


@unittest.skipIf(os.name == 'nt', 'Fuchsia tests not supported on Windows')
class ProvisionStarviewTest(unittest.TestCase):
    """Tests for provision_starview.py."""

    @mock.patch('provision_starview.subprocess.run')
    def test_run_adb_and_shell(self, mock_run: mock.MagicMock) -> None:
        mock_run.return_value = subprocess.CompletedProcess(
            args=[], returncode=0, stdout='ok\n', stderr=''
        )

        self.assertEqual(
            provision_starview.run_shell(
                '/usr/bin/adb', 'usb-123', ['echo', 'hello world']
            ),
            'ok\n',
        )
        mock_run.assert_called_once_with(
            ['/usr/bin/adb', '-s', 'usb-123', 'shell', "echo 'hello world'"],
            capture_output=True,
            text=True,
            check=True,
            timeout=None,
            env={**os.environ, 'ADB_LIBUSB': '0'},
        )

        mock_run.reset_mock()
        provision_starview.run_adb('/usr/bin/adb', ['devices'])
        mock_run.assert_called_once_with(
            ['/usr/bin/adb', 'devices'],
            capture_output=True,
            text=True,
            check=True,
            timeout=None,
            env={**os.environ, 'ADB_LIBUSB': '0'},
        )

    @mock.patch('provision_starview.time.sleep')
    @mock.patch(
        'provision_starview.time.monotonic', side_effect=[0.0, 1.0, 2.0, 3.0]
    )
    @mock.patch('provision_starview.run_adb')
    def test_wait_adb_shell_retries_tcp_and_timeout_expired(
        self,
        mock_run_adb: mock.MagicMock,
        _mock_monotonic: mock.MagicMock,
        mock_sleep: mock.MagicMock,
    ) -> None:
        mock_run_adb.side_effect = [
            subprocess.TimeoutExpired(cmd='adb', timeout=10),  # connect hung
            subprocess.CompletedProcess([], 0, '', ''),  # connect
            subprocess.CalledProcessError(1, 'adb'),  # shell failed
            subprocess.CompletedProcess([], 0, '', ''),  # connect
            subprocess.CompletedProcess([], 0, 'ready\n', ''),  # shell
        ]

        provision_starview.wait_adb_shell(
            '/usr/bin/adb', '127.0.0.1:6525', ['echo', 'ready'], 'ready'
        )

        self.assertEqual(mock_sleep.call_count, 2)
        mock_run_adb.assert_any_call(
            '/usr/bin/adb',
            ['connect', '127.0.0.1:6525'],
            check=False,
            timeout=10,
        )

    @mock.patch('provision_starview.time.monotonic', side_effect=[0.0, 500.0])
    @mock.patch(
        'provision_starview.run_adb',
        side_effect=subprocess.CalledProcessError(1, 'adb'),
    )
    def test_wait_adb_shell_timeout(
        self,
        _mock_run_adb: mock.MagicMock,
        _mock_monotonic: mock.MagicMock,
    ) -> None:
        with self.assertRaises(TimeoutError):
            provision_starview.wait_adb_shell(
                '/usr/bin/adb', 'usb-123', ['echo', 'ready'], 'ready'
            )

    @mock.patch('provision_starview.wait_adb_shell')
    @mock.patch('provision_starview.run_shell', return_value='uid=0(root)\n')
    @mock.patch('provision_starview.run_adb')
    def test_ensure_root_already_root(
        self,
        mock_run_adb: mock.MagicMock,
        _mock_shell: mock.MagicMock,
        mock_wait: mock.MagicMock,
    ) -> None:
        provision_starview.ensure_root(
            '/usr/bin/adb', '127.0.0.1:6525', timeout=60
        )

        mock_run_adb.assert_not_called()
        mock_wait.assert_called_once_with(
            '/usr/bin/adb',
            '127.0.0.1:6525',
            ['echo', 'ready'],
            'ready',
            timeout=60,
        )

    @mock.patch('provision_starview.wait_adb_shell')
    @mock.patch('provision_starview.run_shell', return_value='uid=2000(shell)')
    @mock.patch(
        'provision_starview.run_adb',
        side_effect=subprocess.TimeoutExpired(cmd='adb', timeout=15),
    )
    def test_ensure_root_ignores_timeout_expired(
        self,
        _mock_run_adb: mock.MagicMock,
        _mock_shell: mock.MagicMock,
        mock_wait: mock.MagicMock,
    ) -> None:
        provision_starview.ensure_root(
            '/usr/bin/adb', '127.0.0.1:6525', timeout=60
        )

        mock_wait.assert_has_calls(
            [
                mock.call(
                    '/usr/bin/adb',
                    '127.0.0.1:6525',
                    ['echo', 'ready'],
                    'ready',
                    timeout=60,
                ),
                mock.call(
                    '/usr/bin/adb',
                    '127.0.0.1:6525',
                    ['id'],
                    r'uid=0\(root\).*',
                    timeout=60,
                ),
            ]
        )

    @mock.patch('provision_starview.wait_adb_shell')
    @mock.patch('provision_starview.run_shell', return_value='uid=2000(shell)')
    @mock.patch('provision_starview.run_adb')
    def test_ensure_root_restarts_as_root(
        self,
        mock_run_adb: mock.MagicMock,
        _mock_shell: mock.MagicMock,
        mock_wait: mock.MagicMock,
    ) -> None:
        provision_starview.ensure_root('/usr/bin/adb', 'usb-123', timeout=60)

        mock_run_adb.assert_called_once_with(
            '/usr/bin/adb',
            ['root'],
            serial='usb-123',
            check=False,
            timeout=15,
        )
        mock_wait.assert_has_calls(
            [
                mock.call(
                    '/usr/bin/adb',
                    'usb-123',
                    ['echo', 'ready'],
                    'ready',
                    timeout=60,
                ),
                mock.call(
                    '/usr/bin/adb',
                    'usb-123',
                    ['id'],
                    r'uid=0\(root\).*',
                    timeout=60,
                ),
            ]
        )

    @mock.patch('provision_starview.run_shell')
    def test_ensure_mmap_capable_test_data_root_fresh(
        self, mock_shell: mock.MagicMock
    ) -> None:
        mock_shell.side_effect = [
            '',  # wait for /storage/emulated
            '',  # symlink + Download dirs
            '/dev/block/dm-0 on /data type ext4 (rw)\n',  # mount
            '',  # mkdir + chown + chmod + chcon + mount --bind
        ]

        provision_starview._ensure_mmap_capable_test_data_root(
            '/usr/bin/adb', 'usb-123'
        )

        self.assertIn(
            'mount --bind /mnt/pass_through/0/emulated/0/chromium_tests_root '
            '/sdcard/chromium_tests_root',
            mock_shell.call_args_list[-1].args[2],
        )
        self.assertEqual(mock_shell.call_count, 4)

    @mock.patch('provision_starview.run_shell')
    def test_ensure_mmap_capable_test_data_root_already_mounted(
        self, mock_shell: mock.MagicMock
    ) -> None:
        mock_shell.side_effect = [
            '',  # wait for /storage/emulated
            '',  # symlink + Download dirs
            'remotevol on /sdcard/chromium_tests_root type remotevol (rw)\n',
            '',  # chown + chmod
        ]

        provision_starview._ensure_mmap_capable_test_data_root(
            '/usr/bin/adb', 'usb-123'
        )

        source_root = '/mnt/pass_through/0/emulated/0/chromium_tests_root'
        mock_shell.assert_called_with(
            '/usr/bin/adb',
            'usb-123',
            f'chown -R shell:shell {source_root} && chmod -R 777 {source_root}',
        )
        self.assertEqual(mock_shell.call_count, 4)

    @mock.patch('provision_starview._ensure_mmap_capable_test_data_root')
    @mock.patch('provision_starview.run_shell')
    @mock.patch('provision_starview.wait_adb_shell')
    @mock.patch('provision_starview.ensure_root')
    def test_provision_device(
        self,
        mock_root: mock.MagicMock,
        mock_wait: mock.MagicMock,
        mock_shell: mock.MagicMock,
        mock_ensure_mmap: mock.MagicMock,
    ) -> None:
        provision_starview.provision_device(
            '127.0.0.1:6525',
            adb_path='/usr/bin/adb',
            boot_timeout=120,
        )

        mock_root.assert_called_once_with(
            '/usr/bin/adb', '127.0.0.1:6525', timeout=120
        )
        mock_wait.assert_has_calls(
            [
                mock.call(
                    '/usr/bin/adb',
                    '127.0.0.1:6525',
                    ['getprop', 'sys.boot_completed'],
                    '1',
                    timeout=120,
                ),
                mock.call(
                    '/usr/bin/adb',
                    '127.0.0.1:6525',
                    ['pm', 'path', 'android'],
                    r'package:.+',
                    timeout=120,
                ),
            ]
        )
        mock_shell.assert_called_once_with(
            '/usr/bin/adb',
            '127.0.0.1:6525',
            'input keyevent KEYCODE_WAKEUP; wm dismiss-keyguard; '
            'svc power stayon true',
            check=False,
        )
        mock_ensure_mmap.assert_called_once_with(
            '/usr/bin/adb', '127.0.0.1:6525'
        )


if __name__ == '__main__':
    unittest.main()
