#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
from typing import List

from python.generators.diff_tests import testing
from chrome.chrome_stdlib_testsuites import CHROME_STDLIB_TESTSUITES

# Import diff tests from third_party/perfetto/test/trace_processor
PERFETTO_DIFF_TEST_DIR = os.path.abspath(os.path.join(os.path.dirname(
                    __file__),
                    '..', '..', '..', '..', '..',
                    'third_party', 'perfetto',
                    'test', 'trace_processor'))
sys.path.append(PERFETTO_DIFF_TEST_DIR)
from diff_tests.stdlib.tests import StdlibSmoke

def fetch_all_diff_tests(index_path: str) -> List['testing.TestCase']:
  test_data_path = os.path.abspath(os.path.join(__file__, '../../../data'))

  chrome_stdlib_tests = []
  for test_suite_cls in CHROME_STDLIB_TESTSUITES:
    test_suite = test_suite_cls(index_path,
                                'chrome',
                                test_suite_cls.__name__, test_data_path)
    chrome_stdlib_tests += test_suite.fetch()

  return [
      *StdlibSmoke(index_path, 'stdlib', 'StdlibSmoke').fetch(),
      ] + chrome_stdlib_tests
