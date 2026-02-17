// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['switch_access_e2e_test_base.js']);

/** Test fixture for the SwitchAccess class. */
SwitchAccessSwitchAccessTest = class extends SwitchAccessE2ETest {
  async waitForCallback() {
    return new Promise(resolve => this.promiseCallback = resolve);
  }
};

function resetState() {
  delete Flags.instance;
  delete SwitchAccess.instance;
}

AX_TEST_F(
    'SwitchAccessSwitchAccessTest', 'NoFocusDefersInit', async function() {
      await this.runWithLoadedTree('');
      // Build a new SwitchAccess instance with hooks.
      let initCount = 0;
      const oldInit = SwitchAccess.init;
      SwitchAccess.init = async (...args) => {
        await oldInit(...args);
        initCount++;
        assertNotNullNorUndefined(this.promiseCallback);
        this.promiseCallback();
        delete this.promiseCallback;
      };

      // Stub this out so that focus is undefined.
      chrome.automation.getFocus = callback => {
        callback();
        assertNotNullNorUndefined(this.promiseCallback);
        this.promiseCallback();
        delete this.promiseCallback;
      };

      // Reset state so there are no re-initialization errors.
      resetState();

      // Initialize; we should not have incremented initCount since there's no
      // focus.
      SwitchAccess.init(this.desktop);
      await this.waitForCallback();
      assertEquals(0, initCount);

      // Reset state so there are no re-initialization errors.
      resetState();

      // Restub this to pass a "focused" node.
      chrome.automation.getFocus = callback => callback({});
      SwitchAccess.init(this.desktop);
      await this.waitForCallback();
      assertEquals(1, initCount);
    });
