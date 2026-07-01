#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

# Add repo root to path for PRESUBMIT_test_mocks
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

import PRESUBMIT
from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi, MockFile


class AwFeatureOverridesCommentsTest(unittest.TestCase):

  def testValidDisabledTemporary(self):
    lines = [
        '  // DISABLED_TEMPORARY: crbug.com/123456',
        '  aw_feature_overrides.DisableFeature(blink::features::kSomeFeature);',
    ]
    input_api = MockInputApi()
    input_api.files = [
        MockFile('android_webview/browser/aw_field_trials.cc', lines)
    ]
    errors = PRESUBMIT._CheckAwFeatureOverridesComments(
        input_api, MockOutputApi())
    self.assertEqual(0, len(errors))

  def testValidDisabledIncompatible(self):
    lines = [
        '  // DISABLED_INCOMPATIBLE: WebView architecture constraints',
        '  aw_feature_overrides.DisableFeature(blink::features::kSomeFeature);',
    ]
    input_api = MockInputApi()
    input_api.files = [
        MockFile('android_webview/browser/aw_field_trials.cc', lines)
    ]
    errors = PRESUBMIT._CheckAwFeatureOverridesComments(
        input_api, MockOutputApi())
    self.assertEqual(0, len(errors))

  def testValidDisabledNeedsApi(self):
    lines = [
        '  // DISABLED_NEEDS_API: requires app-facing setting to turn it off',
        '  aw_feature_overrides.DisableFeature(blink::features::kSomeFeature);',
    ]
    input_api = MockInputApi()
    input_api.files = [
        MockFile('android_webview/browser/aw_field_trials.cc', lines)
    ]
    errors = PRESUBMIT._CheckAwFeatureOverridesComments(
        input_api, MockOutputApi())
    self.assertEqual(0, len(errors))

  def testValidDisabledOther(self):
    lines = [
        '  // DISABLED_OTHER: some other reason goes here',
        '  aw_feature_overrides.DisableFeature(blink::features::kSomeFeature);',
    ]
    input_api = MockInputApi()
    input_api.files = [
        MockFile('android_webview/browser/aw_field_trials.cc', lines)
    ]
    errors = PRESUBMIT._CheckAwFeatureOverridesComments(
        input_api, MockOutputApi())
    self.assertEqual(0, len(errors))

  def testValidMultipleFeaturesWithOneReason(self):
    lines = [
        '  // DISABLED_TEMPORARY: crbug.com/123456',
        '  aw_feature_overrides.DisableFeature(blink::features::kFeatureA);',
        '  aw_feature_overrides.DisableFeature(blink::features::kFeatureB);',
    ]
    input_api = MockInputApi()
    input_api.files = [
        MockFile('android_webview/browser/aw_field_trials.cc', lines)
    ]
    errors = PRESUBMIT._CheckAwFeatureOverridesComments(
        input_api, MockOutputApi())
    self.assertEqual(0, len(errors))

  def testValidMultiLineComment(self):
    lines = [
        '  // DISABLED_TEMPORARY: crbug.com/123',
        '  // Some extra explanation here.',
        '  aw_feature_overrides.DisableFeature(blink::features::kSomeFeature);',
    ]
    input_api = MockInputApi()
    input_api.files = [
        MockFile('android_webview/browser/aw_field_trials.cc', lines)
    ]
    errors = PRESUBMIT._CheckAwFeatureOverridesComments(
        input_api, MockOutputApi())
    self.assertEqual(0, len(errors))

  def testValidMultiLineComment_alternateOrder(self):
    lines = [
        '  // Some extra explanation here.',
        '  // DISABLED_TEMPORARY: crbug.com/123',
        '  aw_feature_overrides.DisableFeature(blink::features::kSomeFeature);',
    ]
    input_api = MockInputApi()
    input_api.files = [
        MockFile('android_webview/browser/aw_field_trials.cc', lines)
    ]
    errors = PRESUBMIT._CheckAwFeatureOverridesComments(
        input_api, MockOutputApi())
    self.assertEqual(0, len(errors))

  def testInvalidCommentPrefix(self):
    lines = [
        '  // Disable this feature without the proper comment prefix',
        '  aw_feature_overrides.DisableFeature(blink::features::kSomeFeature);',
    ]
    input_api = MockInputApi()
    input_api.files = [
        MockFile('android_webview/browser/aw_field_trials.cc', lines)
    ]
    errors = PRESUBMIT._CheckAwFeatureOverridesComments(
        input_api, MockOutputApi())
    self.assertEqual(1, len(errors))
    self.assertIn(
        'Preceding comment was: Line 1: '
        '// Disable this feature without the proper comment prefix',
        errors[0].items[0])

  def testInvalidMissingCommentEmptyLine(self):
    lines = [
        '',
        '  aw_feature_overrides.DisableFeature(blink::features::kSomeFeature);',
    ]
    input_api = MockInputApi()
    input_api.files = [
        MockFile('android_webview/browser/aw_field_trials.cc', lines)
    ]
    errors = PRESUBMIT._CheckAwFeatureOverridesComments(
        input_api, MockOutputApi())
    self.assertEqual(1, len(errors))
    self.assertIn('Preceding comment was: None', errors[0].items[0])

  def testInvalidMissingCommentOtherCode(self):
    lines = [
        '  some_other_code();',
        '  aw_feature_overrides.DisableFeature(blink::features::kSomeFeature);',
    ]
    input_api = MockInputApi()
    input_api.files = [
        MockFile('android_webview/browser/aw_field_trials.cc', lines)
    ]
    errors = PRESUBMIT._CheckAwFeatureOverridesComments(
        input_api, MockOutputApi())
    self.assertEqual(1, len(errors))
    self.assertIn('Preceding comment was: None', errors[0].items[0])

  def testInvalidMissingPrefix_multipleFeatures(self):
    lines = [
        '  // Disable these features',
        '  aw_feature_overrides.DisableFeature(blink::features::kFeatureA);',
        '  aw_feature_overrides.DisableFeature(blink::features::kFeatureB);',
    ]
    input_api = MockInputApi()
    input_api.files = [
        MockFile('android_webview/browser/aw_field_trials.cc', lines)
    ]
    errors = PRESUBMIT._CheckAwFeatureOverridesComments(
        input_api, MockOutputApi())
    self.assertEqual(1, len(errors))
    self.assertEqual(2, len(errors[0].items))

  def testMixedValidAndInvalid_multipleFeatures(self):
    lines = [
        '  // This feature has no prefix',
        '  aw_feature_overrides.DisableFeature(blink::features::kFeatureA);',
        '  // DISABLED_TEMPORARY: This feature was annotated correctly',
        '  aw_feature_overrides.DisableFeature(blink::features::kFeatureB);',
    ]
    input_api = MockInputApi()
    input_api.files = [
        MockFile('android_webview/browser/aw_field_trials.cc', lines)
    ]
    errors = PRESUBMIT._CheckAwFeatureOverridesComments(
        input_api, MockOutputApi())
    self.assertEqual(1, len(errors))
    self.assertIn(
        'Preceding comment was: Line 1: // This feature has no prefix',
        errors[0].items[0])

  def testInvalidMissingExplanation(self):
    lines = [
        '  // DISABLED_TEMPORARY:',
        '  aw_feature_overrides.DisableFeature(blink::features::kSomeFeature);',
    ]
    input_api = MockInputApi()
    input_api.files = [
        MockFile('android_webview/browser/aw_field_trials.cc', lines)
    ]
    errors = PRESUBMIT._CheckAwFeatureOverridesComments(
        input_api, MockOutputApi())
    self.assertEqual(1, len(errors))
    self.assertIn(
        'You must provide an explanation why the feature is disabled',
        errors[0].items[0])

  def testIgnoreOtherFiles(self):
    lines = [
        '// DISABLED_TEMPORARY: should not matter',
        'aw_feature_overrides.DisableFeature(blink::features::kSomeFeature);',
    ]
    input_api = MockInputApi()
    input_api.files = [
        MockFile('android_webview/browser/other_file.cc', lines)
    ]
    errors = PRESUBMIT._CheckAwFeatureOverridesComments(
        input_api, MockOutputApi())
    self.assertEqual(0, len(errors))


if __name__ == '__main__':
  unittest.main()
