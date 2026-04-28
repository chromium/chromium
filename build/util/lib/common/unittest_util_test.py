#!/usr/bin/env python3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=protected-access

import logging
import sys
import unittest
import unittest_util


class FilterTestNamesTest(unittest.TestCase):

  possible_list = ["Foo.One",
                   "Foo.Two",
                   "Foo.Three",
                   "Bar.One",
                   "Bar.Two",
                   "Bar.Three",
                   "Quux.One",
                   "Quux.Two",
                   "Quux.Three"]

  def testMatchAll(self):
    x = unittest_util.FilterTestNames(self.possible_list, "*")
    self.assertEquals(x, self.possible_list)

  def testMatchPartial(self):
    x = unittest_util.FilterTestNames(self.possible_list, "Foo.*")
    self.assertEquals(x, ["Foo.One", "Foo.Two", "Foo.Three"])

  def testMatchFull(self):
    x = unittest_util.FilterTestNames(self.possible_list, "Foo.Two")
    self.assertEquals(x, ["Foo.Two"])

  def testMatchTwo(self):
    x = unittest_util.FilterTestNames(self.possible_list, "Bar.*:Foo.*")
    self.assertEquals(x, ["Bar.One",
                          "Bar.Two",
                          "Bar.Three",
                          "Foo.One",
                          "Foo.Two",
                          "Foo.Three"])

  def testMatchWithNegative(self):
    x = unittest_util.FilterTestNames(self.possible_list, "Bar.*:Foo.*-*.Three")
    self.assertEquals(x, ["Bar.One",
                          "Bar.Two",
                          "Foo.One",
                          "Foo.Two"])

  def testMatchOverlapping(self):
    x = unittest_util.FilterTestNames(self.possible_list, "Bar.*:*.Two")
    self.assertEquals(x, ["Bar.One",
                          "Bar.Two",
                          "Bar.Three",
                          "Foo.Two",
                          "Quux.Two"])

  def testMatchWithStrippedName(self):
    possible_list = ["Suite.Test", "Suite.PRE_Test", "Other.Test"]

    def strip_pre(test):
      return test.replace("PRE_", "")

    x = unittest_util.FilterTestNames(possible_list,
                                      "Suite.Test",
                                      test_name_stripped_func=strip_pre)
    self.assertEquals(x, ["Suite.Test", "Suite.PRE_Test"])

    x = unittest_util.FilterTestNames(possible_list,
                                      "Suite.*",
                                      test_name_stripped_func=strip_pre)
    self.assertEquals(x, ["Suite.Test", "Suite.PRE_Test"])

    x = unittest_util.FilterTestNames(possible_list,
                                      "-Suite.Test",
                                      test_name_stripped_func=strip_pre)
    self.assertEquals(x, ["Other.Test"])

  def testMatchWithStrippedNameExplicitPreTest(self):
    possible_list = ["Suite.Test", "Suite.PRE_Test", "Other.Test"]

    def strip_pre(test):
      return test.replace("PRE_", "")

    x = unittest_util.FilterTestNames(possible_list,
                                      "Suite.PRE_Test",
                                      test_name_stripped_func=strip_pre)
    self.assertEquals(x, ["Suite.PRE_Test"])


if __name__ == '__main__':
  logging.getLogger().setLevel(logging.DEBUG)
  unittest.main(verbosity=2)
