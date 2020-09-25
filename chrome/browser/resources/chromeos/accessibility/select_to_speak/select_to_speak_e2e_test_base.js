// Copyright 2018 The Chromium Authors. All rights reserved.
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
#include "ash/accessibility/accessibility_delegate.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/test/browser_test.h"
    `);
  }

  /** @override */
  testGenPreamble() {
    super.testGenPreamble();
    GEN(`
    //keyboard::SetRequestedKeyboardState(keyboard::KEYBOARD_STATE_ENABLED);
    //ash::Shell::Get()->CreateKeyboard();
    base::Closure load_cb =
        base::Bind(&chromeos::AccessibilityManager::SetSelectToSpeakEnabled,
            base::Unretained(chromeos::AccessibilityManager::Get()),
            true);
    chromeos::AccessibilityManager::Get()->SetSelectToSpeakEnabled(true);
    WaitForExtension(extension_misc::kSelectToSpeakExtensionId, load_cb);
      `);
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
};
