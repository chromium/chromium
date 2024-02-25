// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['dictation_test_base.js']);

DictationFocusHandlerTest = class extends DictationE2ETestBase {
  /** @return {!FocusHandler} */
  getFocusHandler() {
    return accessibilityCommon.dictation_.focusHandler_;
  }

  /** @return {!Promise} */
  async activateFocusHandler() {
    await this.getFocusHandler().refresh();
    assertTrue(this.getFocusHandler().active_);
    assertNotNullNorUndefined(this.getFocusHandler().deactivateTimeoutId_);
  }

  /**
   * @param {boolean} active
   * @return {!Promise}
   */
  async waitForFocusHandlerActive(active) {
    const focusHandler = this.getFocusHandler();
    const activeOk = () => {
      return focusHandler.active_ === active;
    };

    if (activeOk()) {
      return;
    }

    await new Promise(resolve => {
      const onActiveChanged = () => {
        if (activeOk()) {
          focusHandler.onActiveChangedForTesting_ = null;
          resolve();
        }
      };

      focusHandler.onActiveChangedForTesting_ = onActiveChanged;
    });
  }

  /**
   * @param {!AutomationNode} target
   * @return {!Promise}
   */
  async waitForFocus(target) {
    const focusHandler = this.getFocusHandler();
    const isTargetFocused = () => {
      return focusHandler.editableNode_ === target;
    };

    if (isTargetFocused()) {
      return;
    }

    await new Promise(resolve => {
      const onFocusChanged = () => {
        if (isTargetFocused()) {
          focusHandler.onFocusChangedForTesting_ = null;
          resolve();
        }
      };

      focusHandler.onFocusChangedForTesting_ = onFocusChanged;
    });
  }

  /** @return {string} */
  simpleSite() {
    return `
      <button autofocus>Start</button>
      <input id="first" type="text"></input>
    `;
  }
};

// This test ensures that FocusHandler activates when Dictation is toggled on.
AX_TEST_F('DictationFocusHandlerTest', 'Activate', async function() {
  const root = await this.runWithLoadedTree(this.simpleSite());
  const input = root.find({role: chrome.automation.RoleType.TEXT_FIELD});
  assertTrue(Boolean(input));

  // FocusHandler should not be activated until Dictation is toggled on.
  assertFalse(this.getFocusHandler().active_);
  assertEquals(null, this.getFocusHandler().editableNode_);
  input.focus();
  this.toggleDictationOn();
  await this.waitForFocusHandlerActive(true);
});

// This test ensures that FocusHandler deactivates automatically after a
// period of inactivity.
AX_TEST_F('DictationFocusHandlerTest', 'Deactivate', async function() {
  // Shorten timeout for testing.
  FocusHandler.DEACTIVATE_TIMEOUT_MS_ = 1000;
  await this.activateFocusHandler();
  await this.waitForFocusHandlerActive(false);
});

// This test ensures that FocusHandler tracks focus once it's been activated.
AX_TEST_F('DictationFocusHandlerTest', 'OnFocusChanged', async function() {
  await this.activateFocusHandler();
  const root = await this.runWithLoadedTree(this.simpleSite());
  const input = root.find({role: chrome.automation.RoleType.TEXT_FIELD});
  assertTrue(Boolean(input));

  input.focus();
  await this.waitForFocus(input);
});

// This test ensures that the timeout to deactivate FocusHandler is reset
// whenever Dictation toggles on.
AX_TEST_F(
    'DictationFocusHandlerTest', 'ResetDeactivateTimeout', async function() {
      this.mockSetTimeoutMethod();
      this.toggleDictationOn();
      await this.waitForFocusHandlerActive(true);
      let callback =
          this.getCallbackWithDelay(FocusHandler.DEACTIVATE_TIMEOUT_MS_);
      assertNotNullNorUndefined(callback);

      this.clearSetTimeoutData();
      this.toggleDictationOff();
      this.toggleDictationOn();
      // Toggling Dictation on should set a new timeout to deactivate
      // FocusHandler.
      callback = this.getCallbackWithDelay(FocusHandler.DEACTIVATE_TIMEOUT_MS_);
      assertNotNullNorUndefined(callback);
      // Running `callback` should deactivate FocusHandler.
      callback();
      await this.waitForFocusHandlerActive(false);
    });
