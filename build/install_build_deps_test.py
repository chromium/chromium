#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import importlib.util
import io
import os
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

_DIR = os.path.dirname(os.path.abspath(__file__))


def _load_module(name, rel_path):
    path = os.path.join(_DIR, rel_path)
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


install_build_deps = _load_module('install_build_deps', 'install-build-deps.py')
install_chromeos_fonts = _load_module(
    'install_chromeos_fonts',
    os.path.join('linux', 'install-chromeos-fonts.py'),
)


@unittest.skipUnless(sys.platform.startswith('linux'), 'Linux-only scripts')
class InstallBuildDepsTest(unittest.TestCase):
    def setUp(self):
        super().setUp()
        install_build_deps.build_apt_package_list.cache_clear()

    @mock.patch('subprocess.run')
    @mock.patch('subprocess.Popen')
    def test_build_apt_package_list(self, mock_popen, mock_run):
        dumpavail_text = (
            'Package: malformed-without-arch\n'
            'Version: 1.0\n'
            '\n'
            'Architecture: i386\n'
            '\n'
            'Package: libc6\n'
            'Version: 2.39-0ubuntu8\n'
            'Architecture: amd64\n'
            '\n'
            'Package: libc6\n'
            'Version: 2.39-0ubuntu8\n'
            'Architecture: i386\n'
            '\n'
            'Package: tzdata\n'
            'Architecture: all\n'
        )
        proc = mock.MagicMock()
        proc.__enter__.return_value = proc
        proc.stdout = io.StringIO(dumpavail_text)
        proc.wait.return_value = 0
        mock_popen.return_value = proc
        mock_run.return_value = subprocess.CompletedProcess(
            args=['apt-cache', 'show', 'nonexistent'], returncode=100
        )

        pkgs = install_build_deps.build_apt_package_list()
        mock_popen.assert_called_once_with(
            ['apt-cache', 'dumpavail'],
            stdout=subprocess.PIPE,
            text=True,
            encoding='utf-8',
            errors='replace',
        )
        self.assertEqual(pkgs, {'libc6', 'libc6:i386', 'tzdata'})
        self.assertTrue(install_build_deps.package_is_installable('libc6:i386'))
        self.assertTrue(
            install_build_deps.package_is_installable(
                'libc6:i386=2.39-0ubuntu8'
            )
        )
        mock_run.assert_not_called()
        self.assertFalse(
            install_build_deps.package_is_installable('nonexistent')
        )
        mock_run.assert_called_once_with(
            ['apt-cache', 'show', 'nonexistent'], capture_output=True
        )

        install_build_deps.build_apt_package_list.cache_clear()
        proc.stdout = io.StringIO('')
        proc.wait.return_value = 100
        proc.returncode = 100
        proc.args = ['apt-cache', 'dumpavail']
        with self.assertRaises(subprocess.CalledProcessError):
            install_build_deps.build_apt_package_list()

    @mock.patch('subprocess.check_call')
    def test_install_locales_batches_sed(self, mock_check_call):
        with tempfile.TemporaryDirectory() as tmpdir:
            locale_gen = os.path.join(tmpdir, 'locale.gen')
            with open(locale_gen, 'w', encoding='utf-8') as f:
                f.write('# en_US.UTF-8 UTF-8\n# da_DK.UTF-8 UTF-8\n')

            def fake_check_call(cmd):
                if cmd[:3] == ['sudo', 'sed', '-i']:
                    with open(locale_gen, 'w', encoding='utf-8') as f:
                        f.write('en_US.UTF-8 UTF-8\nda_DK.UTF-8 UTF-8\n')

            mock_check_call.side_effect = fake_check_call
            with mock.patch.object(
                install_build_deps, 'LOCALE_GEN', locale_gen
            ):
                install_build_deps.install_locales()

            self.assertEqual(mock_check_call.call_count, 2)
            sed_call = mock_check_call.call_args_list[0][0][0]
            self.assertEqual(sed_call[:3], ['sudo', 'sed', '-i'])
            self.assertEqual(sed_call[3], '-e')
            self.assertIn('s/^# en_US.UTF-8/en_US.UTF-8/', sed_call)
            self.assertEqual(sed_call[-1], locale_gen)
            self.assertEqual(
                mock_check_call.call_args_list[1][0][0],
                ['sudo', 'locale-gen'],
            )

    @mock.patch('subprocess.check_call')
    @mock.patch('os.getuid', return_value=0)
    def test_install_chromeos_fonts(self, mock_getuid, mock_check_call):
        del mock_getuid  # Unused.
        with tempfile.TemporaryDirectory() as tmpdir:

            def fake_check_call(cmd):
                if cmd[0] == 'curl':
                    with open(cmd[-1], 'wb') as f:
                        f.write(b'fake-tarball')

            mock_check_call.side_effect = fake_check_call
            with mock.patch.object(install_chromeos_fonts, 'FONTS_DIR', tmpdir):
                rc = install_chromeos_fonts.main([])
                self.assertEqual(rc, 0)
                self.assertEqual(
                    mock_check_call.call_count,
                    len(install_chromeos_fonts.URLS) * 2,
                )
                first_curl = mock_check_call.call_args_list[0][0][0]
                self.assertEqual(first_curl[:2], ['curl', '-fL'])

                mock_check_call.reset_mock()
                rc_cached = install_chromeos_fonts.main([])
                self.assertEqual(rc_cached, 0)
                mock_check_call.assert_not_called()

    @mock.patch('subprocess.check_call')
    @mock.patch('os.getuid', return_value=0)
    def test_install_chromeos_fonts_curl_failure(
        self, mock_getuid, mock_check_call
    ):
        del mock_getuid  # Unused.
        with tempfile.TemporaryDirectory() as tmpdir:
            mock_check_call.side_effect = subprocess.CalledProcessError(
                22, ['curl', '-fL']
            )
            with mock.patch.object(install_chromeos_fonts, 'FONTS_DIR', tmpdir):
                with self.assertRaises(subprocess.CalledProcessError):
                    install_chromeos_fonts.main([])
                stamp = os.path.join(tmpdir, 'chromeos', '.stamp02')
                self.assertFalse(os.path.exists(stamp))


if __name__ == '__main__':
    unittest.main()
