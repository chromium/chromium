#!/usr/bin/env vpython3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests of BundledTestRunner."""

import unittest

from bundled_test_runner import _BundledTestRunner, TestCase

# Test names should be self-explained, no point of adding function docstring.
# pylint: disable=missing-function-docstring, protected-access


class BundledTestRunnerTests(unittest.TestCase):
    """Test class."""

    @staticmethod
    def _default_test_case() -> TestCase:
        return TestCase(
            package='fuchsia-pkg://fuchsia.com/dart_runner_tests#meta/' +
            'dart_runner_tests.cm')

    def test_resolve_test_package_into_far(self):
        runner = _BundledTestRunner(
            'out/put', [BundledTestRunnerTests._default_test_case()],
            'target-id', None, '/tmp/log', [], None)
        self.assertEqual(
            runner._package_deps,
            {'dart_runner_tests': 'out/put/dart_runner_tests.far'})

    def test_dedupe_fars(self):
        runner = _BundledTestRunner(
            'out/put', [BundledTestRunnerTests._default_test_case()],
            'target-id', None, '/tmp/log', ['dart_runner_tests.far'], None)
        self.assertEqual(
            runner._package_deps,
            {'dart_runner_tests': 'out/put/dart_runner_tests.far'})

    def test_absolute_path(self):
        runner = _BundledTestRunner(
            'out/put', [BundledTestRunnerTests._default_test_case()],
            'target-id', None, '/tmp/log',
            ['/tmp/packages/dart_runner_tests.far'], None)
        self.assertEqual(
            runner._package_deps,
            {'dart_runner_tests': '/tmp/packages/dart_runner_tests.far'})

    def test_different_cm(self):
        runner = _BundledTestRunner('out/put', [
            TestCase(
                package=
                'fuchsia-pkg://fuchsia.com/dart_runner_tests#meta/meta.cm')
        ], 'target-id', None, '/tmp/log', [], None)
        self.assertEqual(
            runner._package_deps,
            {'dart_runner_tests': 'out/put/dart_runner_tests.far'})

    def test_join_relative_path(self):
        runner = _BundledTestRunner(
            'out/default', [BundledTestRunnerTests._default_test_case()],
            'target-id', None, '/tmp/log', ['gen/dart_runner_tests.far'], None)
        self.assertEqual(
            runner._package_deps,
            {'dart_runner_tests': 'out/default/gen/dart_runner_tests.far'})

    def test_current_dir_as_out_dir(self):
        runner = _BundledTestRunner(
            '.', [BundledTestRunnerTests._default_test_case()], 'target-id',
            None, '/tmp/log', ['out/default/bin/dart_runner_tests.far'], None)
        self.assertEqual(
            runner._package_deps,
            {'dart_runner_tests': './out/default/bin/dart_runner_tests.far'})

    def test_empty_out_dir(self):
        runner = _BundledTestRunner(
            '', [BundledTestRunnerTests._default_test_case()], 'target-id',
            None, '/tmp/log', ['out/default/bin/dart_runner_tests.far'], None)
        self.assertEqual(
            runner._package_deps,
            {'dart_runner_tests': 'out/default/bin/dart_runner_tests.far'})

    def test_none_out_dir(self):
        with self.assertRaises(TypeError):
            _BundledTestRunner(None,
                               [BundledTestRunnerTests._default_test_case()],
                               'target-id', None, '/tmp/log',
                               ['out/default/bin/dart_runner_tests.far'], None)

    def test_conflict_fars(self):
        with self.assertRaises(AssertionError):
            _BundledTestRunner('',
                               [BundledTestRunnerTests._default_test_case()],
                               'target-id', None, '/tmp/log', [
                                   'out/default/bin/dart_runner_tests.far',
                                   'not/out/default/bin/dart_runner_tests.far'
                               ], None)


if __name__ == '__main__':
    unittest.main()
