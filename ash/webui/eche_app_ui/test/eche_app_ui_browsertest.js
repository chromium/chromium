// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://eche-app.
 */

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');

const HOST_ORIGIN = 'chrome://eche-app';
const GUEST_ORIGIN = 'chrome-untrusted://eche-app';

// js2gtest fixtures require var here (https://crbug.com/1033337).
// eslint-disable-next-line no-var
var EcheAppUIBrowserTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return HOST_ORIGIN;
  }

  /** @override */
  get featureList() {
    return {enabled: ['ash::features::kEcheSWA']};
  }
};

/** @return {!HTMLIFrameElement} */
function queryIFrame() {
  return /** @type{!HTMLIFrameElement} */ (document.querySelector('iframe'));
}

// Tests that chrome://eche-app goes somewhere instead of
// 404ing or crashing.
TEST_F('EcheAppUIBrowserTest', 'HasChromeSchemeURL', async () => {
  const {assertEquals} = await import('chrome://webui-test/chai_assert.js');
  assertEquals(document.title, 'Eche');
  assertEquals(document.location.origin, HOST_ORIGIN);
  testDone();
});

// Tests that chrome://eche-app is allowed to frame
// chrome-untrusted://eche-app. The URL is set in the html. If that URL can't
// load, test this fails like JS ERROR: "Refused to frame '...' because it
// violates the following Content Security Policy directive: "frame-src
// chrome-untrusted://eche-app/". This test also fails if the guest renderer is
// terminated, e.g., due to webui performing bad IPC such as network requests
// (failure detected in content/public/test/no_renderer_crashes_assertion.cc).
// Flaky. See crbug.com/1242355,
TEST_F('EcheAppUIBrowserTest', 'GuestCanLoad', async () => {
  const {assertEquals} = await import('chrome://webui-test/chai_assert.js');
  const guest = queryIFrame();

  assertEquals(document.location.origin, HOST_ORIGIN);
  assertEquals(guest.src, GUEST_ORIGIN + '/untrusted_index.html');

  testDone();
});
