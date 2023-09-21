# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from typing import List

from python.generators.diff_tests import testing
from chrome.tests import Chrome
from chrome.tests_scroll_jank import ChromeScrollJank

def fetch_all_diff_tests(index_path: str) -> List['testing.TestCase']:
  return [
      *ChromeScrollJank(index_path, 'chrome', 'ChromeScrollJank').fetch(),
      *Chrome(index_path, 'chrome', 'Chrome').fetch(),
      ]
