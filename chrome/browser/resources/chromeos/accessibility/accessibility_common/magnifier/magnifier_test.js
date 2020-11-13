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
    window.RoleType = chrome.automation.RoleType;
  }

  async getNextMagnifierBounds() {
    return new Promise(resolve => {
      const listener = (magnifierBounds) => {
        chrome.accessibilityPrivate.onMagnifierBoundsChanged.removeListener(
            listener);
        resolve(magnifierBounds);
      };
      chrome.accessibilityPrivate.onMagnifierBoundsChanged.addListener(
          listener);
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
        <button id="apple">Apple</button><br />
        <button id="banana" style="margin-top: 400px">Banana</button>
      `;
  this.runWithLoadedTree(site, async function(root) {
    const apple = root.find({attributes: {name: 'Apple'}});
    const banana = root.find({attributes: {name: 'Banana'}});

    // Focus and move magnifier to apple.
    apple.focus();

    // Verify magnifier bounds contains apple, but not banana.
    let bounds = await this.getNextMagnifierBounds();
    assertTrue(RectUtil.contains(bounds, apple.location));
    assertFalse(RectUtil.contains(bounds, banana.location));

    // Focus and move magnifier to banana.
    banana.focus();

    // Verify magnifier bounds contains banana, but not apple.
    bounds = await this.getNextMagnifierBounds();
    assertFalse(RectUtil.contains(bounds, apple.location));
    assertTrue(RectUtil.contains(bounds, banana.location));
  });
});

// Disabled - flaky: https://crbug.com/1145612
TEST_F(
    'MagnifierE2ETest', 'DISABLED_MovesDockedMagnifierToActiveDescendant', function() {
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
        // Enable docked magnifier.
        await new Promise(resolve => {
          chrome.accessibilityFeatures.dockedMagnifier.set(
              {value: true}, resolve);
        });

        // Validate magnifier wants to move to root.
        const rootLocation = await getNextMagnifierLocation();
        assertTrue(RectUtil.equal(rootLocation, root.location));

        // Click parent to change active descendant from apple to banana.
        const parent = root.find({role: RoleType.GROUP});
        parent.doDefault();

        // Register and wait for rect from magnifier.
        const rect = await getNextMagnifierLocation();

        // Validate rect from magnifier is rect of banana.
        const bananaNode =
            root.find({role: RoleType.TREE_ITEM, attributes: {name: 'Banana'}});
        assertTrue(RectUtil.equal(rect, bananaNode.location));
      }, {returnPage: true});
    });


TEST_F(
    'MagnifierE2ETest', 'MovesScreenMagnifierToActiveDescendant', function() {
      const site = `
    <span tabindex="1">Top</span>
    <div id="group" role="group" style="width: 200px"
        aria-activedescendant="apple">
      <div id="apple" role="treeitem">Apple</div>
      <div id="banana" role="treeitem" style="margin-top: 400px">Banana</div>
    </div>
    <script>
      const group = document.getElementById('group');
      group.addEventListener('click', function() {
        group.setAttribute('aria-activedescendant', 'banana');
      });
    </script>
  `;
      this.runWithLoadedTree(site, async function(root) {
        const top = root.find({attributes: {name: 'Top'}});
        const banana = root.find({attributes: {name: 'Banana'}});
        const group = root.find({role: RoleType.GROUP});

        // Focus and move magnifier to top.
        top.focus();

        // Verify magnifier bounds don't contain banana.
        let bounds = await this.getNextMagnifierBounds();
        assertFalse(RectUtil.contains(bounds, banana.location));

        // Click group to change active descendant to banana.
        group.doDefault();

        // Verify magnifier bounds contain banana.
        bounds = await this.getNextMagnifierBounds();
        assertTrue(RectUtil.contains(bounds, banana.location));
      });
    });
