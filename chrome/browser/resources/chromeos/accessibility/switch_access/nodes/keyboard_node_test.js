// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../switch_access_e2e_test_base.js']);

/** Test fixture for the keyboard node. */
SwitchAccessKeyboardNodeTest = class extends SwitchAccessE2ETest {};

TEST_F('SwitchAccessKeyboardNodeTest', 'OpenAndClose', function() {
  this.runWithLoadedTree('<input type="text"></input>', async (desktop) => {
    const waitForKeyboardVisibility = async (expectedVisibility) => {
      return new Promise(resolve => {
        if (KeyboardRootNode.isVisible_ === expectedVisibility) {
          resolve();
          return;
        }

        const listener = (evt) => {
          if (evt.target.role === chrome.automation.RoleType.KEYBOARD &&
              KeyboardRootNode.isVisible_ === expectedVisibility) {
            resolve();
          }
        };
        desktop.addEventListener(
            chrome.automation.EventType.STATE_CHANGED, listener);
      });
    };

    const input = desktop.find({role: chrome.automation.RoleType.TEXT_FIELD});
    assertNotNullNorUndefined(input);
    NavigationManager.instance.moveTo_(input);
    assertEquals(
        chrome.automation.RoleType.TEXT_FIELD,
        NavigationManager.currentNode.role,
        'Did not successfully move to the input');

    // Open the virtual keyboard and assert focus gets placed on the back
    // button.
    NavigationManager.enterKeyboard();
    await waitForKeyboardVisibility(true);
    assertTrue(KeyboardRootNode.isVisible_);
    assertUndefined(KeyboardRootNode.getKeyboardObject().state.invisible);
    assertTrue(NavigationManager.currentNode instanceof BackButtonNode);

    // Close the virtual keyboard and assert focus moves back to the input.
    NavigationManager.exitKeyboard();
    await waitForKeyboardVisibility(false);
    assertFalse(KeyboardRootNode.isVisible_);
    assertTrue(KeyboardRootNode.getKeyboardObject().state.invisible);
    assertEquals(
        chrome.automation.RoleType.TEXT_FIELD,
        NavigationManager.currentNode.role,
        'Did not successfully move to the input');
  });
});