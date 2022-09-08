#!/usr/bin/env vpython3
#
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import generate_flag_labels as gen_labels


class _GenerateFlagLabelsTest(unittest.TestCase):
  """Unittests for the generate_flag_labels module.
  """

  def testGetSwitchId(self):
    # Arbitrarily, this test verifies the WebViewExtraHeadersSameOriginOnly
    # feature since we know from field metrics this is logged correctly.
    self.assertEqual(
        -1988840552,
        gen_labels.GetSwitchId('WebViewExtraHeadersSameOriginOnly:disabled'))

  def testFormatName_baseFeature(self):
    self.assertEqual('SomeFeature',
                     gen_labels.FormatName('FooFeatures.SOME_FEATURE', True))
    self.assertEqual(
        'SomeWebViewFeature',
        gen_labels.FormatName('FooFeatures.SOME_WEBVIEW_FEATURE', True))

  def testFormatName_commandLine(self):
    self.assertEqual('some-switch',
                     gen_labels.FormatName('FooSwitches.SOME_SWITCH', False))
    self.assertEqual(
        'some-webview-switch',
        gen_labels.FormatName('FooSwitches.SOME_WEBVIEW_SWITCH', False))

  def testExtractFlagsFromJavaLines(self):
    test_data = """
// Same line
Flag.commandLine(FooSwitches.SOME_SWITCH,
        "Some description"),

// Different line
Flag.commandLine(
        FooSwitches.SOME_OTHER_SWITCH,
        "Some other description"),

// Same line
Flag.baseFeature(FooFeatures.SOME_FEATURE,
        "Some description"),

// Different line
Flag.baseFeature(
        FooFeatures.SOME_OTHER_FEATURE,
        "Some other description"),
""".split('\n')
    flags = gen_labels.ExtractFlagsFromJavaLines(test_data)

    self.assertEqual(4, len(flags))
    self.assertEqual('some-switch', flags[0].name)
    self.assertFalse(flags[0].is_base_feature)
    self.assertEqual('some-other-switch', flags[1].name)
    self.assertFalse(flags[1].is_base_feature)
    self.assertEqual('SomeFeature', flags[2].name)
    self.assertTrue(flags[2].is_base_feature)
    self.assertEqual('SomeOtherFeature', flags[3].name)
    self.assertTrue(flags[3].is_base_feature)


if __name__ == '__main__':
  unittest.main()
