// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../../common/testing/e2e_test_base.js']);
GEN_INCLUDE(['../../common/testing/mock_accessibility_private.js']);
GEN_INCLUDE(['../../common/rect_util.js']);

/**
 * Automatic clicks feature using accessibility common extension browser tests.
 */
AutoclickE2ETest = class extends E2ETestBase {
  constructor() {
    super();
    this.mockAccessibilityPrivate = MockAccessibilityPrivate;
    chrome.accessibilityPrivate = this.mockAccessibilityPrivate;

    // Re-initialize AccessibilityCommon with mock AccessibilityPrivate API.
    accessibilityCommon = new AccessibilityCommon();

    chrome.accessibilityFeatures.autoclick.get({}, () => {
      // Turn off focus ring blinking for test after autoclick is initialized.
      accessibilityCommon.getAutoclickForTest().setNoBlinkFocusRingsForTest();
    });
  }

  /** @override */
  testGenCppIncludes() {
    GEN(`
#include "ash/accessibility/accessibility_delegate.h"
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
    GEN(`
  base::Closure load_cb =
      base::Bind(&chromeos::AccessibilityManager::EnableAutoclick,
          base::Unretained(chromeos::AccessibilityManager::Get()),
          true);
  chromeos::AccessibilityManager::Get()->EnableAutoclick(true);
  WaitForExtension(extension_misc::kAccessibilityCommonExtensionId, load_cb);
    `);
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

TEST_F('AutoclickE2ETest', 'HighlightsRootWebAreaIfNotScrollable', function() {
  this.runWithLoadedTree(
      'data:text/html;charset=utf-8,<p>Cats rock!</p>', function(desktop) {
        const node = desktop.find(
            {role: 'staticText', attributes: {name: 'Cats rock!'}});
        this.mockAccessibilityPrivate.callOnScrollableBoundsForPointRequested(
            // Offset slightly into the node to ensure the hittest
            // happens within the node.
            node.location.left + 1, node.location.top + 1,
            this.newCallback(() => {
              const expected = node.root.location;
              this.assertSameRect(
                  this.mockAccessibilityPrivate.getScrollableBounds(),
                  expected);
              this.assertSameRect(
                  this.mockAccessibilityPrivate.getFocusRings()[0], expected);
            }));
      });
});

TEST_F('AutoclickE2ETest', 'HighlightsScrollableDiv', function() {
  this.runWithLoadedTree(
      'data:text/html;charset=utf-8,' +
          '<div style="width:100px;height:100px;overflow:scroll">' +
          '<div style="margin:50px">cats rock! this text wraps and overflows!' +
          '</div></div>',
      function(desktop) {
        const node = desktop.find({
          role: 'staticText',
          attributes: {name: 'cats rock! this text wraps and overflows!'}
        });
        this.mockAccessibilityPrivate.callOnScrollableBoundsForPointRequested(
            // Offset slightly into the node to ensure the hittest happens
            // within the node.
            node.location.left + 1, node.location.top + 1,
            this.newCallback(() => {
              // The outer div, which is the parent of the parent of the
              // text, is scrollable.
              assertTrue(node.parent.parent.scrollable);
              const expected = node.parent.parent.location;
              this.assertSameRect(
                  this.mockAccessibilityPrivate.getScrollableBounds(),
                  expected);
              this.assertSameRect(
                  this.mockAccessibilityPrivate.getFocusRings()[0], expected);
            }));
      });
});

TEST_F('AutoclickE2ETest', 'RemovesAndAddsAutoclick', function() {
  this.runWithLoadedTree(
      'data:text/html;charset=utf-8,<p>Cats rock!</p>', function(desktop) {
        // Toggle autoclick off and on, ensure it still works and no crashes.
        chrome.accessibilityFeatures.autoclick.set({value: false}, () => {
          chrome.accessibilityFeatures.autoclick.set({value: true}, () => {
            const node = desktop.find(
                {role: 'staticText', attributes: {name: 'Cats rock!'}});
            this.mockAccessibilityPrivate
                .callOnScrollableBoundsForPointRequested(
                    // Offset slightly into the node to ensure the hittest
                    // happens within the node.
                    node.location.left + 1, node.location.top + 1,
                    this.newCallback(() => {
                      const expected = node.root.location;
                      this.assertSameRect(
                          this.mockAccessibilityPrivate.getScrollableBounds(),
                          expected);
                      this.assertSameRect(
                          this.mockAccessibilityPrivate.getFocusRings()[0],
                          expected);
                    }));
          });
        });
      });
});

// TODO(crbug.com/978163): Add tests for when the scrollable area is scrolled
// all the way up or down, left or right. Add tests for nested scrollable areas.
// Add tests for root types like toolbar, dialog, and window to ensure
// we don't break boundaries when searching for scroll bars.
