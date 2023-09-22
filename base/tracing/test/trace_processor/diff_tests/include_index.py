# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from typing import List

from python.generators.diff_tests import testing
from chrome.tests import Chrome
from chrome.tests_scroll_jank import ChromeScrollJank
from chrome.tests_args import ChromeArgs
from chrome.tests_memory_snapshots import ChromeMemorySnapshots
from chrome.tests_processes import ChromeProcesses
from chrome.tests_rail_modes import ChromeRailModes
from chrome.tests_touch_gesture import ChromeTouchGesture

def fetch_all_diff_tests(index_path: str) -> List['testing.TestCase']:
  return [
      *ChromeScrollJank(index_path, 'chrome', 'ChromeScrollJank').fetch(),
      *Chrome(index_path, 'chrome', 'Chrome').fetch(),
      *ChromeArgs(index_path, 'chrome', 'ChromeArgs').fetch(),
      *ChromeMemorySnapshots(index_path, 'chrome', 'ChromeMemorySnapshots')
              .fetch(),
      *ChromeProcesses(index_path, 'chrome', 'ChromeProcesses').fetch(),
      *ChromeRailModes(index_path, 'chrome', 'ChromeRailModes').fetch(),
      *ChromeTouchGesture(index_path, 'chrome', 'ChromeTouchGesture').fetch(),
      ]
