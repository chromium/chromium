// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview E2E test suite for chrome://personalization.
 */

GEN('#include "ash/webui/personalization_app/test/personalization_app_browsertest_fixture.h"');

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');

const ROOT_PAGE = 'chrome://personalization/';
const WALLPAPER_SUBPAGE = 'chrome://personalization/wallpaper/';

class PersonalizationAppBrowserTest extends testing.Test {
  /** @override */
  get browsePreload() {
    return ROOT_PAGE;
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'ash::features::kWallpaperWebUI', 'ash::features::kPersonalizationHub'
      ]
    };
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
}

// TODO(crbug/1262025) revisit this workaround for js2gtest requiring "var"
// declarations.
this[PersonalizationAppBrowserTest.name] = PersonalizationAppBrowserTest;

// Tests that chrome://personalization loads the page without javascript errors
// or a 404 or crash. Should display some text.
TEST_F('PersonalizationAppBrowserTest', 'HasRootPageUrl', () => {
  assertEquals(document.location.href, ROOT_PAGE);
  assertEquals(
      'Personalization',
      document.querySelector('personalization-router')
          .shadowRoot.querySelector('h1')
          .innerText);
  testDone();
});

class WallpaperSubpageBrowserTest extends PersonalizationAppBrowserTest {
  /** @override */
  get browsePreload() {
    return WALLPAPER_SUBPAGE;
  }
}

// TODO(crbug/1262025) revisit this workaround for js2gtest requiring "var"
// declarations.
this[WallpaperSubpageBrowserTest.name] = WallpaperSubpageBrowserTest;

// Tests that chrome://personalization/wallpaper runs js file and that it goes
// somewhere instead of 404ing or crashing.
TEST_F('WallpaperSubpageBrowserTest', 'HasWallpaperSubpageUrl', () => {
  assertEquals(document.location.href, WALLPAPER_SUBPAGE);

  const title = document.querySelector('head > title');
  assertEquals('Wallpaper', title.innerText);
  testDone();
});

TEST_F('WallpaperSubpageBrowserTest', 'LoadsCollectionsUntrustedIframe', () => {
  const router = document.querySelector('personalization-router');
  assertTrue(!!router);

  const collections = router.shadowRoot.querySelector('wallpaper-collections');
  assertTrue(!!collections);

  const iframe = collections.shadowRoot.getElementById('collections-iframe');
  assertTrue(!!iframe);

  assertEquals(
      'chrome-untrusted://personalization/untrusted/collections.html',
      iframe.src);
  testDone();
});
