// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['testing/common_e2e_test_base.js']);

/** Test fixture for string_util.js. */
AccessibilityExtensionStringUtilTest = class extends CommonE2ETestBase {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    await importModule('StringUtil', '/common/string_util.js');
  }
};

AX_TEST_F('AccessibilityExtensionStringUtilTest', 'CommonPrefix', function() {
  assertEquals(
      11, StringUtil.longestCommonPrefixLength('Hello World', 'Hello World'));
  assertEquals(
      0, StringUtil.longestCommonPrefixLength('Hello World', 'Good Morning'));
  assertEquals(
      1, StringUtil.longestCommonPrefixLength('Guten Morgen', 'Good Morning'));

  assertEquals(5, StringUtil.longestCommonPrefixLength('Hello', 'Hello World'));
  assertEquals(5, StringUtil.longestCommonPrefixLength('Hello World', 'Hello'));
  assertEquals(0, StringUtil.longestCommonPrefixLength('Hello', ''));
  assertEquals(0, StringUtil.longestCommonPrefixLength('', 'Hello'));
});

AX_TEST_F('AccessibilityExtensionStringUtilTest', 'CommonSuffix', function() {
  assertEquals(
      11, StringUtil.longestCommonSuffixLength('Hello World', 'Hello World'));
  assertEquals(
      0, StringUtil.longestCommonSuffixLength('Hello World', 'Good Morning'));
  assertEquals(
      4, StringUtil.longestCommonSuffixLength('Good Morning', 'Good Evening'));

  assertEquals(5, StringUtil.longestCommonSuffixLength('World', 'Hello World'));
  assertEquals(5, StringUtil.longestCommonSuffixLength('Hello World', 'World'));
  assertEquals(0, StringUtil.longestCommonSuffixLength('Hello', ''));
  assertEquals(0, StringUtil.longestCommonSuffixLength('', 'Hello'));
});
