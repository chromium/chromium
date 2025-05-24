// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['testing/common_e2e_test_base.js']);

/** Test fixture for keep_alive.js. */
AccessibilityExtensionKeepAliveTest = class extends CommonE2ETestBase {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    // Mock chrome.runtime.getPlatformInfo()
    chrome = chrome || {};
    chrome.runtime = {
      getPlatformInfo: () => {
        return 1;
      }
    };
  }
};

AX_TEST_F(
    'AccessibilityExtensionKeepAliveTest', 'OnlyOneKeepAliveInstance',
    function() {
      assertTrue(
          KeepAlive.instance === undefined,
          'KeepAlive.instance before init() is undefined');

      KeepAlive.init();
      assertTrue(
          KeepAlive.instance !== undefined,
          'KeepAlive.instance after init() is not undefined');

      let keepAliveInstance = KeepAlive.instance;
      KeepAlive.init();
      assertTrue(
          keepAliveInstance === KeepAlive.instance,
          'KeepAlive.instance is the same after multiple init() calls');
    });

AX_TEST_F('AccessibilityExtensionKeepAliveTest', 'Stop', function() {
  KeepAlive.init();
  assertTrue(
      KeepAlive.instance !== undefined,
      'KeepAlive.instance after init() is not undefined');

  KeepAlive.stop();
  assertTrue(
      KeepAlive.instance === undefined,
      'KeepAlive.instance is deleted by KeepAlive.stop()');
});
