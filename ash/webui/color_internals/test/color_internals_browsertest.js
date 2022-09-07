// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://color-internals/
 */

GEN('#include "ash/constants/ash_features.h"');
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
};

// Tests that chrome://color-internals loads successfully.
TEST_F('ColorInternalsUIBrowserTest', 'HasChromeSchemeURL', async () => {
  assertEquals(document.location.origin, HOST_ORIGIN);
  testDone();
});

// Tests that the table body has been constructed properly and has had token
// rows added into it.
TEST_F('ColorInternalsUIBrowserTest', 'BuildsTokenTable', async () => {
  const table = document.querySelector('table');
  assertNotEquals(table.tBodies[0].rows.length, 0);
  testDone();
});
