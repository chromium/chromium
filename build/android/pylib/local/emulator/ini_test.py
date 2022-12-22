#! /usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for ini.py."""


import os
import sys
import textwrap
import unittest

sys.path.append(
    os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..')))
from pylib.local.emulator import ini


class IniTest(unittest.TestCase):
  def testLoadsBasic(self):
    ini_str = textwrap.dedent("""\
        foo.bar = 1
        foo.baz= example
        bar.bad =/path/to/thing

        [section_1]
        foo.bar = 1
        foo.baz= example

        [section_2]
        foo.baz= example
        bar.bad =/path/to/thing

        [section_1]
        bar.bad =/path/to/thing
        """)
    expected = {
        'foo.bar': '1',
        'foo.baz': 'example',
        'bar.bad': '/path/to/thing',
        'section_1': {
            'foo.bar': '1',
            'foo.baz': 'example',
            'bar.bad': '/path/to/thing',
        },
        'section_2': {
            'foo.baz': 'example',
            'bar.bad': '/path/to/thing',
        },
    }
    self.assertEqual(expected, ini.loads(ini_str))

  def testLoadsDuplicatedKeysStrictFailure(self):
    ini_str = textwrap.dedent("""\
        foo.bar = 1
        foo.baz = example
        bar.bad = /path/to/thing
        foo.bar = duplicate
        """)
    with self.assertRaises(ValueError):
      ini.loads(ini_str, strict=True)

  def testLoadsDuplicatedKeysInSectionStrictFailure(self):
    ini_str = textwrap.dedent("""\
        [section_1]
        foo.bar = 1
        foo.baz = example
        bar.bad = /path/to/thing
        foo.bar = duplicate
        """)
    with self.assertRaises(ValueError):
      ini.loads(ini_str, strict=True)

  def testLoadsPermissive(self):
    ini_str = textwrap.dedent("""\
        foo.bar = 1
        foo.baz = example
        bar.bad = /path/to/thing
        foo.bar = duplicate

        [section_1]
        foo.bar = 1
        foo.baz = example
        bar.bad = /path/to/thing
        foo.bar = duplicate
        """)
    expected = {
        'foo.bar': 'duplicate',
        'foo.baz': 'example',
        'bar.bad': '/path/to/thing',
        'section_1': {
            'foo.bar': 'duplicate',
            'foo.baz': 'example',
            'bar.bad': '/path/to/thing',
        },
    }
    self.assertEqual(expected, ini.loads(ini_str, strict=False))

  def testDumpsBasic(self):
    ini_contents = {
        'foo.bar': '1',
        'foo.baz': 'example',
        'bar.bad': '/path/to/thing',
        'section_2': {
            'foo.baz': 'example',
            'bar.bad': '/path/to/thing',
        },
        'section_1': {
            'foo.bar': '1',
            'foo.baz': 'example',
        },
    }
    # ini.dumps is expected to dump to string alphabetically
    # by key and section name.
    expected = textwrap.dedent("""\
        bar.bad = /path/to/thing
        foo.bar = 1
        foo.baz = example

        [section_1]
        foo.bar = 1
        foo.baz = example

        [section_2]
        bar.bad = /path/to/thing
        foo.baz = example
        """)
    self.assertEqual(expected, ini.dumps(ini_contents))

  def testDumpsSections(self):
    ini_contents = {
        'section_2': {
            'foo.baz': 'example',
            'bar.bad': '/path/to/thing',
        },
        'section_1': {
            'foo.bar': '1',
            'foo.baz': 'example',
        },
    }
    # ini.dumps is expected to dump to string alphabetically
    # by key first, and then by section and the associated keys
    expected = textwrap.dedent("""\
        [section_1]
        foo.bar = 1
        foo.baz = example

        [section_2]
        bar.bad = /path/to/thing
        foo.baz = example
        """)
    self.assertEqual(expected, ini.dumps(ini_contents))


if __name__ == '__main__':
  unittest.main()
