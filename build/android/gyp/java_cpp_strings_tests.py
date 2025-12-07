#!/usr/bin/env python3

# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for java_cpp_strings.py.

This test suite contains various tests for the C++ -> Java string generator.
"""

import unittest

import java_cpp_strings
from util import java_cpp_utils


class _TestStringsParser(unittest.TestCase):

  def testParseComments(self):
    test_data = """
/**
 * This should be ignored as well.
 */

// Comment followed by a blank line.

// Comment followed by unrelated code.
int foo() { return 3; }

// Real comment.
const char kASwitch[] = "a-value";

// Real comment that spans
// multiple lines.
const char kAnotherSwitch[] = "another-value";

// Comment followed by nothing.
""".split('\n')
    string_file_parser = java_cpp_utils.CppConstantParser(
        java_cpp_strings.StringParserDelegate(), test_data)
    strings = string_file_parser.Parse()
    self.assertEqual(2, len(strings))
    self.assertEqual('A_SWITCH', strings[0].name)
    self.assertEqual('"a-value"', strings[0].value)
    self.assertEqual(1, len(strings[0].comments.split('\n')))
    self.assertEqual('ANOTHER_SWITCH', strings[1].name)
    self.assertEqual('"another-value"', strings[1].value)
    self.assertEqual(2, len(strings[1].comments.split('\n')))

  def testStringValues(self):
    test_data = r"""
// Single line string constants.
const char kAString[] = "a-value";
const char kNoComment[] = "no-comment";

namespace myfeature {
const char kMyFeatureNoComment[] = "myfeature.no-comment";
}

// Single line switch with a big space.
const char kAStringWithSpace[]                      = "a-value";

// Wrapped constant definition.
const char kAStringWithAVeryLongNameThatWillHaveToWrap[] =
    "a-string-with-a-very-long-name-that-will-have-to-wrap";

// This one has no comment before it.

const char kAStringWithAVeryLongNameThatWillHaveToWrap2[] =
    "a-string-with-a-very-long-name-that-will-have-to-wrap2";

const char kStringWithEscapes[] = "tab\tquote\"newline\n";
const char kStringWithEscapes2[] =
    "tab\tquote\"newline\n";

const char kEmptyString[] = "";

// These are valid C++ but not currently supported by the script.
const char kInvalidLineBreak[] =

    "invalid-line-break";

const char kConcatenateMultipleStringLiterals[] =
    "first line"
    "second line";
""".split('\n')
    string_file_parser = java_cpp_utils.CppConstantParser(
        java_cpp_strings.StringParserDelegate(), test_data)
    strings = string_file_parser.Parse()
    self.assertEqual(9, len(strings))
    self.assertEqual('A_STRING', strings[0].name)
    self.assertEqual('"a-value"', strings[0].value)
    self.assertEqual('NO_COMMENT', strings[1].name)
    self.assertEqual('"no-comment"', strings[1].value)
    self.assertEqual('MY_FEATURE_NO_COMMENT', strings[2].name)
    self.assertEqual('"myfeature.no-comment"', strings[2].value)
    self.assertEqual('A_STRING_WITH_SPACE', strings[3].name)
    self.assertEqual('"a-value"', strings[3].value)
    self.assertEqual('A_STRING_WITH_A_VERY_LONG_NAME_THAT_WILL_HAVE_TO_WRAP',
                     strings[4].name)
    self.assertEqual('"a-string-with-a-very-long-name-that-will-have-to-wrap"',
                     strings[4].value)
    self.assertEqual('A_STRING_WITH_A_VERY_LONG_NAME_THAT_WILL_HAVE_TO_WRAP2',
                     strings[5].name)
    self.assertEqual('"a-string-with-a-very-long-name-that-will-have-to-wrap2"',
                     strings[5].value)
    self.assertEqual('STRING_WITH_ESCAPES', strings[6].name)
    self.assertEqual(r'"tab\tquote\"newline\n"', strings[6].value)
    self.assertEqual('STRING_WITH_ESCAPES2', strings[7].name)
    self.assertEqual(r'"tab\tquote\"newline\n"', strings[7].value)
    self.assertEqual('EMPTY_STRING', strings[8].name)
    self.assertEqual('""', strings[8].value)

  def testTreatWebViewLikeOneWord(self):
    test_data = """
const char kSomeWebViewSwitch[] = "some-webview-switch";
const char kWebViewOtherSwitch[] = "webview-other-switch";
const char kSwitchWithPluralWebViews[] = "switch-with-plural-webviews";
""".split('\n')
    string_file_parser = java_cpp_utils.CppConstantParser(
        java_cpp_strings.StringParserDelegate(), test_data)
    strings = string_file_parser.Parse()
    self.assertEqual('SOME_WEBVIEW_SWITCH', strings[0].name)
    self.assertEqual('"some-webview-switch"', strings[0].value)
    self.assertEqual('WEBVIEW_OTHER_SWITCH', strings[1].name)
    self.assertEqual('"webview-other-switch"', strings[1].value)
    self.assertEqual('SWITCH_WITH_PLURAL_WEBVIEWS', strings[2].name)
    self.assertEqual('"switch-with-plural-webviews"', strings[2].value)

  def testTemplateParsing(self):
    test_data = """
// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package my.java.package;

public any sort of class MyClass {{

{NATIVE_STRINGS}

}}
"""
    package, class_name = java_cpp_utils.ParseTemplateFile(test_data)
    self.assertEqual('my.java.package', package)
    self.assertEqual('MyClass', class_name)

  def testParseStringsWithConditionallyDefinedValues(self):
    test_data = """
#if BUILDFLAG(IS_CHROMEOS)
  const char kMyTestString1[] = "test-string-1";
#endif
#if BUILDFLAG(IS_ANDROID)
  const char kMyTestString1[] = "test-string-1";
#endif
#if BUILDFLAG(IS_WIN)
  const char kMyTestString1[] = "test-string-1";
#endif
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  const char kMyTestString2[] = "test-string-2";
#if BUILDFLAG(IS_POSIX)
  const char kMyTestString3[] = "test-string-3";
#endif
#endif
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  const char kMyTestString4[] = "test-string-4";
#endif

#if BUILDFLAG(IS_POSIX)
  const char kMyTestString5[] = "test-string-5";
#endif

#if !BUILDFLAG(IS_WIN)
  const char kMyTestString6[] = "test-string-6";
#endif
#if !BUILDFLAG(IS_ANDROID)
  const char kMyTestString7[] = "test-string-7";
#else
  const char kMyTestString8[] = "test-string-8";
#endif
#if !BUILDFLAG(IS_POSIX)
  const char kMyTestString9[] = "test-string-9";
#endif

#if BUILDFLAG(ENABLE_HLS_DEMUXER)
  const char kMyTestString10[] = "test-string-10";
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#if BUILDFLAG(IS_ANDROID)
  const char kMyTestString11[] = "test-string-11";
#else
  const char kMyTestString12[] = "test-string-12";
#endif
#endif

#if !BUILDFLAG(IS_WIN)
  const char kMyTestString13[] = "test-string-13";
#else
  const char kMyTestString14[] = "test-string-14";
#endif

};
    """.split('\n')
    string_file_parser = java_cpp_utils.CppConstantParser(
        java_cpp_strings.StringParserDelegate(), test_data)
    strings = string_file_parser.Parse()
    self.assertEqual(8, len(strings))
    self.assertEqual('MY_TEST_STRING1', strings[0].name)
    self.assertEqual('"test-string-1"', strings[0].value)
    self.assertEqual('MY_TEST_STRING4', strings[1].name)
    self.assertEqual('"test-string-4"', strings[1].value)
    self.assertEqual('MY_TEST_STRING5', strings[2].name)
    self.assertEqual('"test-string-5"', strings[2].value)
    self.assertEqual('MY_TEST_STRING6', strings[3].name)
    self.assertEqual('"test-string-6"', strings[3].value)
    self.assertEqual('MY_TEST_STRING8', strings[4].name)
    self.assertEqual('"test-string-8"', strings[4].value)
    self.assertEqual('MY_TEST_STRING10', strings[5].name)
    self.assertEqual('"test-string-10"', strings[5].value)
    self.assertEqual('MY_TEST_STRING11', strings[6].name)
    self.assertEqual('"test-string-11"', strings[6].value)

if __name__ == '__main__':
  unittest.main()
