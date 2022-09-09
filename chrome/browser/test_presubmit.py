#!/usr/bin/env python
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit test runner for Web Development Style Guide checks."""

from web_dev_style import css_checker_test, \
                          html_checker_test, \
                          js_checker_test, \
                          resource_checker_test

_TEST_MODULES = [
    css_checker_test,
    html_checker_test,
    js_checker_test,
    resource_checker_test
]

for test_module in _TEST_MODULES:
  test_module.unittest.main(test_module)
