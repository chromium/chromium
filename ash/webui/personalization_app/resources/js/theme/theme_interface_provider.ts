// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview a singleton getter for the theme mojom interface used in
 * the Personalization SWA. Also contains utility functions around fetching
 * mojom data and mocking out the implementation for testing.
 */

import 'chrome://resources/mojo/mojo/public/js/bindings.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {ThemeProvider, ThemeProviderInterface} from '../../personalization_app.mojom-webui.js';

let themeProvider: ThemeProviderInterface|null = null;

export function setThemeProviderForTesting(
    testProvider: ThemeProviderInterface): void {
  themeProvider = testProvider;
}

/** Returns a singleton for the ThemeProvider mojom interface. */
export function getThemeProvider(): ThemeProviderInterface {
  if (!themeProvider) {
    themeProvider = ThemeProvider.getRemote();
  }
  return themeProvider;
}
