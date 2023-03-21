// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://color-internals/
 */

GEN('#include "chromeos/constants/chromeos_features.h"');
GEN('#include "content/public/test/browser_test.h"');

const HOST_ORIGIN = 'chrome://color-internals';

// TODO:(crbug.com/1262025): We should avoid using `var`.
//
// js2gtest fixtures require var here (https://crbug.com/1033337).
// eslint-disable-next-line no-var
var ColorInternalsUIBrowserTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return HOST_ORIGIN;
  }

  /** @override */
  get isAsync() {
    return true;
  }
  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kJelly']};
  }
};

// Tests that chrome://color-internals loads successfully.
TEST_F('ColorInternalsUIBrowserTest', 'HasChromeSchemeURL', async () => {
  await import('chrome://webui-test/mojo_webui_test_support.js');
  assertEquals(document.location.origin, HOST_ORIGIN);
  testDone();
});

// Tests that the table body has been constructed properly and has had token
// rows added into it.
TEST_F('ColorInternalsUIBrowserTest', 'BuildsTokenTable', async () => {
  await import('chrome://webui-test/mojo_webui_test_support.js');
  const table = document.querySelector('table');
  assertNotEquals(table.tBodies[0].rows.length, 0);
  testDone();
});

TEST_F('ColorInternalsUIBrowserTest', 'DisplaysWallpaperColors', async () => {
  await import('chrome://webui-test/mojo_webui_test_support.js');
  // Wait for initial load to finish to reduce flakiness.
  await new Promise(async (resolve) => {
    const block = document.getElementById('wallpaper-block');
    while (block.hasAttribute('loading')) {
      await new Promise(resolve => setTimeout(resolve, 10));
    }
    resolve();
  });

  const prominentColorsContainer =
      document.getElementById('wallpaper-prominent-colors-container');
  assertGT(
      prominentColorsContainer.querySelectorAll('.wallpaper-color-container')
          .length,
      1, 'more than 1 wallpaper prominent color should be displayed');

  const kMeanContainer =
      document.getElementById('wallpaper-k-mean-color-container');
  assertEquals(
      kMeanContainer.querySelectorAll('.wallpaper-color-container').length, 1,
      'one k mean color should be displayed');
  testDone();
});
