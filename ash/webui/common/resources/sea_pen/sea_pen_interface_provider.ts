// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview a singleton getter for the SeaPen mojom interface used in
 * the Personalization SWA. Also contains utility function for mocking out the
 * implementation for testing.
 */

import 'chrome://resources/mojo/mojo/public/js/bindings.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {SeaPenProvider, SeaPenProviderInterface} from './sea_pen.mojom-webui.js';

let seaPenProvider: SeaPenProviderInterface|null = null;

export function setSeaPenProviderForTesting(
    testProvider: SeaPenProviderInterface): void {
  seaPenProvider = testProvider;
}

/** Returns a singleton for the WallpaperProvider mojom interface. */
export function getSeaPenProvider(): SeaPenProviderInterface {
  if (!seaPenProvider) {
    seaPenProvider = SeaPenProvider.getRemote();
  }
  return seaPenProvider;
}
