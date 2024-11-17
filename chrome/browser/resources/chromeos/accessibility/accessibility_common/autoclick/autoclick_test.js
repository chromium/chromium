// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../../common/testing/e2e_test_base.js']);
GEN_INCLUDE(['../../common/testing/mock_accessibility_private.js']);

/**
 * Automatic clicks feature using accessibility common extension browser tests.
 */
AutoclickE2ETest = class extends E2ETestBase {
  async setUpDeferred() {
    this.mockAccessibilityPrivate = new MockAccessibilityPrivate();
    chrome.accessibilityPrivate = this.mockAccessibilityPrivate;

    window.RoleType = chrome.automation.RoleType;

    // Re-initialize AccessibilityCommon with mock AccessibilityPrivate API.
    accessibilityCommon = new AccessibilityCommon();

    await new Promise(r => {
      chrome.accessibilityFeatures.autoclick.get({}, () => {
        // Turn off focus ring blinking for test after autoclick is initialized.
        accessibilityCommon.getAutoclickForTest().setNoBlinkFocusRingsForTest();
        r();
      });
    });

    await super.setUpDeferred();
  }

  /** @override */
  testGenCppIncludes() {
    super.testGenCppIncludes();
    GEN(`
#include "ash/accessibility/accessibility_delegate.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
    `);
  }

  /** @override */
  testGenPreamble() {
    GEN(`
  base::OnceClosure load_cb =
      base::BindOnce(&ash::AccessibilityManager::EnableAutoclick,
          base::Unretained(ash::AccessibilityManager::Get()),
          true);
    `);
    super.testGenPreambleCommon('kAccessibilityCommonExtensionId');
  }

  /**
   * Asserts that two rects are the same.
   * @param {!chrome.accessibilityPrivate.ScreenRect} first
   * @param {!chrome.accessibilityPrivate.ScreenRect} second
   */
  assertSameRect(first, second) {
    assertTrue(RectUtil.equal(first, second));
  }
};

AX_TEST_F(
    'AutoclickE2ETest', 'HighlightsRootWebAreaIfNotScrollable',
    async function() {
      const root = await this.runWithLoadedTree(
          'data:text/html;charset=utf-8,<p>Cats rock!</p>');
      const node = root.find(
          {role: RoleType.STATIC_TEXT, attributes: {name: 'Cats rock!'}});
      await new Promise(resolve => {
        this.mockAccessibilityPrivate.callOnScrollableBoundsForPointRequested(
            // Offset slightly into the node to ensure the hittest
            // happens within the node.
            node.location.left + 1, node.location.top + 1, resolve);
      });
      const expected = node.root.location;
      const focusRings = this.mockAccessibilityPrivate.getFocusRings();
      this.assertSameRect(
          this.mockAccessibilityPrivate.getScrollableBounds(), expected);
      this.assertSameRect(focusRings[0].rects[0], expected);
    });

AX_TEST_F('AutoclickE2ETest', 'HighlightsScrollableDiv', async function() {
  const root = await this.runWithLoadedTree(
      'data:text/html;charset=utf-8,' +
      '<div style="width:100px;height:100px;overflow:scroll">' +
      '<div style="margin:50px">cats rock! this text wraps and overflows!' +
      '</div></div>');
  const node = root.find({
    role: RoleType.STATIC_TEXT,
    attributes: {name: 'cats rock! this text wraps and overflows!'},
  });
  await new Promise(resolve => {
    this.mockAccessibilityPrivate.callOnScrollableBoundsForPointRequested(
        // Offset slightly into the node to ensure the hittest happens
        // within the node.
        node.location.left + 1, node.location.top + 1, resolve);
  });
  // The outer div, which is the parent of the parent of the
  // text, is scrollable.
  assertTrue(node.parent.parent.scrollable);
  const expected = node.parent.parent.location;
  const focusRings = this.mockAccessibilityPrivate.getFocusRings();
  this.assertSameRect(
      this.mockAccessibilityPrivate.getScrollableBounds(), expected);
  this.assertSameRect(focusRings[0].rects[0], expected);
});

AX_TEST_F('AutoclickE2ETest', 'RemovesAndAddsAutoclick', async function() {
  const root = await this.runWithLoadedTree(
      'data:text/html;charset=utf-8,<p>Cats rock!</p>');
  // Turn on screen magnifier so that when we turn off autoclick, the
  // extension doesn't get unloaded and crash the test.
  await new Promise(resolve => {
    chrome.accessibilityFeatures.screenMagnifier.set({value: true}, resolve);
  });

  // Toggle autoclick off and on, ensure it still works and no crashes.
  await new Promise(resolve => {
    chrome.accessibilityFeatures.autoclick.set({value: false}, resolve);
  });
  await new Promise(resolve => {
    chrome.accessibilityFeatures.autoclick.set({value: true}, resolve);
  });
  const node =
      root.find({role: RoleType.STATIC_TEXT, attributes: {name: 'Cats rock!'}});
  await new Promise(resolve => {
    this.mockAccessibilityPrivate.callOnScrollableBoundsForPointRequested(
        // Offset slightly into the node to ensure the hittest happens
        // within the node.
        node.location.left + 1, node.location.top + 1, resolve);
  });
  const expected = node.root.location;
  const focusRings = this.mockAccessibilityPrivate.getFocusRings();
  this.assertSameRect(
      this.mockAccessibilityPrivate.getScrollableBounds(), expected);
  this.assertSameRect(focusRings[0].rects[0], expected);
});

// TODO(crbug.com/41467584): Add tests for when the scrollable area is scrolled
// all the way up or down, left or right. Add tests for nested scrollable areas.
// Add tests for root types like toolbar, dialog, and window to ensure
// we don't break boundaries when searching for scroll bars.
