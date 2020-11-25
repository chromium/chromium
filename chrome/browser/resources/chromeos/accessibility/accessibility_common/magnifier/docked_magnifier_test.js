// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../../common/testing/e2e_test_base.js']);
GEN_INCLUDE(['../../common/testing/mock_accessibility_private.js']);
GEN_INCLUDE(['../../common/rect_util.js']);
GEN_INCLUDE(['magnifier_test_common.js']);

/**
 * Magnifier feature using accessibility common extension browser tests.
 */
DockedMagnifierE2ETest = class extends E2ETestBase {
  constructor() {
    super();
    window.RoleType = chrome.automation.RoleType;

    /**
     * Registers a listener for
     * chrome.accessibilityPrivate.onMagnifierBoundsChanged in the ctor. After
     * that, the test code can perform the action, and then use
     * waitForNextMagnifierBounds() to be wait for the previously-registered
     * listener to be called.
     *
     * @private
     */
    this.nextMagnifierBoundsWaiter_ = class {
      constructor() {
        this.promise_ = new Promise(resolve => {
          const listener = (magnifierBounds) => {
            chrome.accessibilityPrivate.onMagnifierBoundsChanged.removeListener(
                listener);
            resolve(magnifierBounds);
          };
          chrome.accessibilityPrivate.onMagnifierBoundsChanged.addListener(
              listener);
        });
      }

      async waitForNextMagnifierBounds() {
        return this.promise_;
      }
    };
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
        base::Bind(&chromeos::MagnificationManager::SetDockedMagnifierEnabled,
            base::Unretained(chromeos::MagnificationManager::Get()),
            true);
    WaitForExtension(extension_misc::kAccessibilityCommonExtensionId, load_cb);
      `);
  }
};

TEST_F(
    'DockedMagnifierE2ETest', 'MovesDockedMagnifierToActiveDescendant',
    function() {
      this.runWithLoadedTree(ActiveDescendantSite, async function(root) {
        const top = root.find({attributes: {name: 'Top'}});
        const banana = root.find({attributes: {name: 'Banana'}});
        const group = root.find({role: RoleType.GROUP});

        // Focus and move magnifier to top.
        // Then verify magnifier contain top and not banana.
        const boundsWaiter1 = new this.nextMagnifierBoundsWaiter_();
        top.focus();
        let bounds = await boundsWaiter1.waitForNextMagnifierBounds();
        assertTrue(RectUtil.contains(bounds, top.location));
        assertFalse(RectUtil.contains(bounds, banana.location));

        // Click group to change active descendant to banana.
        // Then verify magnifier bounds contain banana.
        const boundsWaiter2 = new this.nextMagnifierBoundsWaiter_();
        group.doDefault();
        bounds = await boundsWaiter2.waitForNextMagnifierBounds();
        assertFalse(RectUtil.contains(bounds, top.location));
        assertTrue(RectUtil.contains(bounds, banana.location));
      });
    });
