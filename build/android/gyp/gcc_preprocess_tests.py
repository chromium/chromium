#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for gcc_preprocess.py

This test suite contains various tests for the 'java_cpp_template' build rule,
which uses the gcc preprocessor to turn a template into Java code.
"""

import unittest
import tempfile

import gcc_preprocess


class TestPreprocess(unittest.TestCase):

  def testParsePackageName(self):
    with tempfile.NamedTemporaryFile(mode='w') as f:
      template = f.name
      f.file.write("""
package org.chromium.fake;
public class Empty {
}
""")
      f.file.flush()
      package_name, data = gcc_preprocess.ProcessJavaFile(template, [], [])
      self.assertEqual('org.chromium.fake', package_name)
      self.assertEqual(
          """
package org.chromium.fake;
public class Empty {
}
""".strip(), data.strip())

  def testMissingPackageName(self):
    with tempfile.NamedTemporaryFile(mode='w') as f:
      template = f.name
      f.file.write("""
public class Empty {
}
""")
      f.file.flush()
      with self.assertRaisesRegex(Exception,
                                  r'Could not find java package of.*'):
        gcc_preprocess.ProcessJavaFile(template, [], [])

  def testSinglePreprocessorEvaluation(self):
    with tempfile.NamedTemporaryFile(mode='w') as f:
      template = f.name
      f.file.write("""
package org.chromium.fake;
public class Sample {
#if defined(_ENABLE_ASSERTS)
    public boolean ENABLE_ASSERTS = true;
#else
    public boolean ENABLE_ASSERTS = false;
#endif
}
""")
      f.file.flush()
      defines = [
          '_ENABLE_ASSERTS',
      ]
      package_name, data = gcc_preprocess.ProcessJavaFile(template, defines, [])
      self.assertEqual('org.chromium.fake', package_name)
      self.assertEqual(
          """
package org.chromium.fake;
public class Sample {
    public boolean ENABLE_ASSERTS = true;
}
""".strip(), data.strip())

  def testNestedPreprocessorEvaluation(self):
    with tempfile.NamedTemporaryFile(mode='w') as f:
      template = f.name
      f.file.write("""
package org.chromium.fake;
#if defined(USE_FINAL)
#define MAYBE_FINAL final
#else
#define MAYBE_FINAL
#endif
public class Sample {
#if defined(_ENABLE_ASSERTS)
    public MAYBE_FINAL boolean ENABLE_ASSERTS = true;
#else
    public MAYBE_FINAL boolean ENABLE_ASSERTS = false;
#endif
}
""")
      f.file.flush()
      defines = [
          '_ENABLE_ASSERTS',
          'USE_FINAL',
      ]
      package_name, data = gcc_preprocess.ProcessJavaFile(template, defines, [])
      self.assertEqual('org.chromium.fake', package_name)
      self.assertEqual(
          """
package org.chromium.fake;
public class Sample {
    public final boolean ENABLE_ASSERTS = true;
}
""".strip(), data.strip())

  def testPreserveComments(self):
    with tempfile.NamedTemporaryFile(mode='w') as f:
      template = f.name
      f.file.write("""
// Copyright header ...
package org.chromium.fake;
/**
 * Some javadoc.
 */
public class Sample {
    // This is a comment outside the #if block.
#if defined(_ENABLE_ASSERTS)
    // Inside the #if block.
    public boolean ENABLE_ASSERTS = true;
#else
    // Inside the #else block.
    public boolean ENABLE_ASSERTS = false;
#endif
}
""")
      f.file.flush()
      defines = [
          '_ENABLE_ASSERTS',
      ]
      package_name, data = gcc_preprocess.ProcessJavaFile(template, defines, [])
      self.assertEqual('org.chromium.fake', package_name)
      self.assertEqual(
          """
// Copyright header ...
package org.chromium.fake;
/**
 * Some javadoc.
 */
public class Sample {
    // This is a comment outside the #if block.
    // Inside the #if block.
    public boolean ENABLE_ASSERTS = true;
}
""".strip(), data.strip())


if __name__ == '__main__':
  unittest.main()
