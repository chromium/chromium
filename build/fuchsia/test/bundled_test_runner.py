# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Provides a way to run multiple tests as a bundle. It forwards all the
   arguments to the run_test.py, but overrides the test runner it uses.

   Since this class runs in a higher level as the regular run_test.py, it would
   be more clear to not include it into the run_test.py and avoid
   cycle-dependency.

   Use of this test runner may break sharding."""

import argparse
import logging
import os
import sys

from subprocess import CompletedProcess
from typing import List, NamedTuple, Optional
from urllib.parse import urlparse

import run_test
from run_executable_test import ExecutableTestRunner
from test_runner import TestRunner


class TestCase(NamedTuple):
    """Defines a TestCase, it executes the package with optional arguments."""

    # The test package in the format of fuchsia-pkg://...#meta/...cm.
    package: str

    # Optional arguments to pass to the test run. It can include multiple
    # arguments separated by any whitespaces.
    args: str = ''


class _BundledTestRunner(TestRunner):
    """A TestRunner implementation to run multiple test cases. It always run all
       tests even some of them failed. The return code is the return code of the
       last non-zero test run."""

    # private, use run_tests.get_test_runner function instead.
    # Keep the order of parameters consistent with ExecutableTestRunner.
    # pylint: disable=too-many-arguments
    # TODO(crbug.com/346806329): May consider using a structure to capture the
    # arguments.
    def __init__(self, out_dir: str, tests: List[TestCase],
                 target_id: Optional[str], code_coverage_dir: Optional[str],
                 logs_dir: Optional[str], package_deps: List[str],
                 test_realm: Optional[str]):
        super().__init__(
            out_dir, [], [], target_id,
            _BundledTestRunner._merge_packages(tests, package_deps))
        assert tests
        self._tests = tests
        self._code_coverage_dir = code_coverage_dir
        self._logs_dir = logs_dir
        self._test_realm = test_realm

    @staticmethod
    def _merge_packages(tests: List[TestCase],
                        package_deps: List[str]) -> List[str]:
        packages = list(package_deps)
        # Include test packages if they have not been defined in the
        # package_deps.
        packages.extend(
            {urlparse(x.package).path.lstrip('/') + '.far'
             for x in tests} - {os.path.basename(x)
                                for x in packages})
        return packages

    def run_test(self) -> CompletedProcess:
        returncode = 0
        for test in self._tests:
            assert test.package.endswith('.cm')
            test_runner = ExecutableTestRunner(
                self._out_dir, test.args.split(), test.package,
                self._target_id, self._code_coverage_dir, self._logs_dir,
                self._package_deps, self._test_realm)
            # It's a little bit wasteful to resolve all the packages once per
            # test package, but it's easier.
            result = test_runner.run_test().returncode
            logging.info('Result of test %s is %s', test, result)
            if result != 0:
                returncode = result
        return CompletedProcess(args='', returncode=returncode)


def run_tests(tests: List[TestCase]) -> int:
    """Runs multiple tests.

       Args:
         tests: The definition of each test case.

       Note:
         All the packages in tests will always be included, and it's expected
         that the far files sharing the same name as the package in TestCase
         except for the suffix. E.g. test1.far is the far file of
         fuchsia-pkg://fuchsia.com/test1#meta/some.cm.

         Duplicated packages in either --packages or tests are allowed as long
         as they are targeting the same file; otherwise the test run would
         trigger an assertion failure.

         Far files in the --packages can be either absolute paths or relative
         paths starting from --out-dir."""
    # The 'bundled-tests' is a place holder and has no specific meaning; the
    # run_test._get_test_runner is overridden.
    sys.argv.append('bundled-tests')

    def get_test_runner(runner_args: argparse.Namespace, *_) -> TestRunner:
        # test_args are not used, and each TestCase should have its own args.
        return _BundledTestRunner(runner_args.out_dir, tests,
                                  runner_args.target_id,
                                  runner_args.code_coverage_dir,
                                  runner_args.logs_dir, runner_args.packages,
                                  runner_args.test_realm)

    # pylint: disable=protected-access
    run_test._get_test_runner = get_test_runner
    return run_test.main()
