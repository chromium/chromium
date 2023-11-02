// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['switch_access_e2e_test_base.js']);

/** Test fixture for the SwitchAccess class. */
SwitchAccessSwitchAccessTest = class extends SwitchAccessE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    await importModule('SwitchAccess', '/switch_access/switch_access.js');
  }
};

AX_TEST_F('SwitchAccessSwitchAccessTest', 'NoFocusDefersInit', function() {
  // Build a new SwitchAccess instance with hooks.
  let initCount = 0;
  SwitchAccess.finishInit_ = () => initCount++;

  // A fake desktop.
  const fakeDesktop = {};
  fakeDesktop.addEventListener = () => {};
  fakeDesktop.removeEventListener = () => {};

  // Stub out this to be synchronous.
  chrome.automation.getDesktop = callback => callback(fakeDesktop);

  // Stub this out as well so that focus is undefined.
  chrome.automation.getFocus = callback => callback();

  // Initialize; we should not have called finishInit_ since there's no focus.
  SwitchAccess.initialize();
  assertEquals(0, initCount);

  // Restub this to pass a "focused" node.
  chrome.automation.getFocus = callback => callback({});
  SwitchAccess.initialize();
  assertEquals(1, initCount);
});
