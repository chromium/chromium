#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for checkdeps.
"""

import os
import unittest


import builddeps
import checkdeps
import results


class CheckDepsTest(unittest.TestCase):

  def setUp(self):
    self.deps_checker = checkdeps.DepsChecker(
        being_tested=True,
        base_directory=os.path.join(os.path.dirname(__file__), '..', '..'))

  def ImplTestRegularCheckDepsRun(self, ignore_temp_rules, skip_tests):
    self.deps_checker._ignore_temp_rules = ignore_temp_rules
    self.deps_checker._skip_tests = skip_tests
    self.deps_checker.CheckDirectory(
        os.path.join(self.deps_checker.base_directory,
                     'buildtools/checkdeps/testdata'))

    problems = self.deps_checker.results_formatter.GetResults()

    def VerifySubstringsInProblems(key_path, substrings_in_sequence):
      """Finds the problem in |problems| that contains |key_path|,
      then verifies that each of |substrings_in_sequence| occurs in
      that problem, in the order they appear in
      |substrings_in_sequence|.
      """
      found = False
      key_path = os.path.normpath(key_path)
      for problem in problems:
        index = problem.find(key_path)
        if index != -1:
          if substrings_in_sequence is None:
            self.fail(
                f'Expected no problems for {key_path}, but found: {problem}')

          for substring in substrings_in_sequence:
            index = problem.find(substring, index + 1)
            self.assertTrue(index != -1, '%s in %s' % (substring, problem))
          found = True
          break
      if not found and substrings_in_sequence is not None:
        self.fail('Found no problem for file %s' % key_path)

    def VerifyNoProblems(key_path):
      VerifySubstringsInProblems(key_path, None)

    if ignore_temp_rules:
      VerifySubstringsInProblems('testdata/allowed/test.h',
                                 ['-buildtools/checkdeps/testdata/disallowed',
                                  'temporarily_allowed.h',
                                  '-third_party/explicitly_disallowed',
                                  'Because of no rule applying'])
    else:
      VerifySubstringsInProblems('testdata/allowed/test.h',
                                 ['-buildtools/checkdeps/testdata/disallowed',
                                  '-third_party/explicitly_disallowed',
                                  'Because of no rule applying'])

    VerifySubstringsInProblems('testdata/disallowed/test.h',
                               ['-third_party/explicitly_disallowed',
                                'Because of no rule applying',
                                'Because of no rule applying'])
    VerifySubstringsInProblems('disallowed/allowed/test.h',
                               ['-third_party/explicitly_disallowed',
                                'Because of no rule applying',
                                'Because of no rule applying'])
    VerifySubstringsInProblems('testdata/noparent/test.h',
                               ['allowed/bad.h',
                                'Because of no rule applying'])
    VerifySubstringsInProblems('testdata/requires_review_users/usage.cc', [
        'testdata/requires_review/sub/foo.h"',
        'requires_review/sub", which is marked'])
    VerifyNoProblems('requires_review_users/sub/includes_okay/usage.cc')
    VerifySubstringsInProblems(
      'requires_review_users/sub/includes_only_sub/usage.cc',
        [
          'testdata/requires_review/sub/foo.h"',
          'requires_review/sub", which is',
          'testdata/requires_review/sub/inherited/foo.h"',
          'testdata/requires_review/sub/sub/inherited/bar.h"',
          'requires_review/sub/sub", which is',
          ])

    if skip_tests:
      VerifyNoProblems('allowed/not_a_test.cc')
    else:
      VerifySubstringsInProblems('allowed/not_a_test.cc',
                                 ['-buildtools/checkdeps/testdata/disallowed'])

  def testRegularCheckDepsRun(self):
    self.ImplTestRegularCheckDepsRun(False, False)

  def testRegularCheckDepsRunIgnoringTempRules(self):
    self.ImplTestRegularCheckDepsRun(True, False)

  def testRegularCheckDepsRunSkipTests(self):
    self.ImplTestRegularCheckDepsRun(False, True)

  def testRegularCheckDepsRunIgnoringTempRulesSkipTests(self):
    self.ImplTestRegularCheckDepsRun(True, True)

  def CountViolations(self, ignore_temp_rules):
    self.deps_checker._ignore_temp_rules = ignore_temp_rules
    self.deps_checker.results_formatter = results.CountViolationsFormatter()
    self.deps_checker.CheckDirectory(
        os.path.join(self.deps_checker.base_directory,
                     'buildtools/checkdeps/testdata'))
    return self.deps_checker.results_formatter.GetResults()

  def testCountViolations(self):
    self.assertEqual('16', self.CountViolations(False))

  def testCountViolationsIgnoringTempRules(self):
    self.assertEqual('17', self.CountViolations(True))

  def testCountViolationsWithRelativePath(self):
    self.deps_checker.results_formatter = results.CountViolationsFormatter()
    self.deps_checker.CheckDirectory(
        os.path.join('buildtools', 'checkdeps', 'testdata', 'allowed'))
    self.assertEqual('5', self.deps_checker.results_formatter.GetResults())

  def testTempRulesGenerator(self):
    self.deps_checker.results_formatter = results.TemporaryRulesFormatter()
    self.deps_checker.CheckDirectory(
        os.path.join(self.deps_checker.base_directory,
                     'buildtools/checkdeps/testdata/allowed'))
    temp_rules = self.deps_checker.results_formatter.GetResults()
    expected = ['  "!/does_not_exist.h",',
                '  "!buildtools/checkdeps/testdata/disallowed/bad.h",',
                '  "!buildtools/checkdeps/testdata/disallowed/teststuff/bad.h",',
                '  "!third_party/explicitly_disallowed/bad.h",',
                '  "!third_party/no_rule/bad.h",']
    self.assertEqual(expected, temp_rules)

  @unittest.skipIf(os.getcwd().startswith('/google/cog/cloud'),
                  "Skip if not git")
  def testBadBaseDirectoryNotCheckoutRoot(self):
    # This assumes git. It's not a valid test if buildtools is fetched via svn.
    with self.assertRaises(builddeps.DepsBuilderError):
      checkdeps.DepsChecker(being_tested=True,
                            base_directory=os.path.dirname(__file__))

  def testCheckAddedIncludesAllGood(self):
    problems = self.deps_checker.CheckAddedCppIncludes(
      [['buildtools/checkdeps/testdata/allowed/test.cc',
        ['#include "buildtools/checkdeps/testdata/allowed/good.h"',
         '#include "buildtools/checkdeps/testdata/disallowed/allowed/good.h"']
      ]])
    self.assertFalse(problems)

  def testCheckAddedIncludesManyGarbageLines(self):
    garbage_lines = ["My name is Sam%d\n" % num for num in range(50)]
    problems = self.deps_checker.CheckAddedCppIncludes(
      [['buildtools/checkdeps/testdata/allowed/test.cc', garbage_lines]])
    self.assertFalse(problems)

  def testCheckAddedIncludesNoRule(self):
    problems = self.deps_checker.CheckAddedCppIncludes(
      [['buildtools/checkdeps/testdata/allowed/test.cc',
        ['#include "no_rule_for_this/nogood.h"']
      ]])
    self.assertTrue(problems)

  def testCheckAddedIncludesSkippedDirectory(self):
    problems = self.deps_checker.CheckAddedCppIncludes(
      [['buildtools/checkdeps/testdata/disallowed/allowed/skipped/test.cc',
        ['#include "whatever/whocares.h"']
      ]])
    self.assertFalse(problems)

  def testCheckAddedIncludesTempAllowed(self):
    problems = self.deps_checker.CheckAddedCppIncludes(
      [['buildtools/checkdeps/testdata/allowed/test.cc',
        ['#include "buildtools/checkdeps/testdata/disallowed/temporarily_allowed.h"']
      ]])
    self.assertTrue(problems)

  def testCopyIsDeep(self):
    # Regression test for a bug where we were making shallow copies of
    # Rules objects and therefore all Rules objects shared the same
    # dictionary for specific rules.
    #
    # The first pair should bring in a rule from testdata/allowed/DEPS
    # into that global dictionary that allows the
    # temp_allowed_for_tests.h file to be included in files ending
    # with _unittest.cc, and the second pair should completely fail
    # once the bug is fixed, but succeed (with a temporary allowance)
    # if the bug is in place.
    problems = self.deps_checker.CheckAddedCppIncludes(
      [['buildtools/checkdeps/testdata/allowed/test.cc',
        ['#include "buildtools/checkdeps/testdata/disallowed/temporarily_allowed.h"']
       ],
       ['buildtools/checkdeps/testdata/disallowed/foo_unittest.cc',
        ['#include "buildtools/checkdeps/testdata/bongo/temp_allowed_for_tests.h"']
       ]])
    # With the bug in place, there would be two problems reported, and
    # the second would be for foo_unittest.cc.
    self.assertTrue(len(problems) == 1)
    self.assertTrue(problems[0][0].endswith('/test.cc'))

  def testTraversalIsOrdered(self):
    dirs_traversed = []
    for rules, filenames in self.deps_checker.GetAllRulesAndFiles(dir_name='buildtools'):
      self.assertEqual(type(filenames), list)
      self.assertEqual(filenames, sorted(filenames))
      if filenames:
        dir_names = set(os.path.dirname(file) for file in filenames)
        self.assertEqual(1, len(dir_names))
        dirs_traversed.append(dir_names.pop())
    self.assertEqual(dirs_traversed, sorted(dirs_traversed))

  def testCheckPartialImportsAreAllowed(self):
    problems = self.deps_checker.CheckAddedProtoImports(
      [['buildtools/checkdeps/testdata/test.proto',
        ['import "no_rule_for_this/nogood.proto"']
      ]])
    self.assertFalse(problems)

  def testCheckAddedFullPathImportsAllowed(self):
    problems = self.deps_checker.CheckAddedProtoImports(
      [['buildtools/checkdeps/testdata/test.proto',
        ['import "buildtools/checkdeps/testdata/allowed/good.proto"',
         'import "buildtools/checkdeps/testdata/disallowed/sub_folder/good.proto"']
      ]])
    self.assertFalse(problems)

  def testCheckAddedFullPathImportsDisallowed(self):
    problems = self.deps_checker.CheckAddedProtoImports(
      [['buildtools/checkdeps/testdata/test.proto',
        ['import "buildtools/checkdeps/testdata/disallowed/bad.proto"']
      ]])
    self.assertTrue(problems)

  def testCheckAddedFullPathImportsManyGarbageLines(self):
    garbage_lines = ["My name is Sam%d\n" % num for num in range(50)]
    problems = self.deps_checker.CheckAddedProtoImports(
      [['buildtools/checkdeps/testdata/test.proto',
        garbage_lines]])
    self.assertFalse(problems)

  def testCheckAddedIncludesNoRuleFullPath(self):
    problems = self.deps_checker.CheckAddedProtoImports(
      [['buildtools/checkdeps/testdata/test.proto',
        ['import "tools/some.proto"']
      ]])
    self.assertTrue(problems)

  def testCheckResolveDotDot(self):
    # The CppChecker disallows unknown files by default, so if we don't
    # resolve properly this will be forbidden.
    problems = self.deps_checker.CheckAddedCppIncludes(
      [[os.path.join(self.deps_checker.base_directory, 'buildtools/checkdeps/testdata/allowed/test.cc'),
        ['#include "../../testdata/allowed/test.h"']]])
    self.assertFalse(problems)

    # The ProtoChecker allows unknown files by default, so if we don't
    # resolve properly this will be allowed.
    problems = self.deps_checker.CheckAddedProtoImports(
      [[os.path.join(self.deps_checker.base_directory, 'buildtools/checkdeps/testdata/test.proto'),
        ['import "../testdata/disallowed/test.proto"']]])
    self.assertTrue(problems)

if __name__ == '__main__':
  unittest.main()
