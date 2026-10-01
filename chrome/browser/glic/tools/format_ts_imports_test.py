#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import io
import os
import sys
import tempfile
import unittest
from pathlib import Path

# Ensure the directory containing format_ts_imports is in sys.path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import format_ts_imports


class FormatTsImportsTest(unittest.TestCase):

    def test_short_import_unchanged(self):
        content = "import { A, B, C } from './short.js';"
        result = format_ts_imports.rewrite_imports(content, threshold=120, min_symbols=2)
        self.assertEqual(result, content)

    def test_long_import_two_symbols_unchanged(self):
        content = (
            "import { VeryLongSymbolNameNumberOneHereForTestingPurposes, "
            "VeryLongSymbolNameNumberTwoHereForTestingPurposes } from './long_path_to_module.js';"
        )
        self.assertGreater(len(content), 120)
        result = format_ts_imports.rewrite_imports(content, threshold=120, min_symbols=2)
        self.assertEqual(result, content)

    def test_long_import_three_symbols_rewritten(self):
        content = (
            "import { FirstSymbolName, SecondSymbolNameWithLongerName, "
            "ThirdSymbolNameWithEvenLongerNameToExceedLineLength } from './long_path_to_module.js';"
        )
        self.assertGreater(len(content), 120)
        expected = (
            "import {//\n"
            "  FirstSymbolName,//\n"
            "  SecondSymbolNameWithLongerName,//\n"
            "  ThirdSymbolNameWithEvenLongerNameToExceedLineLength,//\n"
            "} from './long_path_to_module.js';"
        )
        result = format_ts_imports.rewrite_imports(content, threshold=120, min_symbols=2)
        self.assertEqual(result, expected)

    def test_short_multiline_with_trailing_comments_collapsed(self):
        content = (
            "import {\n"
            "  Foo, //\n"
            "  Bar, //\n"
            "  Baz, //\n"
            "} from 'taco.js';"
        )
        expected = "import {Foo, Bar, Baz} from 'taco.js';"
        result = format_ts_imports.rewrite_imports(content, threshold=120, min_symbols=2)
        self.assertEqual(result, expected)

    def test_short_multiline_with_inline_comment_collapsed(self):
        content = "import { Foo, Bar, //\n Baz} from 'taco.js';"
        expected = "import {Foo, Bar, Baz} from 'taco.js';"
        result = format_ts_imports.rewrite_imports(content, threshold=120, min_symbols=2)
        self.assertEqual(result, expected)

    def test_long_multiline_import_with_line_comment(self):
        content = (
            "import { FirstSymbolName, SecondSymbolNameWithLongerName, //\n"
            " ThirdSymbolNameWithEvenLongerNameToExceedLineLength } from './long_path_to_module.js';"
        )
        expected = (
            "import {//\n"
            "  FirstSymbolName,//\n"
            "  SecondSymbolNameWithLongerName,//\n"
            "  ThirdSymbolNameWithEvenLongerNameToExceedLineLength,//\n"
            "} from './long_path_to_module.js';"
        )
        result = format_ts_imports.rewrite_imports(content, threshold=120, min_symbols=2)
        self.assertEqual(result, expected)

    def test_import_type_rewritten(self):
        content = (
            "import type { InterfaceDef, PendingReceiver, PostMessageLifecycleObserver, "
            "PostMessageRemote, PostMessageRouter } from '../transport/post_message_transport.js';"
        )
        self.assertGreater(len(content), 120)
        expected = (
            "import type {//\n"
            "  InterfaceDef,//\n"
            "  PendingReceiver,//\n"
            "  PostMessageLifecycleObserver,//\n"
            "  PostMessageRemote,//\n"
            "  PostMessageRouter,//\n"
            "} from '../transport/post_message_transport.js';"
        )
        result = format_ts_imports.rewrite_imports(content, threshold=120, min_symbols=2)
        self.assertEqual(result, expected)

    def test_default_and_named_import(self):
        content = (
            "import DefaultExport, { FirstSymbolName, SecondSymbolNameWithLongerName, "
            "ThirdSymbolNameWithEvenLongerNameToExceedLineLength } from './long_path_to_module.js';"
        )
        self.assertGreater(len(content), 120)
        expected = (
            "import DefaultExport, {//\n"
            "  FirstSymbolName,//\n"
            "  SecondSymbolNameWithLongerName,//\n"
            "  ThirdSymbolNameWithEvenLongerNameToExceedLineLength,//\n"
            "} from './long_path_to_module.js';"
        )
        result = format_ts_imports.rewrite_imports(content, threshold=120, min_symbols=2)
        self.assertEqual(result, expected)

    def test_mixed_comments_stripped_and_reformatted(self):
        content = (
            "import {\n"
            "  // Leading comment with { braces }\n"
            "  FirstSymbolNameWithLongName, // trailing comment with , commas\n"
            "  /* inline block comment */ SecondSymbolNameWithLongerName,\n"
            "  ThirdSymbolNameWithEvenLongerNameToExceedLineLength, // already has //\n"
            "} from './long_path_to_module.js'; // trailing import comment"
        )
        expected = (
            "import {//\n"
            "  FirstSymbolNameWithLongName,//\n"
            "  SecondSymbolNameWithLongerName,//\n"
            "  ThirdSymbolNameWithEvenLongerNameToExceedLineLength,//\n"
            "} from './long_path_to_module.js'; // trailing import comment"
        )
        result = format_ts_imports.rewrite_imports(content, threshold=120, min_symbols=2)
        self.assertEqual(result, expected)

    def test_clang_format_integration(self):
        content = (
            "import { FirstSymbolName, SecondSymbolNameWithLongerName, "
            "ThirdSymbolNameWithEvenLongerNameToExceedLineLength } from './long_path_to_module.js';"
        )
        rewritten = format_ts_imports.rewrite_imports(content, threshold=120, min_symbols=2)
        formatted = format_ts_imports.run_clang_format(rewritten, assume_filename="test.ts")
        self.assertIn("FirstSymbolName,", formatted)
        self.assertIn("SecondSymbolNameWithLongerName,", formatted)
        self.assertIn("ThirdSymbolNameWithEvenLongerNameToExceedLineLength,", formatted)
        # Each line should end with trailing comment //
        for line in formatted.strip().splitlines()[:-1]:
            self.assertTrue(line.endswith("//"))

    def test_check_only_and_in_place(self):
        unformatted_content = (
            "import { FirstSymbolName, SecondSymbolNameWithLongerName, "
            "ThirdSymbolNameWithEvenLongerNameToExceedLineLength } from './long_path_to_module.js';\n"
        )

        with tempfile.TemporaryDirectory() as tmp_dir:
            file_path = Path(tmp_dir) / "test.ts"
            file_path.write_text(unformatted_content, encoding="utf-8")

            # 1. check_only=True should report changes needed without modifying file
            with contextlib.redirect_stdout(io.StringIO()):
                needs_change = format_ts_imports.process_file(
                    file_path, threshold=120, min_symbols=2, in_place=False, check_only=True
                )
            self.assertTrue(needs_change)
            self.assertEqual(file_path.read_text(encoding="utf-8"), unformatted_content)

            # 2. in_place=True should write formatted content to disk
            with contextlib.redirect_stdout(io.StringIO()):
                needs_change = format_ts_imports.process_file(
                    file_path, threshold=120, min_symbols=2, in_place=True, check_only=False
                )
            self.assertTrue(needs_change)
            formatted_content = file_path.read_text(encoding="utf-8")
            self.assertNotEqual(formatted_content, unformatted_content)
            self.assertIn("//", formatted_content)

            # 3. Running check_only=True on now formatted file should report no changes needed
            with contextlib.redirect_stdout(io.StringIO()):
                needs_change = format_ts_imports.process_file(
                    file_path, threshold=120, min_symbols=2, in_place=False, check_only=True
                )
            self.assertFalse(needs_change)


if __name__ == "__main__":
    unittest.main()
