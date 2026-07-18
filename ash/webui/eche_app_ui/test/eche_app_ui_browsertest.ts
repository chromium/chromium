// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://eche-app.
 */

import {assertEquals} from 'chrome://webui-test/chai_assert.js';

const HOST_ORIGIN = 'chrome://eche-app';
const GUEST_ORIGIN = 'chrome-untrusted://eche-app';

suite('EcheAppUIBrowserTest', () => {
  // Tests that chrome://eche-app goes somewhere instead of 404ing or crashing.
  test('HasChromeSchemeURL', () => {
    assertEquals(document.title, 'Eche');
    assertEquals(document.location.origin, HOST_ORIGIN);
  });

  // Tests that chrome://eche-app is allowed to frame
  // chrome-untrusted://eche-app.
  test('GuestCanLoad', () => {
    const guest = document.querySelector('iframe')!;
    assertEquals(document.location.origin, HOST_ORIGIN);
    assertEquals(guest.src, GUEST_ORIGIN + '/untrusted_index.html');
  });
});
