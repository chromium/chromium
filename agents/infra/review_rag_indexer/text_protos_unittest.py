#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for text_protos.py."""

import unittest

import text_protos


class TextProtoParserTest(unittest.TestCase):

    def test_parse_simple(self):
        content = """
        component: "Blink"
        team_email: "blink-dev@chromium.org"
        """
        result = text_protos.parse_text_proto(content)
        self.assertEqual(result, {
            'component': 'Blink',
            'team_email': 'blink-dev@chromium.org'
        })

    def test_parse_nested(self):
        content = """
        monorail {
          project: "chromium"
          component: "Blink>Layout"
        }
        """
        result = text_protos.parse_text_proto(content)
        self.assertEqual(
            result,
            {'monorail': {
                'project': 'chromium',
                'component': 'Blink>Layout'
            }})

    def test_parse_list(self):
        content = """
        mixins: ["//pdf/pdf.mixin", "//v8/v8.mixin"]
        """
        result = text_protos.parse_text_proto(content)
        self.assertEqual(result,
                         {'mixins': ['//pdf/pdf.mixin', '//v8/v8.mixin']})

    def test_parse_comments(self):
        content = """
        # This is a comment
        component: "Blink" # another comment
        """
        result = text_protos.parse_text_proto(content)
        self.assertEqual(result, {'component': 'Blink'})


if __name__ == '__main__':
    unittest.main()
