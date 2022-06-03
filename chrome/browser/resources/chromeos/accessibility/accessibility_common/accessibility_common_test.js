// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../common/testing/e2e_test_base.js']);

/**
 * Accessibility common extension browser tests.
 */
AccessibilityCommonE2ETest = class extends E2ETestBase {
  constructor() {
    super();
  }

  /** @override */
  testGenCppIncludes() {
    super.testGenCppIncludes();
    GEN(`
#include "ash/accessibility/accessibility_delegate.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/test/browser_test.h"
    `);
  }

  /** @override */
  testGenPreamble() {
    super.testGenPreamble();
    // Note that at least one accessibility common feature has to be enabled for
    // the extension to load. Extension load is required for this test suite to
    // have a place to be injected.
    GEN(`
  base::OnceClosure load_cb =
      base::BindOnce(&ash::AccessibilityManager::EnableAutoclick,
          base::Unretained(ash::AccessibilityManager::Get()),
          true);
    `);
    super.testGenPreambleCommon('kAccessibilityCommonExtensionId');
  }

  async getPref(name) {
    return new Promise(resolve => {
      chrome.settingsPrivate.getPref(name, (ret) => {
        resolve(ret);
      });
    });
  }

  async setPref(name, value) {
    return new Promise(resolve => {
      chrome.settingsPrivate.setPref(name, value, undefined, () => {
        resolve();
      });
    });
  }
};

TEST_F('AccessibilityCommonE2ETest', 'ToggleFeatures', function() {
  this.newCallback(async () => {
    // First, verify autoclick is already on.
    let pref = await this.getPref('settings.a11y.autoclick');
    assertEquals('settings.a11y.autoclick', pref.key);
    assertTrue(pref.value);
    assertTrue(!!accessibilityCommon.getAutoclickForTest());

    // Next, flip on screen magnifier and verify all prefs and internal state.
    await this.setPref('settings.a11y.screen_magnifier', true);
    pref = await this.getPref('settings.a11y.autoclick');
    assertEquals('settings.a11y.autoclick', pref.key);
    assertTrue(pref.value);
    assertTrue(!!accessibilityCommon.getAutoclickForTest());
    pref = await this.getPref('settings.a11y.screen_magnifier');
    assertEquals('settings.a11y.screen_magnifier', pref.key);
    assertTrue(pref.value);
    assertTrue(!!accessibilityCommon.getMagnifierForTest());

    // Then, flip off autoclick and verify all prefs and internal state.
    await this.setPref('settings.a11y.autoclick', false);
    pref = await this.getPref('settings.a11y.autoclick');
    assertEquals('settings.a11y.autoclick', pref.key);
    assertFalse(pref.value);
    assertTrue(!accessibilityCommon.getAutoclickForTest());
    pref = await this.getPref('settings.a11y.screen_magnifier');
    assertEquals('settings.a11y.screen_magnifier', pref.key);
    assertTrue(pref.value);
    assertTrue(!!accessibilityCommon.getMagnifierForTest());

    // Unfortunately, turning off all features would remove the extension. Flip
    // autoclick back on.
    await this.setPref('settings.a11y.autoclick', true);
    pref = await this.getPref('settings.a11y.autoclick');
    assertEquals('settings.a11y.autoclick', pref.key);
    assertTrue(pref.value);
    assertTrue(!!accessibilityCommon.getAutoclickForTest());
    pref = await this.getPref('settings.a11y.screen_magnifier');
    assertEquals('settings.a11y.screen_magnifier', pref.key);
    assertTrue(pref.value);
    assertTrue(!!accessibilityCommon.getMagnifierForTest());

    // And, finally flip screen magnifier off.
    await this.setPref('settings.a11y.screen_magnifier', false);
    pref = await this.getPref('settings.a11y.autoclick');
    assertEquals('settings.a11y.autoclick', pref.key);
    assertTrue(pref.value);
    assertTrue(!!accessibilityCommon.getAutoclickForTest());
    pref = await this.getPref('settings.a11y.screen_magnifier');
    assertEquals('settings.a11y.screen_magnifier', pref.key);
    assertFalse(pref.value);
    assertTrue(!accessibilityCommon.getMagnifierForTest());
  })();
});
