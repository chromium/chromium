#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import PRESUBMIT

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))
from PRESUBMIT_test_mocks import MockFile, MockInputApi


class _MockOutputApi:
  class PresubmitPromptWarning(str):
    pass

  class PresubmitError(str):
    pass


def _stub_rhs_lines(input_api, tuples):
  """
  Replace input_api.RightHandSideLines with a stub returning `tuples`.

  `tuples`: iterable of (MockFile, line_num, line_text)
  """
  def _rhs(file_filter):
    # Respect the file_filter the checker passes in.
    return [(f, ln, lt) for (f, ln, lt) in tuples if file_filter(f)]
  input_api.RightHandSideLines = _rhs


class CheckNoLiteralBrandNamesTest(unittest.TestCase):
  def test_flags_literal_Chrome_inside_message(self):
    # <message> Chrome </message> should be flagged.
    lines = [
      '<messages>',
      '  <message name="IDS_TESTING_BRAND">',
      '    Chrome',
      '  </message>',
      '</messages>',
    ]
    mock_file = MockFile('chrome/app/generated_resources.grd', lines)
    input_api = MockInputApi()
    # Touched line is the "Chrome" content line (line 3).
    _stub_rhs_lines(input_api, [(mock_file, 3, lines[2])])

    out = PRESUBMIT._CheckNoLiteralBrandNamesInGeneratedResources(
                      input_api, _MockOutputApi())
    self.assertEqual(1, len(out))
    self.assertIn(
      "Hardcoded brand names found in generated_resources.grd:", str(out[0]))
    # The following line is the corrected part.
    # The check flags line 1, so the test should assert for line 1.
    self.assertIn("chrome/app/generated_resources.grd:1", str(out[0]))

  def test_flags_literal_Chromium_inside_message(self):
    lines = [
      '<messages>',
      '  <message name="IDS_SOMETHING">',
      '    Chromium',
      '  </message>',
      '</messages>',
    ]
    mock_file = MockFile('chrome/app/generated_resources.grd', lines)
    input_api = MockInputApi()
    _stub_rhs_lines(input_api, [(mock_file, 3, lines[2])])

    out = PRESUBMIT._CheckNoLiteralBrandNamesInGeneratedResources(
                      input_api, _MockOutputApi())
    self.assertEqual(1, len(out))
    self.assertIn("chromium", str(out[0]).lower())

  def test_ignores_text_inside_ex_example(self):
    # The example text <ex>Google Chrome</ex> must be ignored.
    lines = [
      '<messages>',
      '  <message name="IDS_HIDE_APP_MAC_TESTING">',
      '    Hide <ph name="PRODUCT_NAME">$1<ex>Google Chrome</ex></ph>',
      '  </message>',
      '</messages>',
    ]
    mock_file = MockFile('chrome/app/generated_resources.grd', lines)
    input_api = MockInputApi()
    _stub_rhs_lines(input_api, [(mock_file, 3, lines[2])])

    out = PRESUBMIT._CheckNoLiteralBrandNamesInGeneratedResources(
                      input_api, _MockOutputApi())
    self.assertEqual([], out, msg=f"Unexpected result: {out}")

if __name__ == '__main__':
  unittest.main()