#! /usr/bin/env vpython3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from xml.etree import ElementTree

from pylib.utils import dexdump

# pylint: disable=protected-access

emptyAnnotations = dexdump.Annotations(classAnnotations={},
                                       methodsAnnotations={})


class DexdumpXMLParseTest(unittest.TestCase):

  def testParseAnnotations(self):
    example_xml_string = (
        '<package name="com.foo.bar">\n'
        'Class #1 annotations:\n'
        'Annotations on class\n'
        ' VISIBILITY_RUNTIME Ldalvik/annotation/AppModeFull; value=Alpha\n'
        'Annotations on method #512 \'example\'\n'
        ' VISIBILITY_SYSTEM Ldalvik/annotation/Signature; value=Bravo\n'
        ' VISIBILITY_RUNTIME Ldalvik/annotation/Test;\n'
        ' VISIBILITY_RUNTIME Ldalvik/annotation/Test2; value=Charlie\n'
        ' VISIBILITY_RUNTIME Ldalvik/annotation/Test3; A=B x B={ C D }\n'
        ' VISIBILITY_RUNTIME Ldalvik/annotation/Test4; A=B x B={ C D } C=D\n'
        '<class name="Class1" extends="java.lang.Object">\n'
        '</class>\n'
        '<class name="Class2" extends="java.lang.Object">\n'
        '</class>\n'
        '</package>\n')

    actual = dexdump._ParseAnnotations(example_xml_string)

    expected = {
        1:
        dexdump.Annotations(
            classAnnotations={'AppModeFull': {
                'value': 'Alpha'
            }},
            methodsAnnotations={
                'example': {
                    'Test': None,
                    'Test2': {
                        'value': 'Charlie'
                    },
                    'Test3': {
                        'A': 'B x',
                        'B': ['C', 'D']
                    },
                    'Test4': {
                        'A': 'B x',
                        'B': ['C', 'D'],
                        'C': 'D'
                    },
                }
            },
        )
    }

    self.assertEqual(expected, actual)

  def testParseRootXmlNode(self):
    example_xml_string = ('<api>'
                          '<package name="com.foo.bar1">'
                          '<class'
                          '  name="Class1"'
                          '  extends="java.lang.Object"'
                          '  abstract="false"'
                          '  static="false"'
                          '  final="true"'
                          '  visibility="public">'
                          '<method'
                          '  name="class1Method1"'
                          '  return="java.lang.String"'
                          '  abstract="false"'
                          '  native="false"'
                          '  synchronized="false"'
                          '  static="false"'
                          '  final="false"'
                          '  visibility="public">'
                          '</method>'
                          '<method'
                          '  name="class1Method2"'
                          '  return="viod"'
                          '  abstract="false"'
                          '  native="false"'
                          '  synchronized="false"'
                          '  static="false"'
                          '  final="false"'
                          '  visibility="public">'
                          '</method>'
                          '</class>'
                          '<class'
                          '  name="Class2"'
                          '  extends="java.lang.Object"'
                          '  abstract="true"'
                          '  static="false"'
                          '  final="true"'
                          '  visibility="public">'
                          '<method'
                          '  name="class2Method1"'
                          '  return="java.lang.String"'
                          '  abstract="false"'
                          '  native="false"'
                          '  synchronized="false"'
                          '  static="false"'
                          '  final="false"'
                          '  visibility="public">'
                          '</method>'
                          '</class>'
                          '</package>'
                          '<package name="com.foo.bar2">'
                          '</package>'
                          '<package name="com.foo.bar3">'
                          '</package>'
                          '</api>')

    actual = dexdump._ParseRootNode(ElementTree.fromstring(example_xml_string),
                                    {})

    expected = {
        'com.foo.bar1': {
            'classes': {
                'Class1': {
                    'methods': ['class1Method1', 'class1Method2'],
                    'superclass': 'java.lang.Object',
                    'is_abstract': False,
                    'annotations': emptyAnnotations,
                },
                'Class2': {
                    'methods': ['class2Method1'],
                    'superclass': 'java.lang.Object',
                    'is_abstract': True,
                    'annotations': emptyAnnotations,
                }
            },
        },
        'com.foo.bar2': {
            'classes': {}
        },
        'com.foo.bar3': {
            'classes': {}
        },
    }
    self.assertEqual(expected, actual)

  def testParsePackageNode(self):
    example_xml_string = (
        '<package name="com.foo.bar">'
        '<class name="Class1" extends="java.lang.Object">'
        '</class>'
        '<class name="Class2" extends="java.lang.Object" abstract="true">'
        '</class>'
        '</package>')


    (actual, classCount) = dexdump._ParsePackageNode(
        ElementTree.fromstring(example_xml_string), 0, {})

    expected = {
        'classes': {
            'Class1': {
                'methods': [],
                'superclass': 'java.lang.Object',
                'is_abstract': False,
                'annotations': emptyAnnotations,
            },
            'Class2': {
                'methods': [],
                'superclass': 'java.lang.Object',
                'is_abstract': True,
                'annotations': emptyAnnotations,
            },
        },
    }
    self.assertEqual(expected, actual)
    self.assertEqual(classCount, 2)

  def testParseClassNode(self):
    example_xml_string = ('<class name="Class1" extends="java.lang.Object">'
                          '<method name="method1" visibility="public">'
                          '</method>'
                          '<method name="method2" visibility="public">'
                          '</method>'
                          '<method name="method3" visibility="private">'
                          '</method>'
                          '</class>')

    actual = dexdump._ParseClassNode(ElementTree.fromstring(example_xml_string),
                                     0, {})

    expected = {
        'methods': ['method1', 'method2'],
        'superclass': 'java.lang.Object',
        'is_abstract': False,
        'annotations': emptyAnnotations,
    }
    self.assertEqual(expected, actual)


if __name__ == '__main__':
  unittest.main()
