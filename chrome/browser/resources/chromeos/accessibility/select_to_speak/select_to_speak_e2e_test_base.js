// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../common/testing/e2e_test_base.js']);

/**
 * Base class for browser tests for select-to-speak.
 */
SelectToSpeakE2ETest = class extends E2ETestBase {
  /** @override */
  testGenCppIncludes() {
    super.testGenCppIncludes();
    GEN(`
#include "ash/keyboard/ui/keyboard_util.h"
#include "ui/accessibility/accessibility_features.h"
    `);
  }

  /** @override */
  testGenPreamble() {
    super.testGenPreamble();
    GEN(`
    base::OnceClosure load_cb =
        base::BindOnce(&ash::AccessibilityManager::SetSelectToSpeakEnabled,
            base::Unretained(ash::AccessibilityManager::Get()),
            true);
    `);
    super.testGenPreambleCommon('kSelectToSpeakExtensionId');
  }

  /**
   * Asserts that two strings are equal, collapsing repeated spaces and
   * removing leading / trailing whitespace.
   * @param {string} first The first string to compare.
   * @param {string} second The second string to compare.
   */
  assertEqualsCollapseWhitespace(first, second) {
    assertEquals(
        first.replace(/\s+/g, ' ').replace(/^\s/, '').replace(/\s$/, ''),
        second.replace(/\s+/g, ' ').replace(/^\s/, '').replace(/\s$/, ''));
  }

  /**
   * Helper function to find a staticText node from a root
   * @param {AutomationNode} root The root node to search through
   * @param {string} text The text to search for
   * @return {AutomationNode} The found text node, or null if none is found.
   */
  findTextNode(root, text) {
    return root.find({role: 'staticText', attributes: {name: text}});
  }

  /**
   * Triggers select-to-speak to read selected text at a keystroke.
   */
  triggerReadSelectedText() {
    assertFalse(this.mockTts.currentlySpeaking());
    assertEquals(this.mockTts.pendingUtterances().length, 0);
    selectToSpeak.fireMockKeyDownEvent(
        {keyCode: SelectToSpeakConstants.SEARCH_KEY_CODE});
    selectToSpeak.fireMockKeyDownEvent(
        {keyCode: SelectToSpeakConstants.READ_SELECTION_KEY_CODE});
    assertTrue(selectToSpeak.inputHandler_.isSelectionKeyDown_);
    selectToSpeak.fireMockKeyUpEvent(
        {keyCode: SelectToSpeakConstants.READ_SELECTION_KEY_CODE});
    selectToSpeak.fireMockKeyUpEvent(
        {keyCode: SelectToSpeakConstants.SEARCH_KEY_CODE});
  }

  /**
   * Triggers speech using the search key and clicking with the mouse.
   * @param {Object} downEvent The mouse-down event.
   * @param {Object} upEvent The mouse-up event.
   */
  triggerReadMouseSelectedText(downEvent, upEvent) {
    selectToSpeak.fireMockKeyDownEvent(
        {keyCode: SelectToSpeakConstants.SEARCH_KEY_CODE});
    selectToSpeak.fireMockMouseDownEvent(downEvent);
    selectToSpeak.fireMockMouseUpEvent(upEvent);
    selectToSpeak.fireMockKeyUpEvent(
        {keyCode: SelectToSpeakConstants.SEARCH_KEY_CODE});
  }

  /**
   * Waits one event loop before invoking callback. Useful if you are waiting
   * for pending promises to resolve.
   * @param {function()} callback
   */
  waitOneEventLoop(callback) {
    setTimeout(this.newCallback(callback), 0);
  }
};
