// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview E2E test suite for chrome://personalization.
 */

GEN('#include "ash/webui/personalization_app/test/personalization_app_browsertest_fixture.h"');

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');

const HOST_ORIGIN = 'chrome://personalization';

var PersonalizationAppBrowserTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return HOST_ORIGIN;
  }

  /** @override */
  get featureList() {
    return {enabled: ['ash::features::kWallpaperWebUI']};
  }

  /** @override */
  get isAsync() {
    return true;
  }

  /** @override */
  get runAccessibilityChecks() {
    return false;
  }

  /** @override */
  get typedefCppFixture() {
    return 'PersonalizationAppBrowserTestFixture';
  }
};

// Tests that chrome://personalization runs js file and that it goes
// somewhere instead of 404ing or crashing.
TEST_F('PersonalizationAppBrowserTest', 'HasChromeSchemeURL', () => {
  assertEquals(document.location.origin, HOST_ORIGIN);

  const title = document.querySelector('head > title');
  assertEquals('Wallpaper', title.innerText);
  testDone();
});

TEST_F(
    'PersonalizationAppBrowserTest', 'LoadsCollectionsUntrustedIframe', () => {
      const router = document.querySelector('personalization-router');
      assertTrue(!!router);

      const collections =
          router.shadowRoot.querySelector('wallpaper-collections');
      assertTrue(!!collections);

      const iframe =
          collections.shadowRoot.getElementById('collections-iframe');
      assertTrue(!!iframe);

      assertEquals(
          'chrome-untrusted://personalization/untrusted/collections.html',
          iframe.src);
      testDone();
    });