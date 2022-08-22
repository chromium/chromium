// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PrintingMetadataProvider, PrintingMetadataProviderInterface} from './printing_manager.mojom-webui.js';

/**
 * @type {?PrintingMetadataProviderInterface}
 */
let metadataProvider = null;

/**
 * @param {!PrintingMetadataProviderInterface} testProvider
 */
export function setMetadataProviderForTesting(testProvider) {
  metadataProvider = testProvider;
}

/**
 * @return {!PrintingMetadataProviderInterface}
 */
export function getMetadataProvider() {
  if (metadataProvider) {
    return metadataProvider;
  }
  metadataProvider = PrintingMetadataProvider.getRemote();
  return metadataProvider;
}
