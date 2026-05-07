# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import tempfile
import unittest

# Add current directory to python module lookup path
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)

import check_manifests


class CheckManifestsTest(unittest.TestCase):

    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()

    def tearDown(self):
        self.temp_dir.cleanup()

    def test_manifest_match(self):
        m1 = {
            "name": "Extension",
            "version": "1.0",
            "manifest_version": 3,
            "externally_connectable": {
                "matches": ["https://google.com/*"]
            }
        }

        m2 = {
            "name": "Extension",
            "version": "1.0",
            "manifest_version": 3,
            "externally_connectable": {
                "matches": ["https://internal.google.com/*"]
            }
        }

        import json
        m1_path = os.path.join(self.temp_dir.name, "manifest.json")
        m2_path = os.path.join(self.temp_dir.name, "manifest_internal.json")

        with open(m1_path, 'w') as f:
            f.write(json.dumps(m1))
        with open(m2_path, 'w') as f:
            f.write(json.dumps(m2))

        rc = check_manifests.main(manifest_path=m1_path,
                                  internal_manifest_path=m2_path)
        self.assertEqual(rc, 0)

    def test_manifest_mismatch(self):
        m1 = {"name": "Extension A", "version": "1.0", "manifest_version": 3}

        m2 = {"name": "Extension B", "version": "1.0", "manifest_version": 3}

        import json
        m1_path = os.path.join(self.temp_dir.name, "manifest.json")
        m2_path = os.path.join(self.temp_dir.name, "manifest_internal.json")

        with open(m1_path, 'w') as f:
            f.write(json.dumps(m1))
        with open(m2_path, 'w') as f:
            f.write(json.dumps(m2))

        rc = check_manifests.main(manifest_path=m1_path,
                                  internal_manifest_path=m2_path)
        self.assertEqual(rc, 1)


if __name__ == '__main__':
    unittest.main()
