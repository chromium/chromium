#!/usr/bin/env python3

# Copyright 2020 The Chromium Authors. All rights reserved.
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

// Real comment.
const base::Feature kSomeFeature{"SomeFeature",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

// Real comment that spans
// multiple lines.
const base::Feature kSomeOtherFeature{"SomeOtherFeature",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

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
const base::Feature kShort{"Short", base::FEATURE_DISABLED_BY_DEFAULT};

// 2 lines
const base::Feature kTwoLineFeatureA{"TwoLineFeatureA",
                                     base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kTwoLineFeatureB{
    "TwoLineFeatureB", base::FEATURE_DISABLED_BY_DEFAULT};

// 3 lines
const base::Feature kFeatureWithAVeryLongNameThatWillHaveToWrap{
    "FeatureWithAVeryLongNameThatWillHaveToWrap",
    base::FEATURE_DISABLED_BY_DEFAULT};
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
const base::Feature kMismatchedFeature{"MismatchedName",
    base::FEATURE_DISABLED_BY_DEFAULT};

namespace myfeature {
// In a namespace
const base::Feature kSomeFeature{"SomeFeature",
                                 base::FEATURE_DISABLED_BY_DEFAULT};
}

// Defined with equals sign
const base::Feature kFoo = {"Foo", base::FEATURE_DISABLED_BY_DEFAULT};

// Build config-specific base::Feature
#if defined(OS_ANDROID)
const base::Feature kAndroidOnlyFeature{"AndroidOnlyFeature",
                                        base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Value depends on build config
const base::Feature kMaybeEnabled{"MaybeEnabled",
#if defined(OS_ANDROID)
    base::FEATURE_DISABLED_BY_DEFAULT
#else
    base::FEATURE_ENABLED_BY_DEFAULT
#endif
};
""".split('\n')
    feature_file_parser = java_cpp_utils.CppConstantParser(
        java_cpp_features.FeatureParserDelegate(), test_data)
    features = feature_file_parser.Parse()
    self.assertEqual(5, len(features))
    self.assertEqual('MISMATCHED_FEATURE', features[0].name)
    self.assertEqual('"MismatchedName"', features[0].value)
    self.assertEqual('SOME_FEATURE', features[1].name)
    self.assertEqual('"SomeFeature"', features[1].value)
    self.assertEqual('FOO', features[2].name)
    self.assertEqual('"Foo"', features[2].value)
    self.assertEqual('ANDROID_ONLY_FEATURE', features[3].name)
    self.assertEqual('"AndroidOnlyFeature"', features[3].value)
    self.assertEqual('MAYBE_ENABLED', features[4].name)
    self.assertEqual('"MaybeEnabled"', features[4].value)

  def testNotYetSupported(self):
    # Negative test for cases we don't yet support, to ensure we don't misparse
    # these until we intentionally add proper support.
    test_data = """
// Not currently supported: name depends on C++ directive
const base::Feature kNameDependsOnOs{
#if defined(OS_ANDROID)
    "MaybeName1",
#else
    "MaybeName2",
#endif
    base::FEATURE_DISABLED_BY_DEFAULT};

// Not currently supported: feature named with a constant instead of literal
const base::Feature kNamedAfterConstant{kNamedStringConstant,
                                        base::FEATURE_DISABLED_BY_DEFAULT};
""".split('\n')
    feature_file_parser = java_cpp_utils.CppConstantParser(
        java_cpp_features.FeatureParserDelegate(), test_data)
    features = feature_file_parser.Parse()
    self.assertEqual(0, len(features))

  def testTreatWebViewLikeOneWord(self):
    test_data = """
const base::Feature kSomeWebViewFeature{"SomeWebViewFeature",
                                        base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kWebViewOtherFeature{"WebViewOtherFeature",
                                         base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kFeatureWithPluralWebViews{
    "FeatureWithPluralWebViews",
    base::FEATURE_ENABLED_BY_DEFAULT};
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
const base::Feature kFeatureWithEscapes{"Weird\tfeature\"name\n",
                                        base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kFeatureWithEscapes2{
    "Weird\tfeature\"name\n",
    base::FEATURE_ENABLED_BY_DEFAULT};
""".split('\n')
    feature_file_parser = java_cpp_utils.CppConstantParser(
        java_cpp_features.FeatureParserDelegate(), test_data)
    features = feature_file_parser.Parse()
    self.assertEqual('FEATURE_WITH_ESCAPES', features[0].name)
    self.assertEqual(r'"Weird\tfeature\"name\n"', features[0].value)
    self.assertEqual('FEATURE_WITH_ESCAPES2', features[1].name)
    self.assertEqual(r'"Weird\tfeature\"name\n"', features[1].value)

  def testNoBaseNamespacePrefix(self):
    test_data = """
const Feature kSomeFeature{"SomeFeature", FEATURE_DISABLED_BY_DEFAULT};
""".split('\n')
    feature_file_parser = java_cpp_utils.CppConstantParser(
        java_cpp_features.FeatureParserDelegate(), test_data)
    features = feature_file_parser.Parse()
    self.assertEqual('SOME_FEATURE', features[0].name)
    self.assertEqual('"SomeFeature"', features[0].value)


if __name__ == '__main__':
  unittest.main()
