# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for find_candidates.py."""

import os
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import find_candidates as fc

_LICENSE = "".join(f"// License line {i}\n" for i in range(15))
_VALID_FQN = "org.chromium.base.Log"
_BANNED_FQN = "org.chromium.fake.NotImported"


def _java_source(*fqns):
    body = "".join(f"    {f} o{i};\n" for i, f in enumerate(fqns))
    return _LICENSE + "class Foo {\n  void bar() {\n" + body + "  }\n}\n"


def _fake_is_valid_import(fqn, search_root):
    return fqn == _VALID_FQN


class CheckFileTest(unittest.TestCase):
    def setUp(self):
        self._root_tmp = tempfile.TemporaryDirectory()
        self._cwd_tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._root_tmp.cleanup)
        self.addCleanup(self._cwd_tmp.cleanup)
        self.root = os.path.realpath(self._root_tmp.name)
        self.other_cwd = os.path.realpath(self._cwd_tmp.name)

        old_cwd = os.getcwd()
        self.addCleanup(os.chdir, old_cwd)
        os.chdir(self.other_cwd)

        patcher = mock.patch.object(
            fc, "is_valid_import", side_effect=_fake_is_valid_import
        )
        self.mock_is_valid = patcher.start()
        self.addCleanup(patcher.stop)

    def _write(self, rel_path, content):
        abs_path = os.path.join(self.root, *rel_path.split("/"))
        os.makedirs(os.path.dirname(abs_path), exist_ok=True)
        with open(abs_path, "w", encoding="utf-8") as f:
            f.write(content)
        return abs_path

    def test_relative_path_resolved_against_search_root(self):
        rel = "chrome/android/Foo.java"
        self._write(rel, _java_source(_VALID_FQN, _BANNED_FQN))
        # The file does not exist relative to the CWD.
        self.assertFalse(os.path.exists(rel))

        result = fc.check_file(rel, self.root)

        self.assertIsNotNone(result)
        self.assertEqual(result["valid"], [_VALID_FQN])
        self.assertEqual(result["banned"], [_BANNED_FQN])
        self.mock_is_valid.assert_any_call(_VALID_FQN, self.root)
        self.mock_is_valid.assert_any_call(_BANNED_FQN, self.root)

    def test_absolute_path_resolved_directly(self):
        abs_path = self._write(
            "chrome/android/Foo.java", _java_source(_VALID_FQN, _BANNED_FQN)
        )
        self.assertTrue(os.path.isabs(abs_path))

        # A bogus search_root must not affect absolute path resolution.
        result = fc.check_file(abs_path, os.path.join(self.root, "nope"))

        self.assertIsNotNone(result)
        self.assertEqual(result["valid"], [_VALID_FQN])
        self.assertEqual(result["banned"], [_BANNED_FQN])

    def test_only_banned_fqns_returns_none(self):
        rel = "chrome/android/Foo.java"
        self._write(rel, _java_source(_BANNED_FQN))
        self.assertIsNone(fc.check_file(rel, self.root))

    def test_clean_file_returns_none(self):
        rel = "chrome/android/Clean.java"
        self._write(rel, _java_source())
        self.assertIsNone(fc.check_file(rel, self.root))
        self.mock_is_valid.assert_not_called()

    def test_missing_file_returns_none(self):
        self.assertIsNone(fc.check_file("does/not/Exist.java", self.root))
        self.assertIsNone(
            fc.check_file(os.path.join(self.root, "Missing.java"), self.root)
        )


if __name__ == "__main__":
    unittest.main()
