#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from typing import List

from python.generators.diff_tests import testing
from chrome.tests import ChromeStdlib
from chrome.tests_scroll_jank import ChromeScrollJankStdlib
from chrome.tests_chrome_interactions import ChromeInteractions

def fetch_all_diff_tests(index_path: str) -> List['testing.TestCase']:
  return [
      *ChromeScrollJankStdlib(index_path, 'chrome', 'ChromeScrollJank').fetch(),
      *ChromeStdlib(index_path, 'chrome', 'Chrome').fetch(),
      *ChromeInteractions(index_path, 'chrome', 'ChromeInteractions').fetch(),
      ]
