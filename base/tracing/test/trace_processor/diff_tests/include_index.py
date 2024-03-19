#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
from typing import List

from python.generators.diff_tests import testing
from chrome.tests import ChromeStdlib
from chrome.tests_scroll_jank import ChromeScrollJankStdlib
from chrome.tests_chrome_interactions import ChromeInteractions

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
  return [
      *StdlibSmoke(index_path, 'stdlib', 'StdlibSmoke').fetch(),
      *ChromeScrollJankStdlib(index_path, 'chrome', 'ChromeScrollJankStdlib',
                              test_data_path).fetch(),
      *ChromeStdlib(index_path, 'chrome', 'ChromeStdlib',
                    test_data_path).fetch(),
      *ChromeInteractions(index_path, 'chrome', 'ChromeInteractions',
                          test_data_path).fetch(),
      ]
