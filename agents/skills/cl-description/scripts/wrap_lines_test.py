#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for wrap_lines.py"""

import unittest
from wrap_lines import wrap_text


class WrapLinesTest(unittest.TestCase):
    """Tests for wrap_text function."""

    def test_empty_text(self):
        """Test empty text returns empty string."""
        self.assertEqual(wrap_text(""), "")
        self.assertEqual(wrap_text("   "), "")

    def test_single_line_subject(self):
        """Test single-line subject without body is returned as-is."""
        subject = "[Component] Simple subject line"
        self.assertEqual(wrap_text(subject), subject)

    def test_subject_and_body_short_lines(self):
        """Test subject and body with short lines are preserved with
        empty line."""
        text = "[Component] Subject\nShort body line."
        expected = "[Component] Subject\n\nShort body line."
        self.assertEqual(wrap_text(text), expected)

    def test_long_body_lines_wrapping(self):
        """Test long body lines are wrapped to the specified width."""
        subject = "[Component] Subject"
        long_line = (
            "This is a very long line that should be wrapped to "
            "seventy-two characters by the script because it exceeds the "
            "length limit.")
        text = f"{subject}\n{long_line}"

        result = wrap_text(text, width=72)

        self.assertTrue(result.startswith(subject))
        parts = result.split('\n\n', 1)
        self.assertEqual(len(parts), 2)
        body = parts[1]

        for line in body.split('\n'):
            self.assertLessEqual(len(line), 72)

    def test_multiple_paragraphs(self):
        """Test multiple paragraphs retain their separation."""
        subject = "[Comp] Subject"
        p1 = "Paragraph one with some text."
        p2 = "Paragraph two with some more text."
        text = f"{subject}\n{p1}\n\n{p2}"

        result = wrap_text(text)
        expected = f"{subject}\n\n{p1}\n\n{p2}"
        self.assertEqual(result, expected)


if __name__ == '__main__':
    unittest.main()
