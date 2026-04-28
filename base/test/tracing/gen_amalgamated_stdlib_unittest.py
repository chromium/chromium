#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest
from pyfakefs import fake_filesystem_unittest

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import gen_amalgamated_stdlib


class GenAmalgamatedStdlibTest(fake_filesystem_unittest.TestCase):

    def setUp(self):
        self.setUpPyfakefs()

    def test_filename_to_variable(self):
        self.assertEqual(gen_amalgamated_stdlib.filename_to_variable("foo"),
                         "kFoo")
        self.assertEqual(
            gen_amalgamated_stdlib.filename_to_variable("foo_bar"), "kFooBar")
        self.assertEqual(
            gen_amalgamated_stdlib.filename_to_variable("foo-bar"), "kFooBar")
        self.assertEqual(
            gen_amalgamated_stdlib.filename_to_variable(
                f"foo{os.path.sep}bar"), "kFooBar")

    def test_main_basic(self):
        cpp_out = "/fake/output.cc"
        sql_file1 = "/fake/file1.sql"
        sql_file2 = "/fake/file2.sql"
        self.fs.create_file(sql_file1, contents="SELECT * FROM table1;")
        self.fs.create_file(sql_file2, contents="SELECT * FROM table2;")

        gen_amalgamated_stdlib.main(
            ["--cpp-out", cpp_out, sql_file1, sql_file2])

        self.assertTrue(self.fs.exists(cpp_out))
        with open(cpp_out, 'r') as f:
            content = f.read()
            self.assertIn("namespace base::test {", content)
            self.assertIn("kFile1", content)
            self.assertIn("kFile2", content)
            self.assertIn("kChromeStdlibFilesToSql", content)
            self.assertIn("}  // namespace base::test", content)


if __name__ == '__main__':
    unittest.main()
