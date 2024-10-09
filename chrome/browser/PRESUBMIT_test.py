#!/usr/bin/env python
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import sys
import unittest

import PRESUBMIT

file_dir_path = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(file_dir_path, '..', '..'))
from PRESUBMIT_test_mocks import MockAffectedFile
from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi

_VALID_DEP = "+third_party/blink/public/platform/web_something.h,"
_INVALID_DEP = "+third_party/blink/public/web/web_something.h,"
_INVALID_DEP2 = "+third_party/blink/public/web/web_nothing.h,"


class BlinkPublicWebUnwantedDependenciesTest(unittest.TestCase):
  def makeInputApi(self, files):
    input_api = MockInputApi()
    input_api.files = files
    # Override os_path.exists because the presubmit uses the actual
    # os.path.exists.
    input_api.CreateMockFileInPath(
        [x.LocalPath() for x in input_api.AffectedFiles(include_deletes=True)])
    return input_api

  INVALID_DEPS_MESSAGE = ('chrome/browser cannot depend on '
                          'blink/public/web interfaces. Use'
                          ' blink/public/common instead.')

  def testAdditionOfUnwantedDependency(self):
    input_api = self.makeInputApi([
        MockAffectedFile('DEPS', [_INVALID_DEP], [], action='M')])
    warnings = PRESUBMIT._CheckUnwantedDependencies(input_api, MockOutputApi())
    self.assertEqual(1, len(warnings))
    self.assertEqual(self.INVALID_DEPS_MESSAGE, warnings[0].message)
    self.assertEqual(1, len(warnings[0].items))

  def testAdditionOfUnwantedDependencyInComment(self):
    input_api = self.makeInputApi([
        MockAffectedFile('DEPS', ["#" + _INVALID_DEP], [], action='M')])
    warnings = PRESUBMIT._CheckUnwantedDependencies(input_api, MockOutputApi())
    self.assertEqual([], warnings)

  def testAdditionOfValidDependency(self):
    input_api = self.makeInputApi([
        MockAffectedFile('DEPS', [_VALID_DEP], [], action='M')])
    warnings = PRESUBMIT._CheckUnwantedDependencies(input_api, MockOutputApi())
    self.assertEqual([], warnings)

  def testAdditionOfMultipleUnwantedDependency(self):
    input_api = self.makeInputApi([
        MockAffectedFile('DEPS', [_INVALID_DEP, _INVALID_DEP2], action='M')])
    warnings = PRESUBMIT._CheckUnwantedDependencies(input_api, MockOutputApi())
    self.assertEqual(1, len(warnings))
    self.assertEqual(self.INVALID_DEPS_MESSAGE, warnings[0].message)
    self.assertEqual(2, len(warnings[0].items))

    input_api = self.makeInputApi([
        MockAffectedFile('DEPS', [_INVALID_DEP, _VALID_DEP], [], action='M')])
    warnings = PRESUBMIT._CheckUnwantedDependencies(input_api, MockOutputApi())
    self.assertEqual(1, len(warnings))
    self.assertEqual(self.INVALID_DEPS_MESSAGE, warnings[0].message)
    self.assertEqual(1, len(warnings[0].items))

  def testRemovalOfUnwantedDependency(self):
    input_api = self.makeInputApi([
        MockAffectedFile('DEPS', [], [_INVALID_DEP], action='M')])
    warnings = PRESUBMIT._CheckUnwantedDependencies(input_api, MockOutputApi())
    self.assertEqual([], warnings)

  def testRemovalOfValidDependency(self):
    input_api = self.makeInputApi([
        MockAffectedFile('DEPS', [], [_VALID_DEP], action='M')])
    warnings = PRESUBMIT._CheckUnwantedDependencies(input_api, MockOutputApi())
    self.assertEqual([], warnings)


class InteractiveUiTestLibIncludeTest(unittest.TestCase):
  def testAdditionOfUnwantedDependency(self):
    lines = ['#include "ui/base/test/ui_controls.h"',
             '#include "ui/base/test/foo.h"',
             '#include "chrome/test/base/interactive_test_utils.h"']
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('foo_interactive_uitest.cc', lines),
      MockAffectedFile('foo_browsertest.cc', lines),
      MockAffectedFile('foo_interactive_browsertest.cc', lines),
      MockAffectedFile('foo_unittest.cc', lines) ]
    mock_output_api = MockOutputApi()
    errors = PRESUBMIT._CheckNoInteractiveUiTestLibInNonInteractiveUiTest(
        mock_input_api, mock_output_api)
    self.assertEqual(1, len(errors))
    # 2 lines from 2 files.
    self.assertEqual(4, len(errors[0].items))


class CheckBuildFilesForIndirectAshSourcesTest(unittest.TestCase):
  MESSAGE = "Indirect sources detected."

  def testScope(self):
    """We only complain for changes to BUILD.gn under certain directories."""

    new_contents = [
        'source_set("foo") {',
        '  sources = [ "a/b.cc" ]',
        '}',
    ]

    mock_output_api = MockOutputApi()
    mock_input_api = MockInputApi()
    mock_input_api.files = [
        MockAffectedFile('BUILD.gn', new_contents),
        MockAffectedFile('chrome/browser/BUILD.gn', new_contents),
        MockAffectedFile('chrome/browser/ash/build.cc', new_contents),
        MockAffectedFile('chrome/browser/ash/BUILD.gn', new_contents),
        MockAffectedFile('chrome/browser/ashley/BUILD.gn', new_contents),
        MockAffectedFile('chrome/browser/chromeos/a/b/BUILD.gn', new_contents),
        MockAffectedFile('chrome/browser/resources/ash/BUILD.gn', new_contents),
        MockAffectedFile('chrome/browser/ui/BUILD.gn', new_contents),
        MockAffectedFile('chrome/browser/ui/ash/foo/BUILD.gn', new_contents),
        MockAffectedFile('chrome/browser/ui/chromeos/BUILD.gn', new_contents),
        MockAffectedFile('chrome/browser/ui/webui/ash/BUILD.gn', new_contents),
    ]

    results = PRESUBMIT._CheckBuildFilesForIndirectAshSources(mock_input_api,
                                                              mock_output_api)

    for result in results:
      self.assertEqual(result.message, self.MESSAGE)

    self.assertCountEqual(
        [r.items for r in results],
        [["chrome/browser/ash/BUILD.gn"],
         ["chrome/browser/chromeos/a/b/BUILD.gn"],
         ["chrome/browser/ui/ash/foo/BUILD.gn"],
         ["chrome/browser/ui/chromeos/BUILD.gn"],
         ["chrome/browser/ui/webui/ash/BUILD.gn"]])

  def testComplexFormatting(self):
    new_contents = [
        'source_set("foo") {',
        '  sources = [ "../0", "a/1",]',
        '\tsources += ["a/2" ]',
        'sources += [ # bla',
        '   "a/3",',
        '  ]',
        '   # sources = ["a/b"]',
        'sources += # bla',
        '    ["a/4"]#bla',
        '}',
        'static_library("bar"){',
        ' deps = []',
        ' sources = []',
        ' if (something) {',
        '   sources += [',
        '',
        '     "a/5", "ab", "a/6","a/7",# "a/b"',
        '     "a/8"]',
        '   sources',
        '     += [ "a/9" ]}',
        '}',
    ]

    mock_output_api = MockOutputApi()
    mock_input_api = MockInputApi()
    mock_input_api.files = [
        MockAffectedFile('chrome/browser/ash/BUILD.gn', new_contents),
    ]

    results = PRESUBMIT._CheckBuildFilesForIndirectAshSources(mock_input_api,
                                                              mock_output_api)

    self.assertEqual(len(results), 1)
    self.assertEqual(results[0].message, self.MESSAGE)
    self.assertEqual(results[0].items, ["chrome/browser/ash/BUILD.gn"])
    self.assertEqual(
        [s.lstrip() for s in results[0].long_text.splitlines()[1:]],
        ['../0', 'a/1', 'a/2', 'a/3', 'a/4', 'a/5', 'a/6', 'a/7', 'a/8', 'a/9'])

  def testModifications(self):
    old_contents = [
        'source_set("foo") {',
        '  sources = ["x/y", "a/b"]',
        '}',
    ]
    new_contents_good = [
        'source_set("foo") {',
        '  sources = ["x/y", "ab"]',
        '}',
    ]
    new_contents_bad = [
        'source_set("foo") {',
        '  sources = ["x/y", "a/b", "a/c"]',
        '}',
    ]

    mock_output_api = MockOutputApi()
    mock_input_api = MockInputApi()
    mock_input_api.files = [
        MockAffectedFile('chrome/browser/ash/BUILD.gn',
                         new_contents_bad, old_contents),
        MockAffectedFile('chrome/browser/chromeos/BUILD.gn',
                         new_contents_good, old_contents),
    ]

    results = PRESUBMIT._CheckBuildFilesForIndirectAshSources(mock_input_api,
                                                              mock_output_api)

    self.assertEqual(len(results), 1)
    self.assertEqual(results[0].message, self.MESSAGE)
    self.assertEqual(results[0].items, ["chrome/browser/ash/BUILD.gn"])
    self.assertEqual(
        [s.lstrip() for s in results[0].long_text.splitlines()[1:]],
        ['a/c'])


if __name__ == '__main__':
  unittest.main()
