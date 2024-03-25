# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
A list of Chrome Stdlib TestSuites to be used by the DiffTestRunner. We
define this here so we can use the same list in both the Chromium and
Perfetto repositories.
"""

from .tests import ChromeStdlib
from .tests_scroll_jank import ChromeScrollJankStdlib
from .tests_chrome_interactions import ChromeInteractions

CHROME_STDLIB_TESTSUITES = [
    ChromeScrollJankStdlib,
    ChromeStdlib,
    ChromeInteractions,
]