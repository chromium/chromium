#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Basic tests for chromium_docs.py"""
# pylint: disable=protected-access

import unittest
from pathlib import Path

from chromium_docs import ChromiumDocs, SearchResult


class ChromiumDocsTest(unittest.TestCase):
    """Tests for ChromiumDocs class."""

    def test_default_config(self):
        """Test default configuration is valid."""
        docs = ChromiumDocs()
        config = docs._default_config()

        self.assertIn('indexing', config)
        self.assertIn('search', config)
        self.assertIn('scan_patterns', config['indexing'])

    def test_extract_title_h1(self):
        """Test title extraction from H1 heading."""
        docs = ChromiumDocs()
        content = "# My Document Title\n\nSome content here."
        title = docs._extract_title(content)

        self.assertEqual(title, "My Document Title")

    def test_extract_title_h2_fallback(self):
        """Test title extraction falls back to H2."""
        docs = ChromiumDocs()
        content = "Some intro text\n\n## Secondary Title\n\nContent."
        title = docs._extract_title(content)

        self.assertEqual(title, "Secondary Title")

    def test_extract_title_untitled(self):
        """Test untitled document handling."""
        docs = ChromiumDocs()
        content = "Just some content without headers."
        title = docs._extract_title(content)

        self.assertEqual(title, "Untitled Document")

    def test_extract_keywords(self):
        """Test keyword extraction finds Chromium terms."""
        docs = ChromiumDocs()
        content = "The browser process communicates with renderer via mojo."
        keywords = docs._extract_keywords(content)

        self.assertIn('browser', keywords)
        self.assertIn('renderer', keywords)
        self.assertIn('mojo', keywords)

    def test_categorize_testing_doc(self):
        """Test categorization of testing documents."""
        docs = ChromiumDocs()
        path = Path("/src/testing/guide.md")
        content = "How to write browser_test cases."
        category = docs._categorize_document(path, content)

        self.assertEqual(category, 'testing')

    def test_categorize_gpu_doc(self):
        """Test categorization of GPU documents."""
        docs = ChromiumDocs()
        path = Path("/src/gpu/README.md")
        content = "GPU process architecture."
        category = docs._categorize_document(path, content)

        self.assertEqual(category, 'gpu')

    def test_calculate_score_title_match(self):
        """Test scoring prioritizes title matches."""
        docs = ChromiumDocs()
        doc_data = {
            'title': 'Mojo IPC Guide',
            'content': 'How to use mojo for IPC.',
            'keywords': ['mojo', 'ipc']
        }

        score = docs._calculate_score(doc_data, ['mojo'])
        self.assertGreater(score, 0)

    def test_calculate_score_no_match(self):
        """Test scoring returns zero for no matches."""
        docs = ChromiumDocs()
        doc_data = {
            'title': 'GPU Architecture',
            'content': 'Graphics processing.',
            'keywords': ['gpu', 'graphics']
        }

        score = docs._calculate_score(doc_data, ['network', 'http'])
        self.assertEqual(score, 0)

    def test_extract_excerpt(self):
        """Test excerpt extraction finds relevant line."""
        docs = ChromiumDocs()
        content = "First line.\nThis line mentions mojo bindings.\nLast line."
        excerpt = docs._extract_excerpt(content, ['mojo'])

        self.assertIn('mojo', excerpt.lower())

    def test_search_result_dataclass(self):
        """Test SearchResult dataclass."""
        result = SearchResult(path="docs/test.md",
                              title="Test Doc",
                              summary="A test document.",
                              score=5.0,
                              category="testing",
                              keywords=["test"])

        self.assertEqual(result.path, "docs/test.md")
        self.assertEqual(result.score, 5.0)


if __name__ == '__main__':
    unittest.main()
