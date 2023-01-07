// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://system-extensions-internals/
 */

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');

const HOST_ORIGIN = 'chrome://system-extensions-internals';

// TODO:(crbug.com/1262025): We should avoid using `var`.
//
// js2gtest fixtures require var here (https://crbug.com/1033337).
// eslint-disable-next-line no-var
var SystemExtensionsInternalsUIBrowserTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return HOST_ORIGIN;
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'ash::features::kSystemExtensions',
      ],
    };
  }

  /** @override */
  get isAsync() {
    return true;
  }
};

// Tests that chrome://system-extensions-internals loads successfully.
TEST_F(
    'SystemExtensionsInternalsUIBrowserTest', 'HasChromeSchemeURL',
    async () => {
      const header = document.querySelector('title');
      assertEquals(header.innerText, 'System Extensions Internals');
      assertEquals(document.location.origin, HOST_ORIGIN);
      testDone();
    });
