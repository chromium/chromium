// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../../common/testing/e2e_test_base.js']);
GEN_INCLUDE(['../../common/testing/mock_accessibility_private.js']);
GEN_INCLUDE(['../../common/rect_util.js']);

/**
 * Magnifier feature using accessibility common extension browser tests.
 */
MagnifierE2ETest = class extends E2ETestBase {
  constructor() {
    super();
    this.mockAccessibilityPrivate = MockAccessibilityPrivate;
    chrome.accessibilityPrivate = this.mockAccessibilityPrivate;

    window.RoleType = chrome.automation.RoleType;
    window.getNextMagnifierLocation = this.getNextMagnifierLocation;

    // Re-initialize AccessibilityCommon with mock AccessibilityPrivate API.
    window.accessibilityCommon = new AccessibilityCommon();
  }

  async getNextMagnifierLocation() {
    return new Promise(resolve => {
      chrome.accessibilityPrivate.registerMoveMagnifierToRectCallback(resolve);
    });
  }

  /** @override */
  testGenCppIncludes() {
    super.testGenCppIncludes();
    GEN(`
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/test/browser_test.h"
    `);
  }

  /** @override */
  testGenPreamble() {
    super.testGenPreamble();
    GEN(`
    base::Closure load_cb =
        base::Bind(&chromeos::MagnificationManager::SetMagnifierEnabled,
            base::Unretained(chromeos::MagnificationManager::Get()),
            true);
    WaitForExtension(extension_misc::kAccessibilityCommonExtensionId, load_cb);
      `);
  }
};

TEST_F('MagnifierE2ETest', 'MovesScreenMagnifierToFocusedElement', function() {
  const site = `
        <button id="apple">Apple</button>
        <button id="banana">Banana</button>
      `;
  this.runWithLoadedTree(site, async function(root) {
    // Validate magnifier wants to move to root.
    const rootLocation = await getNextMagnifierLocation();
    assertTrue(RectUtil.equal(rootLocation, root.location));

    // Focus banana node.
    const banana =
        root.find({role: RoleType.BUTTON, attributes: {name: 'Banana'}});
    banana.focus();

    // Validate magnifier wants to move to banana.
    const bananaLocation = await getNextMagnifierLocation();
    assertTrue(RectUtil.equal(bananaLocation, banana.location));
  });
});

TEST_F(
    'MagnifierE2ETest', 'MovesScreenMagnifierToActiveDescendant', function() {
      const site = `
    <div role="group" id="parent" aria-activedescendant="apple">
      <div id="apple" role="treeitem">Apple</div>
      <div id="banana" role="treeitem">Banana</div>
    </div>
    <script>
      const parent = document.getElementById('parent');
      parent.addEventListener('click', function() {
        parent.setAttribute('aria-activedescendant', 'banana');
      });
      </script>
  `;
      this.runWithLoadedTree(site, async function(root) {
        // Click parent to change active descendant from apple to banana.
        const parent = root.find({role: RoleType.GROUP});
        parent.doDefault();

        // Register and wait for rect from magnifier.
        const rect = await getNextMagnifierLocation();

        // Validate rect from magnifier is rect of banana.
        const bananaNode =
            root.find({role: RoleType.TREE_ITEM, attributes: {name: 'Banana'}});
        assertTrue(RectUtil.equal(rect, bananaNode.location));
      });
    });
