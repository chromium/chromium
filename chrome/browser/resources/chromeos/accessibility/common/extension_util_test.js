// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['testing/common_e2e_test_base.js']);

/** Test fixture for extension_util.ts. */
AccessibilityExtensionExtensionUtilTest = class extends CommonE2ETestBase {
};

AX_TEST_F('AccessibilityExtensionExtensionUtilTest', 'IsValidSender', function() {
  // Valid sender
  assertTrue(ExtensionUtil.isValidSender({
    url: 'chrome-extension://' + chrome.runtime.id + '/background.html'
  }));
  assertTrue(ExtensionUtil.isValidSender({
    url: 'chrome-extension://' + chrome.runtime.id + '/mv3/panel/panel.html'
  }));

  // Invalid senders: mismatched extension ID
  assertFalse(ExtensionUtil.isValidSender({
    url: 'chrome-extension://wrong-extension-id/background.html'
  }));

  // Invalid senders: web origin
  assertFalse(ExtensionUtil.isValidSender({
    url: 'https://docs.google.com/document'
  }));

  // Invalid senders: empty or missing URL
  assertFalse(ExtensionUtil.isValidSender({
    url: ''
  }));
  assertFalse(ExtensionUtil.isValidSender({}));
  assertFalse(ExtensionUtil.isValidSender(undefined));
});
