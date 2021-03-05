#! /usr/bin/env python
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for ini.py."""

import textwrap
import unittest

from pylib.local.emulator import ini


class IniTest(unittest.TestCase):
  def testLoadsBasic(self):
    ini_str = textwrap.dedent("""\
        foo.bar = 1
        foo.baz= example
        bar.bad =/path/to/thing
        """)
    expected = {
        'foo.bar': '1',
        'foo.baz': 'example',
        'bar.bad': '/path/to/thing',
    }
    self.assertEqual(expected, ini.loads(ini_str))

  def testLoadsStrictFailure(self):
    ini_str = textwrap.dedent("""\
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
        """)
    expected = {
        'foo.bar': 'duplicate',
        'foo.baz': 'example',
        'bar.bad': '/path/to/thing',
    }
    self.assertEqual(expected, ini.loads(ini_str, strict=False))

  def testDumpsBasic(self):
    ini_contents = {
        'foo.bar': '1',
        'foo.baz': 'example',
        'bar.bad': '/path/to/thing',
    }
    # ini.dumps is expected to dump to string alphabetically
    # by key.
    expected = textwrap.dedent("""\
        bar.bad = /path/to/thing
        foo.bar = 1
        foo.baz = example
        """)
    self.assertEqual(expected, ini.dumps(ini_contents))


if __name__ == '__main__':
  unittest.main()
