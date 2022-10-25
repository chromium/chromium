// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview E2E test suite for chrome://personalization.
 */

GEN('#include "ash/webui/personalization_app/test/personalization_app_browsertest_fixture.h"');

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "ash/public/cpp/ambient/ambient_client.h"');
GEN('#include "chromeos/constants/chromeos_features.h"');
GEN('#include "content/public/test/browser_test.h"');

const ROOT_PAGE = 'chrome://personalization/';
const WALLPAPER_SUBPAGE = 'chrome://personalization/wallpaper';

class PersonalizationAppBrowserTest extends testing.Test {
  /** @override */
  get browsePreload() {
    return ROOT_PAGE;
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'chromeos::features::kDarkLightMode',
      ],
    };
  }

  /** @override */
  get isAsync() {
    return true;
  }

  /** @override */
  get typedefCppFixture() {
    return 'ash::personalization_app::PersonalizationAppBrowserTestFixture';
  }
}

/**
 * Wait until |func| returns true.
 * If |timeoutMs| milliseconds elapse, will reject with |message|.
 * Polls every |intervalMs| milliseconds.
 */
function waitUntil(func, message, intervalMs = 50, timeoutMs = 1001) {
  let rejectTimer = null;
  let pollTimer = null;

  function cleanup() {
    if (rejectTimer) {
      window.clearTimeout(rejectTimer);
    }
    if (pollTimer) {
      window.clearInterval(pollTimer);
    }
  }

  return new Promise((resolve, reject) => {
    rejectTimer = window.setTimeout(() => {
      cleanup();
      reject(message);
    }, timeoutMs);

    pollTimer = window.setInterval(() => {
      if (func()) {
        cleanup();
        resolve();
      }
    }, intervalMs);
  });
}

// TODO(crbug/1262025) revisit this workaround for js2gtest requiring "var"
// declarations.
this[PersonalizationAppBrowserTest.name] = PersonalizationAppBrowserTest;

// Tests that chrome://personalization loads the page without javascript errors
// or a 404 or crash. Displays user preview, wallpaper preview, and ambient
// preview
TEST_F('PersonalizationAppBrowserTest', 'HasRootPageUrl', () => {
  assertEquals(document.location.href, ROOT_PAGE);
  const userPreview = document.querySelector('personalization-router')
                          .shadowRoot.querySelector('personalization-main')
                          .shadowRoot.querySelector('user-preview');
  const wallpaperPreview = document.querySelector('personalization-router')
                               .shadowRoot.querySelector('personalization-main')
                               .shadowRoot.querySelector('wallpaper-preview');
  assertTrue(!!userPreview);
  assertTrue(!!wallpaperPreview);
  testDone();
});

TEST_F('PersonalizationAppBrowserTest', 'ShowsThemeButtons', async () => {
  const theme = document.querySelector('personalization-router')
                    .shadowRoot.querySelector('personalization-main')
                    .shadowRoot.querySelector('personalization-theme');
  await waitUntil(
      () => theme.shadowRoot.getElementById('lightMode'),
      'failed to find light button');
  const lightButton = theme.shadowRoot.getElementById('lightMode');
  assertTrue(!!lightButton);
  assertEquals(lightButton.getAttribute('aria-pressed'), 'true');
  const darkButton = theme.shadowRoot.getElementById('darkMode');
  assertTrue(!!darkButton);
  assertEquals(darkButton.getAttribute('aria-pressed'), 'false');
  testDone();
});

TEST_F('PersonalizationAppBrowserTest', 'ShowsUserInfo', async () => {
  const preview = document.querySelector('personalization-router')
                      .shadowRoot.querySelector('personalization-main')
                      .shadowRoot.querySelector('user-preview');

  await waitUntil(
      () => preview.shadowRoot.getElementById('email'),
      'failed to find user email');
  assertEquals(
      'fake-email', preview.shadowRoot.getElementById('email').innerText);
  assertEquals(
      'Fake Name', preview.shadowRoot.getElementById('name').innerText);
  testDone();
});

class PersonalizationAppAmbientModeAllowedBrowserTest extends
    PersonalizationAppBrowserTest {
  /** @override */
  get testGenPreamble() {
    return () => {
      GEN('ash::AmbientClient::Get()->SetAmbientModeAllowedForTesting(true);');
    };
  }
}

this[PersonalizationAppAmbientModeAllowedBrowserTest.name] =
    PersonalizationAppAmbientModeAllowedBrowserTest;

TEST_F(
    'PersonalizationAppAmbientModeAllowedBrowserTest', 'ShowsAmbientPreview',
    () => {
      const preview = document.querySelector('personalization-router')
                          .shadowRoot.querySelector('personalization-main')
                          .shadowRoot.querySelector('ambient-preview');
      assertTrue(!!preview);
      testDone();
    });

TEST_F(
    'PersonalizationAppAmbientModeAllowedBrowserTest',
    'ShowsAmbientSubpageLink', () => {
      const ambientSubpageLink =
          document.querySelector('personalization-router')
              .shadowRoot.querySelector('personalization-main')
              .shadowRoot.querySelector('#ambientSubpageLink');
      assertTrue(!!ambientSubpageLink);
      testDone();
    });

class PersonalizationAppAmbientModeDisallowedBrowserTest extends
    PersonalizationAppBrowserTest {
  /** @override */
  get testGenPreamble() {
    return () => {
      GEN('ash::AmbientClient::Get()->SetAmbientModeAllowedForTesting(false);');
    };
  }
}

this[PersonalizationAppAmbientModeDisallowedBrowserTest.name] =
    PersonalizationAppAmbientModeDisallowedBrowserTest;

TEST_F(
    'PersonalizationAppAmbientModeDisallowedBrowserTest',
    'NotShowAmbientPreview', () => {
      const preview = document.querySelector('personalization-router')
                          .shadowRoot.querySelector('personalization-main')
                          .shadowRoot.querySelector('ambient-preview');
      assertFalse(!!preview);
      testDone();
    });

TEST_F(
    'PersonalizationAppAmbientModeDisallowedBrowserTest',
    'NotShowAmbientSubpageLink', () => {
      const ambientSubpageLink =
          document.querySelector('personalization-router')
              .shadowRoot.querySelector('personalization-main')
              .shadowRoot.querySelector('#ambientSubpageLink');
      assertFalse(!!ambientSubpageLink);
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

TEST_F('WallpaperSubpageBrowserTest', 'LoadsCollectionsGrid', () => {
  const router = document.querySelector('personalization-router');
  assertTrue(!!router, 'personalization-router should be top level element');

  const wallpaperSubpage = router.shadowRoot.querySelector('wallpaper-subpage');
  assertTrue(
      !!wallpaperSubpage,
      'wallpaper-subpage should be found under personalization-router');

  const collections =
      wallpaperSubpage.shadowRoot.querySelector('wallpaper-collections');
  assertTrue(
      !!collections,
      'wallpaper-collections should be found under wallpaper-subpage');

  assertFalse(
      collections.parentElement.hidden, 'parent element should be visible');
  assertGT(
      collections.offsetWidth, 0,
      'wallpaper-collections should have visible width');
  assertGT(
      collections.offsetHeight, 0,
      'wallpaper-collections grid should have visible height');
  testDone();
});
