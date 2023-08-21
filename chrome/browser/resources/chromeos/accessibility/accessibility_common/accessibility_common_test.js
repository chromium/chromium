// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../common/testing/common_e2e_test_base.js']);

/**
 * Accessibility common extension browser tests.
 */
AccessibilityCommonE2ETest = class extends CommonE2ETestBase {
  async getFeature(name) {
    return new Promise(resolve => {
      chrome.accessibilityPrivate.isFeatureEnabled(
          name, enabled => resolve(enabled));
    });
  }

  async getPref(name) {
    return new Promise(resolve => {
      chrome.settingsPrivate.getPref(name, ret => {
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
    assertTrue(Boolean(accessibilityCommon.getAutoclickForTest()));

    // Then check that GameFace is disabled by default.
    const enabled = await this.getFeature(
        chrome.accessibilityPrivate.AccessibilityFeature.GAME_FACE_INTEGRATION);
    assertFalse(enabled);
    assertFalse(Boolean(accessibilityCommon.getGameFaceForTest()));

    // Next, flip on screen magnifier and verify all prefs and internal state.
    await this.setPref('settings.a11y.screen_magnifier', true);
    pref = await this.getPref('settings.a11y.autoclick');
    assertEquals('settings.a11y.autoclick', pref.key);
    assertTrue(pref.value);
    assertTrue(Boolean(accessibilityCommon.getAutoclickForTest()));
    pref = await this.getPref('settings.a11y.screen_magnifier');
    assertEquals('settings.a11y.screen_magnifier', pref.key);
    assertTrue(pref.value);
    assertTrue(Boolean(accessibilityCommon.getMagnifierForTest()));

    // Then, flip off autoclick and verify all prefs and internal state.
    await this.setPref('settings.a11y.autoclick', false);
    pref = await this.getPref('settings.a11y.autoclick');
    assertEquals('settings.a11y.autoclick', pref.key);
    assertFalse(pref.value);
    assertTrue(!accessibilityCommon.getAutoclickForTest());
    pref = await this.getPref('settings.a11y.screen_magnifier');
    assertEquals('settings.a11y.screen_magnifier', pref.key);
    assertTrue(pref.value);
    assertTrue(Boolean(accessibilityCommon.getMagnifierForTest()));

    // Unfortunately, turning off all features would remove the extension. Flip
    // autoclick back on.
    await this.setPref('settings.a11y.autoclick', true);
    pref = await this.getPref('settings.a11y.autoclick');
    assertEquals('settings.a11y.autoclick', pref.key);
    assertTrue(pref.value);
    assertTrue(Boolean(accessibilityCommon.getAutoclickForTest()));
    pref = await this.getPref('settings.a11y.screen_magnifier');
    assertEquals('settings.a11y.screen_magnifier', pref.key);
    assertTrue(pref.value);
    assertTrue(Boolean(accessibilityCommon.getMagnifierForTest()));

    // And, finally flip screen magnifier off.
    await this.setPref('settings.a11y.screen_magnifier', false);
    pref = await this.getPref('settings.a11y.autoclick');
    assertEquals('settings.a11y.autoclick', pref.key);
    assertTrue(pref.value);
    assertTrue(Boolean(accessibilityCommon.getAutoclickForTest()));
    pref = await this.getPref('settings.a11y.screen_magnifier');
    assertEquals('settings.a11y.screen_magnifier', pref.key);
    assertFalse(pref.value);
    assertTrue(!accessibilityCommon.getMagnifierForTest());
  })();
});

GEN('#include "ui/accessibility/accessibility_features.h"');

/**
 * Accessibility common extension browser tests with enabled GameFace feature.
 */
AccessibilityCommonWithGameFaceEnabledE2ETest =
    class extends AccessibilityCommonE2ETest {
  /** @override */
  get featureList() {
    return {enabled: ['features::kAccessibilityGameFaceIntegration']};
  }
};

TEST_F(
    'AccessibilityCommonWithGameFaceEnabledE2ETest', 'GameFaceEnabled',
    function() {
      this.newCallback(async () => {
        // Then check that GameFace is enabled.
        const enabled = await this.getFeature(
            chrome.accessibilityPrivate.AccessibilityFeature
                .GAME_FACE_INTEGRATION);
        assertTrue(enabled);
        assertTrue(Boolean(accessibilityCommon.getGameFaceForTest()));
      })();
    });
