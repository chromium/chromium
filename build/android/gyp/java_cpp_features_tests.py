#!/usr/bin/env python3

# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for java_cpp_features.py.

This test suite contains various tests for the C++ -> Java base::Feature
generator.
"""

import unittest

import java_cpp_features
from util import java_cpp_utils


class _TestFeaturesParser(unittest.TestCase):
  def testParseComments(self):
    test_data = """
/**
 * This should be ignored as well.
 */

// Comment followed by a blank line.

// Comment followed by unrelated code.
int foo() { return 3; }

// Real comment. base::Feature intentionally split across two lines.
BASE_FEATURE(kSomeFeature, "SomeFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Real comment that spans
// multiple lines.
BASE_FEATURE(kSomeOtherFeature, "SomeOtherFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Comment followed by nothing.
""".split('\n')
    feature_file_parser = java_cpp_utils.CppConstantParser(
        java_cpp_features.FeatureParserDelegate(), test_data)
    features = feature_file_parser.Parse()
    self.assertEqual(2, len(features))
    self.assertEqual('SOME_FEATURE', features[0].name)
    self.assertEqual('"SomeFeature"', features[0].value)
    self.assertEqual(1, len(features[0].comments.split('\n')))
    self.assertEqual('SOME_OTHER_FEATURE', features[1].name)
    self.assertEqual('"SomeOtherFeature"', features[1].value)
    self.assertEqual(2, len(features[1].comments.split('\n')))

  def testWhitespace(self):
    test_data = """
// 1 line
BASE_FEATURE(kShort, "Short", base::FEATURE_DISABLED_BY_DEFAULT);

// 2 lines
BASE_FEATURE(kTwoLineFeatureA, "TwoLineFeatureA",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTwoLineFeatureB,
    "TwoLineFeatureB", base::FEATURE_DISABLED_BY_DEFAULT);

// 3 lines
BASE_FEATURE(kFeatureWithAVeryLongNameThatWillHaveToWrap,
    "FeatureWithAVeryLongNameThatWillHaveToWrap",
    base::FEATURE_DISABLED_BY_DEFAULT);
""".split('\n')
    feature_file_parser = java_cpp_utils.CppConstantParser(
        java_cpp_features.FeatureParserDelegate(), test_data)
    features = feature_file_parser.Parse()
    self.assertEqual(4, len(features))
    self.assertEqual('SHORT', features[0].name)
    self.assertEqual('"Short"', features[0].value)
    self.assertEqual('TWO_LINE_FEATURE_A', features[1].name)
    self.assertEqual('"TwoLineFeatureA"', features[1].value)
    self.assertEqual('TWO_LINE_FEATURE_B', features[2].name)
    self.assertEqual('"TwoLineFeatureB"', features[2].value)
    self.assertEqual('FEATURE_WITH_A_VERY_LONG_NAME_THAT_WILL_HAVE_TO_WRAP',
                     features[3].name)
    self.assertEqual('"FeatureWithAVeryLongNameThatWillHaveToWrap"',
                     features[3].value)

  def testCppSyntax(self):
    test_data = """
// Mismatched name
BASE_FEATURE(kMismatchedFeature, "MismatchedName",
    base::FEATURE_DISABLED_BY_DEFAULT);

namespace myfeature {
// In a namespace
BASE_FEATURE(kSomeFeature, "SomeFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);
}

// Build config-specific base::Feature
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kAndroidOnlyFeature, "AndroidOnlyFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Value depends on build config
BASE_FEATURE(kMaybeEnabled, "MaybeEnabled",
#if BUILDFLAG(IS_ANDROID)
    base::FEATURE_DISABLED_BY_DEFAULT
#else
    base::FEATURE_ENABLED_BY_DEFAULT
#endif
);
""".split('\n')
    feature_file_parser = java_cpp_utils.CppConstantParser(
        java_cpp_features.FeatureParserDelegate(), test_data)
    features = feature_file_parser.Parse()
    self.assertEqual(4, len(features))
    self.assertEqual('MISMATCHED_FEATURE', features[0].name)
    self.assertEqual('"MismatchedName"', features[0].value)
    self.assertEqual('SOME_FEATURE', features[1].name)
    self.assertEqual('"SomeFeature"', features[1].value)
    self.assertEqual('ANDROID_ONLY_FEATURE', features[2].name)
    self.assertEqual('"AndroidOnlyFeature"', features[2].value)
    self.assertEqual('MAYBE_ENABLED', features[3].name)
    self.assertEqual('"MaybeEnabled"', features[3].value)

  def testNotYetSupported(self):
    # Negative test for cases we don't yet support, to ensure we don't misparse
    # these until we intentionally add proper support.
    test_data = """
// Not currently supported: name depends on C++ directive
BASE_FEATURE(kNameDependsOnOs,
#if BUILDFLAG(IS_ANDROID)
    "MaybeName1",
#else
    "MaybeName2",
#endif
    base::FEATURE_DISABLED_BY_DEFAULT);

// Not currently supported: feature named with a constant instead of literal
BASE_FEATURE(kNamedAfterConstant, kNamedStringConstant,
             base::FEATURE_DISABLED_BY_DEFAULT};
""".split('\n')
    feature_file_parser = java_cpp_utils.CppConstantParser(
        java_cpp_features.FeatureParserDelegate(), test_data)
    features = feature_file_parser.Parse()
    self.assertEqual(0, len(features))

  def testTreatWebViewLikeOneWord(self):
    test_data = """
BASE_FEATURE(kSomeWebViewFeature, "SomeWebViewFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kWebViewOtherFeature, "WebViewOtherFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kFeatureWithPluralWebViews,
    "FeatureWithPluralWebViews",
    base::FEATURE_ENABLED_BY_DEFAULT);
""".split('\n')
    feature_file_parser = java_cpp_utils.CppConstantParser(
        java_cpp_features.FeatureParserDelegate(), test_data)
    features = feature_file_parser.Parse()
    self.assertEqual('SOME_WEBVIEW_FEATURE', features[0].name)
    self.assertEqual('"SomeWebViewFeature"', features[0].value)
    self.assertEqual('WEBVIEW_OTHER_FEATURE', features[1].name)
    self.assertEqual('"WebViewOtherFeature"', features[1].value)
    self.assertEqual('FEATURE_WITH_PLURAL_WEBVIEWS', features[2].name)
    self.assertEqual('"FeatureWithPluralWebViews"', features[2].value)

  def testSpecialCharacters(self):
    test_data = r"""
BASE_FEATURE(kFeatureWithEscapes, "Weird\tfeature\"name\n",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFeatureWithEscapes2,
    "Weird\tfeature\"name\n",
    base::FEATURE_ENABLED_BY_DEFAULT);
""".split('\n')
    feature_file_parser = java_cpp_utils.CppConstantParser(
        java_cpp_features.FeatureParserDelegate(), test_data)
    features = feature_file_parser.Parse()
    self.assertEqual('FEATURE_WITH_ESCAPES', features[0].name)
    self.assertEqual(r'"Weird\tfeature\"name\n"', features[0].value)
    self.assertEqual('FEATURE_WITH_ESCAPES2', features[1].name)
    self.assertEqual(r'"Weird\tfeature\"name\n"', features[1].value)


if __name__ == '__main__':
  unittest.main()
