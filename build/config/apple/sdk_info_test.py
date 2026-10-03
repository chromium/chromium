#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for sdk_info.py, codesign.py, and strip_arm64e.py."""

import io
import os
import plistlib
import struct
import sys
import tempfile
import unittest
from unittest import mock

import codesign
import sdk_info

_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
_IOS_CONFIG_DIR = os.path.abspath(os.path.join(_THIS_DIR, '..', 'ios'))
if _IOS_CONFIG_DIR not in sys.path:
    sys.path.insert(0, _IOS_CONFIG_DIR)

import strip_arm64e  # noqa: E402


class SdkInfoAndToolsTest(unittest.TestCase):
    def test_sdk_info_non_darwin(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            ver_plist = os.path.join(tmpdir, 'Contents', 'version.plist')
            os.makedirs(os.path.dirname(ver_plist))
            with open(ver_plist, 'wb') as f:
                plistlib.dump(
                    {
                        'CFBundleShortVersionString': '27.0',
                        'ProductBuildVersion': '17A324',
                    },
                    f,
                )

            sim_sdk = os.path.join(
                tmpdir,
                'Contents',
                'Developer',
                'Platforms',
                'iPhoneSimulator.platform',
                'Developer',
                'SDKs',
                'iPhoneSimulator.sdk',
            )
            sys_ver = os.path.join(sim_sdk, 'System', 'Library', 'CoreServices')
            os.makedirs(sys_ver)
            with open(os.path.join(sys_ver, 'SystemVersion.plist'), 'wb') as f:
                plistlib.dump(
                    {
                        'ProductVersion': '27.0',
                        'ProductBuildVersion': '25A354',
                    },
                    f,
                )

            settings = {}
            sdk_info.FillXcodeVersion(settings, tmpdir)
            self.assertEqual(settings['xcode_version'], '2700')
            self.assertEqual(settings['xcode_version_verbatim'], '27.0')
            self.assertEqual(settings['xcode_build'], '17A324')

            with mock.patch.object(sys, 'platform', 'linux'):
                sdk_info.FillMachineOSBuild(settings, tmpdir)
                self.assertEqual(settings['machine_os_build'], '25A354')

                sdk_info.FillSDKPathAndVersion(
                    settings, 'iphonesimulator', '2700', tmpdir
                )
                self.assertEqual(settings['sdk_version'], '27.0')
                self.assertEqual(settings['sdk_build'], '25A354')
                self.assertEqual(settings['sdk_path'], sim_sdk)

                if os.name != 'nt':
                    links_dir = os.path.join(
                        tmpdir, 'out', 'Debug', 'sdk_links'
                    )
                    sdk_info.CreateXcodeSymlinkAt(
                        sim_sdk, links_dir, os.path.join(tmpdir, 'out', 'Debug')
                    )
                    link_path = os.path.join(links_dir, 'iPhoneSimulator.sdk')
                    self.assertTrue(os.path.islink(link_path))
                    self.assertFalse(os.path.isabs(os.readlink(link_path)))
                    self.assertEqual(
                        os.path.realpath(link_path), os.path.realpath(sim_sdk)
                    )

    def test_codesign_non_darwin(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            profile_path = os.path.join(tmpdir, 'test.mobileprovision')
            plist_bytes = plistlib.dumps(
                {
                    'ApplicationIdentifierPrefix': ['PREFIX'],
                    'Entitlements': {
                        'application-identifier': 'PREFIX.com.example.app'
                    },
                    'ExpirationDate': plistlib.datetime.datetime(
                        2030, 1, 1, tzinfo=plistlib.datetime.timezone.utc
                    ),
                    'UUID': '1234-5678',
                }
            )
            with open(profile_path, 'wb') as f:
                f.write(b'\x30\x82\x01\x00' + plist_bytes + b'\x00\x00')

            with mock.patch.object(sys, 'platform', 'linux'):
                prof = codesign.ProvisioningProfile(profile_path)
                self.assertEqual(prof.application_identifier_prefix, 'PREFIX')

                stderr_buf = io.StringIO()
                with mock.patch('sys.stderr', stderr_buf):
                    codesign.CodeSignBundle('/tmp/app', '-', [])
                self.assertEqual(stderr_buf.getvalue(), '')

                with mock.patch('sys.stderr', stderr_buf):
                    codesign.CodeSignBundle('/tmp/app', 'iPhone Developer', [])
                self.assertIn(
                    'skipping cryptographic codesign', stderr_buf.getvalue()
                )

    def test_strip_arm64e_get_archs_and_main(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            macho_arm64 = os.path.join(tmpdir, 'thin_arm64')
            with open(macho_arm64, 'wb') as f:
                f.write(struct.pack('<III', 0xFEEDFACF, 0x0100000C, 0))

            macho_arm64e = os.path.join(tmpdir, 'thin_arm64e')
            with open(macho_arm64e, 'wb') as f:
                f.write(struct.pack('<III', 0xFEEDFACF, 0x0100000C, 0x80000002))

            slice0_bytes = struct.pack('<III', 0xFEEDFACF, 0x0100000C, 0) + (
                b'\xaa' * 52
            )
            slice1_bytes = struct.pack(
                '<III', 0xFEEDFACF, 0x0100000C, 0x80000002
            ) + (b'\xbb' * 52)
            fat_bin = os.path.join(tmpdir, 'fat_bin')
            with open(fat_bin, 'wb') as f:
                f.write(struct.pack('>II', 0xCAFEBABE, 2))
                f.write(
                    struct.pack(
                        '>IIIII', 0x0100000C, 0, 64, len(slice0_bytes), 6
                    )
                )
                f.write(
                    struct.pack(
                        '>IIIII',
                        0x0100000C,
                        0x80000002,
                        128,
                        len(slice1_bytes),
                        6,
                    )
                )
                f.write(b'\x00' * (64 - 48))
                f.write(slice0_bytes)
                f.write(slice1_bytes)

            with mock.patch.object(sys, 'platform', 'linux'):
                self.assertEqual(strip_arm64e.get_archs(macho_arm64), ['arm64'])
                self.assertEqual(
                    strip_arm64e.get_archs(macho_arm64e), ['arm64e']
                )
                self.assertEqual(
                    strip_arm64e.get_archs(fat_bin), ['arm64', 'arm64e']
                )

                out_bin = os.path.join(tmpdir, 'out_bin')
                strip_arm64e.main(
                    [
                        '--input',
                        fat_bin,
                        '--output',
                        out_bin,
                        '--xcode-version',
                        '2700',
                    ]
                )
                self.assertEqual(strip_arm64e.get_archs(out_bin), ['arm64'])
                with open(out_bin, 'rb') as f:
                    out_data = f.read()
                _, nfat = struct.unpack('>II', out_data[:8])
                self.assertEqual(nfat, 1)
                _, _, offset, size, _ = struct.unpack('>IIIII', out_data[8:28])
                self.assertEqual(out_data[offset : offset + size], slice0_bytes)


if __name__ == '__main__':
    unittest.main()
