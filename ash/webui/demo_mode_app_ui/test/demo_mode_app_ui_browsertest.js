// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');

const HOST_ORIGIN = 'chrome://demo-mode-app';

// js2gtest fixtures require var here (https://crbug.com/1033337).
// eslint-disable-next-line no-var
var DemoModeAppUIBrowserTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return HOST_ORIGIN;
  }

  /** @override */
  get featureList() {
    return {enabled: ['ash::features::kDemoModeSWA']};
  }
};

// Tests that chrome://demo-mode-app runs js file and that it goes
// somewhere instead of 404ing or crashing.
TEST_F('DemoModeAppUIBrowserTest', 'HasChromeSchemeURL', () => {
  const header = document.querySelector('h1');

  assertEquals(header.innerText, 'Demo Mode App');
  assertEquals(document.location.origin, HOST_ORIGIN);
});
