#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import PRESUBMIT

# Append chrome source root to import `PRESUBMIT_test_mocks.py`.
sys.path.append(
    os.path.dirname(
        os.path.dirname(
            os.path.dirname(os.path.dirname(os.path.abspath(__file__))))))
from PRESUBMIT_test_mocks import MockAffectedFile, MockInputApi, MockOutputApi

_PARTITION_ALLOC_BASE_PATH = 'base/allocator/partition_allocator/src/'


class PartitionAllocIncludeGuardsTest(unittest.TestCase):

    def _CheckForIncludeGuardsWithMock(self, filename, lines):
        mock_input_api = MockInputApi()
        mock_input_api.files = [MockAffectedFile(filename, lines)]
        mock_output_api = MockOutputApi()
        return PRESUBMIT.CheckForIncludeGuards(mock_input_api, mock_output_api)

    def testExpectedGuardNameDoesNotError(self):
        lines = [
            '#ifndef PARTITION_ALLOC_RANDOM_H_',
            '#define PARTITION_ALLOC_RANDOM_H_',
            '#endif  // PARTITION_ALLOC_RANDOM_H_'
        ]
        errors = self._CheckForIncludeGuardsWithMock(
            _PARTITION_ALLOC_BASE_PATH + 'partition_alloc/random.h', lines)
        self.assertEqual(0, len(errors))

    def testMissingGuardErrors(self):
        lines = []
        errors = self._CheckForIncludeGuardsWithMock(
            _PARTITION_ALLOC_BASE_PATH + 'partition_alloc/random.h', lines)
        self.assertEqual(1, len(errors))
        self.assertIn('Missing include guard', errors[0].message)
        self.assertIn('Recommended name: PARTITION_ALLOC_RANDOM_H_',
                      errors[0].message)

    def testMissingGuardInNonHeaderFileDoesNotError(self):
        lines = []
        errors = self._CheckForIncludeGuardsWithMock(
            _PARTITION_ALLOC_BASE_PATH + 'partition_alloc/random.cc', lines)
        self.assertEqual(0, len(errors))

    def testGuardNotCoveringWholeFileErrors(self):
        lines = [
            '#ifndef PARTITION_ALLOC_RANDOM_H_',
            '#define PARTITION_ALLOC_RANDOM_H_',
            '#endif  // PARTITION_ALLOC_RANDOM_H_',
            'int oh_i_forgot_to_guard_this;'
        ]
        errors = self._CheckForIncludeGuardsWithMock(
            _PARTITION_ALLOC_BASE_PATH + 'partition_alloc/random.h', lines)
        self.assertEqual(1, len(errors))
        self.assertIn('not covering the whole file', errors[0].message)

    def testMissingDefineInGuardErrors(self):
        lines = [
            '#ifndef PARTITION_ALLOC_RANDOM_H_',
            'int somehow_put_here;'
            '#define PARTITION_ALLOC_RANDOM_H_',
            '#endif  // PARTITION_ALLOC_RANDOM_H_',
        ]
        errors = self._CheckForIncludeGuardsWithMock(
            _PARTITION_ALLOC_BASE_PATH + 'partition_alloc/random.h', lines)
        self.assertEqual(1, len(errors))
        self.assertIn(
            'Missing "#define PARTITION_ALLOC_RANDOM_H_" for include guard',
            errors[0].message)


class PartitionAllocUnexpectedPreprocessorDefinesTest(unittest.TestCase):

    def _CheckUnexpectedPreprocessorDefinesWithMock(self,
                                                    filename,
                                                    new_contents,
                                                    changed_contents=None):
        mock_input_api = MockInputApi()
        mock_file = MockAffectedFile(filename, new_contents)
        if changed_contents is not None:
            mock_file._changed_contents = changed_contents
        mock_input_api.files = [mock_file]
        mock_output_api = MockOutputApi()
        return PRESUBMIT.CheckUnexpectedPreprocessorDefines(
            mock_input_api, mock_output_api)

    def testAllowedMacros(self):
        lines = ['#if defined(__clang__)']
        errors = self._CheckUnexpectedPreprocessorDefinesWithMock(
            _PARTITION_ALLOC_BASE_PATH + 'partition_alloc/random.cc', lines)
        self.assertEqual(0, len(errors))

    def testHeaderGuardsIgnored(self):
        lines = ['#if !defined(PARTITION_ALLOC_RANDOM_H_)']
        errors = self._CheckUnexpectedPreprocessorDefinesWithMock(
            _PARTITION_ALLOC_BASE_PATH + 'partition_alloc/random.h', lines)
        self.assertEqual(0, len(errors))

        lines = ['#if !defined(RANDOM_H_)']

        errors = self._CheckUnexpectedPreprocessorDefinesWithMock(
            _PARTITION_ALLOC_BASE_PATH + 'partition_alloc/random.h', lines)
        self.assertEqual(0, len(errors))

    def testLocallyDefinedMacrosIgnored(self):
        lines = [
            '#define PA_MY_LOCAL_MACRO 1',
            '#if defined(PA_MY_LOCAL_MACRO)'
        ]
        errors = self._CheckUnexpectedPreprocessorDefinesWithMock(
            _PARTITION_ALLOC_BASE_PATH + 'partition_alloc/random.cc', lines)
        self.assertEqual(0, len(errors))

    def testUnexpectedMacrosError(self):
        lines = ['#if defined(MY_CRAZY_UNEXPECTED_MACRO)']
        errors = self._CheckUnexpectedPreprocessorDefinesWithMock(
            _PARTITION_ALLOC_BASE_PATH + 'partition_alloc/random.cc', lines)
        self.assertEqual(1, len(errors))
        self.assertEqual('error', errors[0].type)
        self.assertIn('Unexpected macro `MY_CRAZY_UNEXPECTED_MACRO`',
                      errors[0].message)

    def testUnexpectedMacrosErrorInObjectiveC(self):
        lines = ['#if defined(MY_CRAZY_UNEXPECTED_MACRO)']
        errors = self._CheckUnexpectedPreprocessorDefinesWithMock(
            _PARTITION_ALLOC_BASE_PATH + 'partition_alloc/random.mm', lines)
        self.assertEqual(1, len(errors))
        self.assertEqual('error', errors[0].type)
        self.assertIn('Unexpected macro `MY_CRAZY_UNEXPECTED_MACRO`',
                      errors[0].message)

    def testNonRelevantFilesIgnored(self):
        lines = ['#if defined(MY_CRAZY_UNEXPECTED_MACRO)']
        errors = self._CheckUnexpectedPreprocessorDefinesWithMock(
            'base/not_partition_alloc/random.cc', lines)
        self.assertEqual(0, len(errors))

    def testBuildConfigIgnored(self):
        lines = ['#if defined(MY_CRAZY_UNEXPECTED_MACRO)']
        errors = self._CheckUnexpectedPreprocessorDefinesWithMock(
            _PARTITION_ALLOC_BASE_PATH + 'partition_alloc/build_config.h',
            lines)
        self.assertEqual(0, len(errors))


if __name__ == '__main__':
    unittest.main()
