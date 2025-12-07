#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for compile_java.py"""

import collections
import unittest

import compile_java


def _CreateData(class_annotation='',
                class_prefix='public ',
                nested_annotation='',
                suffix=''):
  return f"""\
package pkg;

import foo;
import foo.Bar;
import foo.Bar.Baz;

{class_annotation}
@SomeThing
{class_prefix}class Foo {{
  {nested_annotation}
  public static class Nested {{ }}
}}
""" + suffix


class CompileJavaTests(unittest.TestCase):

  def do_service_loader_test(self, **kwargs):
    data = _CreateData(**kwargs)
    services_map = collections.defaultdict(list)
    compile_java.ParseJavaSource(data, services_map)
    return dict(services_map)

  def do_classes_test(self, **kwargs):
    data = _CreateData(**kwargs)
    services_map = collections.defaultdict(list)
    _, class_names = compile_java.ParseJavaSource(data, services_map)
    return class_names

  def testServiceImpl_NoUses(self):
    services_map = self.do_service_loader_test()
    self.assertEqual({}, services_map)

  def testServiceImpl_LocalType(self):
    services_map = self.do_service_loader_test(
        class_annotation='@ServiceImpl(Local.class)')
    self.assertEqual({'pkg.Local': ['pkg.Foo']}, services_map)

  def testServiceImpl_ImportedTopType(self):
    services_map = self.do_service_loader_test(
        class_annotation='@ServiceImpl(Bar.class)')
    self.assertEqual({'foo.Bar': ['pkg.Foo']}, services_map)

  def testServiceImpl_ImportedNestedType1(self):
    services_map = self.do_service_loader_test(
        class_annotation='@ServiceImpl(Baz.class)')
    self.assertEqual({'foo.Bar$Baz': ['pkg.Foo']}, services_map)

  def testServiceImpl_ImportedNestedType2(self):
    services_map = self.do_service_loader_test(
        class_annotation='@ServiceImpl(Bar.Baz.class)')
    self.assertEqual({'foo.Bar$Baz': ['pkg.Foo']}, services_map)

  def testServiceImpl_NestedImpl(self):
    services_map = self.do_service_loader_test(
        class_annotation='@ServiceImpl(Baz.class)',
        nested_annotation='@ServiceImpl(Baz.class)')
    self.assertEqual({'foo.Bar$Baz': ['pkg.Foo', 'pkg.Foo$Nested']},
                     services_map)

  def testParseClasses(self):
    classes = self.do_classes_test(class_prefix='public final ',
                                   suffix='\nprivate class Extra {}')
    self.assertEqual(['Foo', 'Extra'], classes)

  def testErrorOnNonPublic(self):

    def inner():
      self.do_classes_test(class_annotation='@ServiceImpl(Local.class)',
                           class_prefix='')

    self.assertRaises(Exception, inner)

if __name__ == '__main__':
  unittest.main()
