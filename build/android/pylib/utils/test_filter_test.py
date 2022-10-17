#!/usr/bin/env vpython3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys
import tempfile
import unittest

from pylib.utils import test_filter

class ParseFilterFileTest(unittest.TestCase):

  def testParseFilterFile_commentsAndBlankLines(self):
    input_lines = [
      'positive1',
      '# comment',
      'positive2  # Another comment',
      ''
      'positive3'
    ]
    actual = test_filter.ParseFilterFile(input_lines)
    expected = ['positive1', 'positive2', 'positive3'], []
    self.assertEqual(expected, actual)

  def testParseFilterFile_onlyPositive(self):
    input_lines = [
      'positive1',
      'positive2'
    ]
    actual = test_filter.ParseFilterFile(input_lines)
    expected = ['positive1', 'positive2'], []
    self.assertEqual(expected, actual)

  def testParseFilterFile_onlyNegative(self):
    input_lines = [
      '-negative1',
      '-negative2'
    ]
    actual = test_filter.ParseFilterFile(input_lines)
    expected = [], ['negative1', 'negative2']
    self.assertEqual(expected, actual)

  def testParseFilterFile_positiveAndNegative(self):
    input_lines = [
      'positive1',
      'positive2',
      '-negative1',
      '-negative2'
    ]
    actual = test_filter.ParseFilterFile(input_lines)
    expected = ['positive1', 'positive2'], ['negative1', 'negative2']
    self.assertEqual(expected, actual)


class InitializeFilterFromArgsTest(unittest.TestCase):

  def testInitializeBasicFilter(self):
    parser = argparse.ArgumentParser()
    test_filter.AddFilterOptions(parser)
    args = parser.parse_args([
        '--test-filter',
        'FooTest.testFoo:BarTest.testBar'])
    expected = ['FooTest.testFoo:BarTest.testBar']
    actual = test_filter.InitializeFiltersFromArgs(args)
    self.assertEqual(actual, expected)

  def testInitializeJavaStyleFilter(self):
    parser = argparse.ArgumentParser()
    test_filter.AddFilterOptions(parser)
    args = parser.parse_args([
        '--test-filter',
        'FooTest#testFoo:BarTest#testBar'])
    expected = ['FooTest.testFoo:BarTest.testBar']
    actual = test_filter.InitializeFiltersFromArgs(args)
    self.assertEqual(actual, expected)

  def testInitializeBasicIsolatedScript(self):
    parser = argparse.ArgumentParser()
    test_filter.AddFilterOptions(parser)
    args = parser.parse_args([
        '--isolated-script-test-filter',
        'FooTest.testFoo::BarTest.testBar'])
    expected = ['FooTest.testFoo:BarTest.testBar']
    actual = test_filter.InitializeFiltersFromArgs(args)
    self.assertEqual(actual, expected)

  @unittest.skipIf(os.name == "nt", "Opening NamedTemporaryFile by name "
                   "doesn't work in Windows.")
  def testFilterArgWithPositiveFilterInFilterFile(self):
    parser = argparse.ArgumentParser()
    test_filter.AddFilterOptions(parser)
    with tempfile.NamedTemporaryFile(mode='w') as tmp_file:
      tmp_file.write('positive1\npositive2\n-negative2\n-negative3\n')
      tmp_file.seek(0)
      args = parser.parse_args([
          '--test-filter=-negative1',
          '--test-launcher-filter-file',
          tmp_file.name])
      expected = ['-negative1', 'positive1:positive2-negative2:negative3']
      actual = test_filter.InitializeFiltersFromArgs(args)
      self.assertEqual(actual, expected)

  @unittest.skipIf(os.name == "nt", "Opening NamedTemporaryFile by name "
                   "doesn't work in Windows.")
  def testFilterFileWithPositiveFilterInFilterArg(self):
    parser = argparse.ArgumentParser()
    test_filter.AddFilterOptions(parser)
    with tempfile.NamedTemporaryFile(mode='w') as tmp_file:
      tmp_file.write('-negative2\n-negative3\n')
      tmp_file.seek(0)
      args = parser.parse_args([
          '--test-filter',
          'positive1:positive2-negative1',
          '--test-launcher-filter-file',
          tmp_file.name])
      expected = ['positive1:positive2-negative1', '-negative2:negative3']
      actual = test_filter.InitializeFiltersFromArgs(args)
      self.assertEqual(actual, expected)

  @unittest.skipIf(os.name == "nt", "Opening NamedTemporaryFile by name "
                   "doesn't work in Windows.")
  def testPositiveFilterInBothFileAndArg(self):
    parser = argparse.ArgumentParser()
    test_filter.AddFilterOptions(parser)
    with tempfile.NamedTemporaryFile(mode='w') as tmp_file:
      tmp_file.write('positive2-negative2\n')
      tmp_file.seek(0)
      args = parser.parse_args([
          '--test-filter', 'positive1-negative1', '--test-launcher-filter-file',
          tmp_file.name
      ])
      expected = ['positive1-negative1', 'positive2-negative2']
      actual = test_filter.InitializeFiltersFromArgs(args)
      self.assertEqual(actual, expected)

  @unittest.skipIf(os.name == "nt", "Opening NamedTemporaryFile by name "
                   "doesn't work in Windows.")
  def testFilterArgWithFilterFileAllNegative(self):
    parser = argparse.ArgumentParser()
    test_filter.AddFilterOptions(parser)
    with tempfile.NamedTemporaryFile(mode='w') as tmp_file:
      tmp_file.write('-negative3\n-negative4\n')
      tmp_file.seek(0)
      args = parser.parse_args([
          '--test-filter=-negative1:negative2',
          '--test-launcher-filter-file',
          tmp_file.name])
      expected = ['-negative1:negative2', '-negative3:negative4']
      actual = test_filter.InitializeFiltersFromArgs(args)
      self.assertEqual(actual, expected)


class AppendPatternsToFilter(unittest.TestCase):
  def testAllEmpty(self):
    expected = ''
    actual = test_filter.AppendPatternsToFilter('', [], [])
    self.assertEqual(actual, expected)

  def testAppendOnlyPositiveToEmptyFilter(self):
    expected = 'positive'
    actual = test_filter.AppendPatternsToFilter('', ['positive'])
    self.assertEqual(actual, expected)

  def testAppendOnlyNegativeToEmptyFilter(self):
    expected = '-negative'
    actual = test_filter.AppendPatternsToFilter('',
                                                negative_patterns=['negative'])
    self.assertEqual(actual, expected)

  def testAppendToEmptyFilter(self):
    expected = 'positive-negative'
    actual = test_filter.AppendPatternsToFilter('', ['positive'], ['negative'])
    self.assertEqual(actual, expected)

  def testAppendToPositiveOnlyFilter(self):
    expected = 'positive1:positive2-negative'
    actual = test_filter.AppendPatternsToFilter('positive1', ['positive2'],
                                                ['negative'])
    self.assertEqual(actual, expected)

  def testAppendToNegativeOnlyFilter(self):
    expected = 'positive-negative1:negative2'
    actual = test_filter.AppendPatternsToFilter('-negative1', ['positive'],
                                                ['negative2'])
    self.assertEqual(actual, expected)

  def testAppendPositiveToFilter(self):
    expected = 'positive1:positive2-negative1'
    actual = test_filter.AppendPatternsToFilter('positive1-negative1',
                                                ['positive2'])
    self.assertEqual(actual, expected)

  def testAppendNegativeToFilter(self):
    expected = 'positive1-negative1:negative2'
    actual = test_filter.AppendPatternsToFilter('positive1-negative1',
                                                negative_patterns=['negative2'])
    self.assertEqual(actual, expected)

  def testAppendBothToFilter(self):
    expected = 'positive1:positive2-negative1:negative2'
    actual = test_filter.AppendPatternsToFilter('positive1-negative1',
                                                positive_patterns=['positive2'],
                                                negative_patterns=['negative2'])
    self.assertEqual(actual, expected)

  def testAppendMultipleToFilter(self):
    expected = 'positive1:positive2:positive3-negative1:negative2:negative3'
    actual = test_filter.AppendPatternsToFilter('positive1-negative1',
                                                ['positive2', 'positive3'],
                                                ['negative2', 'negative3'])
    self.assertEqual(actual, expected)

  def testRepeatedAppendToFilter(self):
    expected = 'positive1:positive2:positive3-negative1:negative2:negative3'
    filter_string = test_filter.AppendPatternsToFilter('positive1-negative1',
                                                       ['positive2'],
                                                       ['negative2'])
    actual = test_filter.AppendPatternsToFilter(filter_string, ['positive3'],
                                                ['negative3'])
    self.assertEqual(actual, expected)

  def testAppendHashSeparatedPatternsToFilter(self):
    expected = 'positive.test1:positive.test2-negative.test1:negative.test2'
    actual = test_filter.AppendPatternsToFilter('positive#test1-negative#test1',
                                                       ['positive#test2'],
                                                       ['negative#test2'])
    self.assertEqual(actual, expected)


class HasPositivePatterns(unittest.TestCase):
  def testEmpty(self):
    expected = False
    actual = test_filter.HasPositivePatterns('')
    self.assertEqual(actual, expected)

  def testHasOnlyPositive(self):
    expected = True
    actual = test_filter.HasPositivePatterns('positive')
    self.assertEqual(actual, expected)

  def testHasOnlyNegative(self):
    expected = False
    actual = test_filter.HasPositivePatterns('-negative')
    self.assertEqual(actual, expected)

  def testHasBoth(self):
    expected = True
    actual = test_filter.HasPositivePatterns('positive-negative')
    self.assertEqual(actual, expected)


if __name__ == '__main__':
  sys.exit(unittest.main())
