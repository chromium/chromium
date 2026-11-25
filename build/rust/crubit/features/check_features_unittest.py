#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

sys.path.append(os.path.dirname(__file__))
import check_features
from check_features import (
    FindMissingFeatures,
    FindStaleExclusions,
    FindStaleFastTrackedFeatures,
    FindUnexpectedFeatures,
    ParseBzlFeatures,
    ParseGniList,
)


def _ReadRealGniFile():
    with open(check_features.GNI_PATH) as f:
        gni_text = f.read()
    globally_enabled = ParseGniList(
        gni_text, check_features.GNI_GLOBALLY_ENABLED_VARIABLE
    )
    fast_tracked = ParseGniList(
        gni_text, check_features.GNI_FAST_TRACKED_VARIABLE
    )
    return globally_enabled, fast_tracked


class ParseBzlFeaturesTest(unittest.TestCase):
    def test_basic(self):
        bzl = '''
"""Docstring."""

visibility(["//..."])

SUPPORTED_FEATURES = [
    "supported",
    # Comment.
    "foo",  # Trailing comment.
]

OTHER = ["bar"]
'''
        self.assertEqual(ParseBzlFeatures(bzl), ['supported', 'foo'])

    def test_missing_variable(self):
        with self.assertRaisesRegex(ValueError, 'SUPPORTED_FEATURES'):
            ParseBzlFeatures('OTHER = ["foo"]\n')

    def test_not_a_literal(self):
        with self.assertRaisesRegex(ValueError, 'SUPPORTED_FEATURES'):
            ParseBzlFeatures('SUPPORTED_FEATURES = BASE + ["foo"]\n')


class ParseGniListTest(unittest.TestCase):
    def test_basic(self):
        gni = '''
import("//build/config/rust.gni")

fast_list = [
  # Comment.
  "bar",
]

# Comment.
some_list = [
  # Comment.
  "foo",

  # Another comment.
  "supported",
]
some_list += fast_list

other_list = [ "baz" ]
'''
        self.assertEqual(ParseGniList(gni, 'some_list'), ['foo', 'supported'])
        self.assertEqual(ParseGniList(gni, 'fast_list'), ['bar'])

    def test_missing_variable(self):
        with self.assertRaisesRegex(ValueError, 'some_list'):
            ParseGniList('other_list = [ "bar" ]\n', 'some_list')

    def test_real_gni_file(self):
        globally_enabled, fast_tracked = _ReadRealGniFile()
        self.assertIn('supported', globally_enabled)
        self.assertNotIn('supported', fast_tracked)


class FindMissingFeaturesTest(unittest.TestCase):
    def test_nothing_missing(self):
        self.assertEqual(
            FindMissingFeatures(
                upstream=['a', 'b'],
                chromium=['a', 'b', 'chromium_only'],
                exclusions={},
            ),
            [],
        )

    def test_missing(self):
        self.assertEqual(
            FindMissingFeatures(
                upstream=['c', 'a', 'b'], chromium=['a'], exclusions={}
            ),
            ['b', 'c'],
        )

    def test_excluded(self):
        self.assertEqual(
            FindMissingFeatures(
                upstream=['a', 'b', 'c'],
                chromium=['a'],
                exclusions={'b': 'Some reason.'},
            ),
            ['c'],
        )


class FindUnexpectedFeaturesTest(unittest.TestCase):
    def test_nothing_unexpected(self):
        self.assertEqual(
            FindUnexpectedFeatures(
                upstream=['a', 'b'], chromium=['a'], fast_tracked=[]
            ),
            [],
        )

    def test_unexpected(self):
        self.assertEqual(
            FindUnexpectedFeatures(
                upstream=['a'], chromium=['c', 'a', 'b'], fast_tracked=[]
            ),
            ['b', 'c'],
        )

    def test_fast_tracked(self):
        self.assertEqual(
            FindUnexpectedFeatures(
                upstream=['a'], chromium=['a', 'b', 'c'], fast_tracked=['b']
            ),
            ['c'],
        )


class FindStaleFastTrackedFeaturesTest(unittest.TestCase):
    def test_no_stale_fast_tracked_features(self):
        self.assertEqual(
            FindStaleFastTrackedFeatures(upstream=['a'], fast_tracked=['b']),
            [],
        )

    def test_upstream_now(self):
        self.assertEqual(
            FindStaleFastTrackedFeatures(
                upstream=['a', 'c', 'b'], fast_tracked=['c', 'b', 'd']
            ),
            ['b', 'c'],
        )


class FindStaleExclusionsTest(unittest.TestCase):
    def test_no_stale_exclusions(self):
        self.assertEqual(
            FindStaleExclusions(
                upstream=['a', 'b'],
                chromium=['a'],
                exclusions={'b': 'Some reason.'},
            ),
            [],
        )

    def test_not_upstream_anymore(self):
        self.assertEqual(
            FindStaleExclusions(
                upstream=['a'], chromium=['a'], exclusions={'b': 'Some reason.'}
            ),
            ['b'],
        )

    def test_enabled_in_chromium(self):
        self.assertEqual(
            FindStaleExclusions(
                upstream=['a', 'b'],
                chromium=['a', 'b'],
                exclusions={'b': 'Some reason.'},
            ),
            ['b'],
        )

    def test_real_exclusions_are_not_enabled_in_chromium(self):
        globally_enabled, fast_tracked = _ReadRealGniFile()
        for feature in check_features.KNOWN_EXCLUSIONS:
            self.assertNotIn(feature, globally_enabled)
            self.assertNotIn(feature, fast_tracked)


if __name__ == '__main__':
    unittest.main()
