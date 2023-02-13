// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview a singleton getter for the ambient mojom interface used in
 * the Personalization SWA. Also contains utility functions around fetching
 * mojom data and mocking out the implementation for testing.
 */

import 'chrome://resources/mojo/mojo/public/js/bindings.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {AmbientProvider, AmbientProviderInterface} from '../../personalization_app.mojom-webui.js';

let ambientProvider: AmbientProviderInterface|null = null;

export function setAmbientProviderForTesting(
    testProvider: AmbientProviderInterface): void {
  ambientProvider = testProvider;
}

/** Returns a singleton for the AmbientProvider mojom interface. */
export function getAmbientProvider(): AmbientProviderInterface {
  if (!ambientProvider) {
    ambientProvider = AmbientProvider.getRemote();
  }
  return ambientProvider;
}
