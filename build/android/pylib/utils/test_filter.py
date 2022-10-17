# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re


_CMDLINE_NAME_SEGMENT_RE = re.compile(
    r' with(?:out)? \{[^\}]*\}')


def ParseFilterFile(input_lines):
  """Converts test filter file contents to positive and negative pattern lists.

  See //testing/buildbot/filters/README.md for description of the
  syntax that |input_lines| are expected to follow.

  See
  https://github.com/google/googletest/blob/main/docs/advanced.md#running-a-subset-of-the-tests
  for description of the syntax that --gtest_filter argument should follow.

  Args:
    input_lines: An iterable (e.g. a list or a file) containing input lines.
  Returns:
    tuple containing the lists of positive patterns and negative patterns
  """
  # Strip comments and whitespace from each line and filter non-empty lines.
  stripped_lines = (l.split('#', 1)[0].strip() for l in input_lines)
  filter_lines = [l for l in stripped_lines if l]

  # Split the tests into positive and negative patterns (gtest treats
  # every pattern after the first '-' sign as an exclusion).
  positive_patterns = [l for l in filter_lines if l[0] != '-']
  negative_patterns = [l[1:] for l in filter_lines if l[0] == '-']
  return positive_patterns, negative_patterns


def AddFilterOptions(parser):
  """Adds filter command-line options to the provided parser.

  Args:
    parser: an argparse.ArgumentParser instance.
  """
  parser.add_argument(
      # Deprecated argument.
      '--gtest-filter-file',
      # New argument.
      '--test-launcher-filter-file',
      action='append',
      dest='test_filter_files',
      help='Path to file that contains googletest-style filter strings. '
      'See also //testing/buildbot/filters/README.md.')

  filter_group = parser.add_mutually_exclusive_group()
  filter_group.add_argument('-f',
                            '--test-filter',
                            '--gtest_filter',
                            '--gtest-filter',
                            dest='test_filters',
                            action='append',
                            help='googletest-style filter string.',
                            default=os.environ.get('GTEST_FILTER'))
  filter_group.add_argument(
      '--isolated-script-test-filter',
      action='append',
      dest='isolated_script_test_filters',
      help='isolated script filter string. '
      'Like gtest filter strings, but with :: separators instead of :')


def AppendPatternsToFilter(test_filter, positive_patterns=None,
                           negative_patterns=None):
  """Returns a test-filter string with additional patterns.

  Args:
    test_filter: test filter string
    positive_patterns: list of positive patterns to add to string
    negative_patterns: list of negative patterns to add to string
  """
  positives = []
  negatives = []
  positive = ''
  negative = ''

  split_filter = test_filter.split('-', 1)
  if len(split_filter) == 1:
    positive = split_filter[0]
  else:
    positive, negative = split_filter

  positives += [f for f in positive.split(':') if f]
  negatives += [f for f in negative.split(':') if f]

  positives += positive_patterns if positive_patterns else []
  negatives += negative_patterns if negative_patterns else []

  final_filter = ':'.join([p.replace('#', '.') for p in positives])
  if negatives:
    final_filter += '-' + ':'.join([n.replace('#', '.') for n in negatives])
  return final_filter


def HasPositivePatterns(test_filter):
  """Returns True if test_filter contains a positive pattern, else False

  Args:
    test_filter: test-filter style string
  """
  return bool(len(test_filter) > 0 and test_filter[0] != '-')


def InitializeFiltersFromArgs(args):
  """Returns a filter string from the command-line option values.

  Args:
    args: an argparse.Namespace instance resulting from a using parser
      to which the filter options above were added.
  """
  test_filters = []
  if args.isolated_script_test_filters:
    args.test_filters = [
        isolated_script_test_filter.replace('::', ':')
        for isolated_script_test_filter in args.isolated_script_test_filters
    ]
  if args.test_filters:
    for filt in args.test_filters:
      test_filters.append(
          _CMDLINE_NAME_SEGMENT_RE.sub('', filt.replace('#', '.')))

  if not args.test_filter_files:
    return test_filters

  # At this point it's potentially several files, in a list and ; separated
  for test_filter_files in args.test_filter_files:
    # At this point it's potentially several files, ; separated
    for test_filter_file in test_filter_files.split(';'):
      # At this point it's individual files
      with open(test_filter_file, 'r') as f:
        positive_patterns, negative_patterns = ParseFilterFile(f)
        filter_string = AppendPatternsToFilter('', positive_patterns,
                                               negative_patterns)
        test_filters.append(filter_string)

  return test_filters
