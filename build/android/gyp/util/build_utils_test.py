#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import os
import sys
import unittest

sys.path.insert(
    0, os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir)))
from util import build_utils

_DEPS = collections.OrderedDict()
_DEPS['a'] = []
_DEPS['b'] = []
_DEPS['c'] = ['a']
_DEPS['d'] = ['a']
_DEPS['e'] = ['f']
_DEPS['f'] = ['a', 'd']
_DEPS['g'] = []
_DEPS['h'] = ['d', 'b', 'f']
_DEPS['i'] = ['f']


class BuildUtilsTest(unittest.TestCase):
  def testGetSortedTransitiveDependencies_all(self):
    TOP = _DEPS.keys()
    EXPECTED = ['a', 'b', 'c', 'd', 'f', 'e', 'g', 'h', 'i']
    actual = build_utils.GetSortedTransitiveDependencies(TOP, _DEPS.get)
    self.assertEqual(EXPECTED, actual)

  def testGetSortedTransitiveDependencies_leaves(self):
    TOP = ['c', 'e', 'g', 'h', 'i']
    EXPECTED = ['a', 'c', 'd', 'f', 'e', 'g', 'b', 'h', 'i']
    actual = build_utils.GetSortedTransitiveDependencies(TOP, _DEPS.get)
    self.assertEqual(EXPECTED, actual)

  def testGetSortedTransitiveDependencies_leavesReverse(self):
    TOP = ['i', 'h', 'g', 'e', 'c']
    EXPECTED = ['a', 'd', 'f', 'i', 'b', 'h', 'g', 'e', 'c']
    actual = build_utils.GetSortedTransitiveDependencies(TOP, _DEPS.get)
    self.assertEqual(EXPECTED, actual)


if __name__ == '__main__':
  unittest.main()
