#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import plistlib
import tempfile
import unittest

import mac_toolchain


class MacToolchainTest(unittest.TestCase):
    def _write_plist(self, path, data):
        with open(path, 'wb') as f:
            plistlib.dump(data, f)

    def test_parse_version(self):
        # Tests that `_ParseVersion` compares multi-digit version components
        # numerically rather than lexicographically.
        self.assertLess(
            mac_toolchain._ParseVersion('9.0'),
            mac_toolchain._ParseVersion('27.0'),
        )
        self.assertLess(
            mac_toolchain._ParseVersion('26.2'),
            mac_toolchain._ParseVersion('26.10'),
        )
        self.assertLess(
            mac_toolchain._ParseVersion('26.0.1'),
            mac_toolchain._ParseVersion('27.0'),
        )

    def test_is_license_accepted_missing_file(self):
        # Tests that `_IsLicenseAccepted` returns False when the plist does not
        # exist.
        with tempfile.TemporaryDirectory() as tmp_dir:
            plist_path = os.path.join(tmp_dir, 'com.apple.dt.Xcode.plist')
            self.assertFalse(
                mac_toolchain._IsLicenseAccepted(plist_path, '27.0')
            )

    def test_is_license_accepted_gm_only(self):
        # Tests that `_IsLicenseAccepted` checks `GM_VERSION_KEY` when
        # `PTR_VERSION_KEY` is absent.
        with tempfile.TemporaryDirectory() as tmp_dir:
            plist_path = os.path.join(tmp_dir, 'com.apple.dt.Xcode.plist')
            self._write_plist(
                plist_path,
                {
                    mac_toolchain.GM_VERSION_KEY: '27.0',
                    mac_toolchain.GM_LICENSE_KEY: 'EA2002',
                },
            )
            self.assertTrue(
                mac_toolchain._IsLicenseAccepted(plist_path, '27.0')
            )
            self.assertFalse(
                mac_toolchain._IsLicenseAccepted(plist_path, '27.1')
            )

    def test_is_license_accepted_stale_ptr_version(self):
        # Tests that `_IsLicenseAccepted` returns False when `GM_VERSION_KEY`
        # is up to date but `PTR_VERSION_KEY` holds an older Xcode version.
        with tempfile.TemporaryDirectory() as tmp_dir:
            plist_path = os.path.join(tmp_dir, 'com.apple.dt.Xcode.plist')
            self._write_plist(
                plist_path,
                {
                    mac_toolchain.GM_VERSION_KEY: '27.0',
                    mac_toolchain.GM_LICENSE_KEY: 'EA2002',
                    mac_toolchain.PTR_VERSION_KEY: '26.0.1',
                    mac_toolchain.PTR_LICENSE_KEY: 'EA1910',
                },
            )
            self.assertFalse(
                mac_toolchain._IsLicenseAccepted(plist_path, '27.0')
            )

            self._write_plist(
                plist_path,
                {
                    mac_toolchain.GM_VERSION_KEY: '27.0',
                    mac_toolchain.GM_LICENSE_KEY: 'EA2002',
                    mac_toolchain.PTR_VERSION_KEY: '27.0',
                    mac_toolchain.PTR_LICENSE_KEY: 'EA2002',
                },
            )
            self.assertTrue(
                mac_toolchain._IsLicenseAccepted(plist_path, '27.0')
            )


if __name__ == '__main__':
    unittest.main()
