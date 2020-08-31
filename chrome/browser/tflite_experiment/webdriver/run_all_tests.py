# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import os
import test_setup

sys.path.append(
    os.path.join(os.path.dirname(__file__, ), os.pardir, os.pardir, os.pardir,
                 os.pardir, 'tools', 'chrome_proxy', 'webdriver'))
import common
common.ParseFlags = test_setup.parse_flags

if __name__ == "__main__":
    results = test_setup.RunAllTests(run_all_tests=True)
    if results.errors or results.failures:
        sys.exit(2)
