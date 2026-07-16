#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

sys.path.insert(1, os.path.join(os.path.dirname(__file__), '..'))
from util import java_cpp_utils


class JavaCppUtilsTest(unittest.TestCase):

  def testProcessListMacros_noMacros(self):
    lines = [
        'enum class Foo {\n',
        '  A,\n',
        '  B,\n',
        '};\n',
    ]
    self.assertEqual(lines, java_cpp_utils.ProcessListMacros(lines))

  def testProcessListMacros_unusedMacro(self):
    lines = [
        '#define MY_MACRO(V) \\\n',
        '  V(A) \\\n',
        '  V(B)\n',
        '\n',
        'enum class Foo {\n',
        '  C,\n',
        '};\n',
    ]
    self.assertEqual(lines, java_cpp_utils.ProcessListMacros(lines))

  def testProcessListMacros_simpleExpansion(self):
    lines = [
        '#define MY_MACRO(V) \\\n',
        '  V(A) \\\n',
        '  V(B)\n',
        '#define SOME_VISITOR(x) x,\n',
        '\n',
        'enum class Foo { MY_MACRO(SOME_VISITOR) };\n',
    ]
    expected = [
        '#define MY_MACRO(V) \\\n',
        '  V(A) \\\n',
        '  V(B)\n',
        '#define SOME_VISITOR(x) x,\n',
        '\n',
        'enum class Foo { \n',
        'A,\n',
        'B,\n',
        ' };\n',
    ]
    self.assertEqual(expected, java_cpp_utils.ProcessListMacros(lines))

  def testProcessListMacros_expansionWithCommentsAndSpaces(self):
    lines = [
        '#define MY_MACRO(V)     \\\n',
        '  /* Comment */         \\\n',
        '  V(0, A, "ValueA")     \\\n',
        '  V(1, B, "ValueB")\n',
        '#define VISITOR(x, y, z) y,\n',
        '\n',
        'enum class Foo {\n',
        '  MY_MACRO(VISITOR)\n',
        '};\n',
    ]
    expected = [
        '#define MY_MACRO(V)     \\\n',
        '  /* Comment */         \\\n',
        '  V(0, A, "ValueA")     \\\n',
        '  V(1, B, "ValueB")\n',
        '#define VISITOR(x, y, z) y,\n',
        '\n',
        'enum class Foo {\n',
        '  \n',
        'A,\n',
        'B,\n',
        '\n',
        '};\n',
    ]
    self.assertEqual(expected, java_cpp_utils.ProcessListMacros(lines))

  def testProcessListMacros_multipleMacros(self):
    lines = [
        '#define MACRO_A(V) \\\n',
        '  V(A1) \\\n',
        '  V(A2)\n',
        '#define MACRO_B(V) \\\n',
        '  V(B1) \\\n',
        '  V(B2)\n',
        '#define VISITOR(x) x,\n',
        '\n',
        'enum class Foo { MACRO_A(VISITOR) };\n',
        'enum class Bar { MACRO_B(VISITOR) };\n',
    ]
    expected = [
        '#define MACRO_A(V) \\\n',
        '  V(A1) \\\n',
        '  V(A2)\n',
        '#define MACRO_B(V) \\\n',
        '  V(B1) \\\n',
        '  V(B2)\n',
        '#define VISITOR(x) x,\n',
        '\n',
        'enum class Foo { \n',
        'A1,\n',
        'A2,\n',
        ' };\n',
        'enum class Bar { \n',
        'B1,\n',
        'B2,\n',
        ' };\n',
    ]
    self.assertEqual(expected, java_cpp_utils.ProcessListMacros(lines))

  def testProcessListMacros_enumWithVisitor(self):
    lines = [
        '#define ENUM_MACRO(V) \\\n',
        '  V(A) \\\n',
        '  V(B)\n',
        '#define MY_VISITOR(x) x = 1,\n',
        'enum { ENUM_MACRO(MY_VISITOR) };\n',
    ]
    expected = [
        '#define ENUM_MACRO(V) \\\n',
        '  V(A) \\\n',
        '  V(B)\n',
        '#define MY_VISITOR(x) x = 1,\n',
        'enum { \n',
        'A = 1,\n',
        'B = 1,\n',
        ' };\n',
    ]
    self.assertEqual(expected, java_cpp_utils.ProcessListMacros(lines))

  def testProcessListMacros_strings(self):
    lines = [
        '#define STRINGS(V) \\\n',
        '  V(kMyString, "value, with comma") \\\n',
        '  V(kAnother, "value2")\n',
        '#define DEFINE_STRING(name, val) const char name[] = val;\n',
        'STRINGS(DEFINE_STRING)\n',
    ]
    expected = [
        '#define STRINGS(V) \\\n',
        '  V(kMyString, "value, with comma") \\\n',
        '  V(kAnother, "value2")\n',
        '#define DEFINE_STRING(name, val) const char name[] = val;\n',
        '\n',
        'const char kMyString[] = "value, with comma";\n',
        'const char kAnother[] = "value2";\n',
        '\n',
    ]
    self.assertEqual(expected, java_cpp_utils.ProcessListMacros(lines))

  def testProcessListMacros_features(self):
    lines = [
        '#define FEATURES(V) \\\n',
        '  V(kMyFeature, "MyFeature", base::FEATURE_DISABLED_BY_DEFAULT)\n',
        '#define DEFINE_FEATURE(name, str, status) BASE_FEATURE(name, str, status);\n',
        'FEATURES(DEFINE_FEATURE)\n',
    ]
    expected = [
        '#define FEATURES(V) \\\n',
        '  V(kMyFeature, "MyFeature", base::FEATURE_DISABLED_BY_DEFAULT)\n',
        '#define DEFINE_FEATURE(name, str, status) BASE_FEATURE(name, str, status);\n',
        '\n',
        'BASE_FEATURE(kMyFeature, "MyFeature", base::FEATURE_DISABLED_BY_DEFAULT);\n',
        '\n',
    ]
    self.assertEqual(expected, java_cpp_utils.ProcessListMacros(lines))

  def testProcessListMacros_nestedParentheses(self):
    lines = [
        '#define STRINGS(V) \\\n',
        '  V(kMyString, Bar(1, 2)) \\\n',
        '  V(kAnother, "value2")\n',
        '#define DEFINE_STRING(name, val) const char name[] = val;\n',
        'STRINGS(DEFINE_STRING)\n',
    ]
    expected = [
        '#define STRINGS(V) \\\n',
        '  V(kMyString, Bar(1, 2)) \\\n',
        '  V(kAnother, "value2")\n',
        '#define DEFINE_STRING(name, val) const char name[] = val;\n',
        '\n',
        'const char kMyString[] = Bar(1, 2);\n',
        'const char kAnother[] = "value2";\n',
        '\n',
    ]
    self.assertEqual(expected, java_cpp_utils.ProcessListMacros(lines))

  def testProcessListMacros_missingVisitorError(self):
    lines = [
        '#define ENUM_MACRO(V) \\\n',
        '  V(A) \\\n',
        '  V(B)\n',
        'enum { ENUM_MACRO(MISSING_VISITOR) };\n',
    ]
    with self.assertRaises(Exception) as context:
      java_cpp_utils.ProcessListMacros(lines)
    self.assertIn(
        "Visitor macro 'MISSING_VISITOR' used in 'ENUM_MACRO' call "
        "is not defined in the file.", str(context.exception))


if __name__ == '__main__':
  unittest.main()
