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


if __name__ == '__main__':
    unittest.main()
