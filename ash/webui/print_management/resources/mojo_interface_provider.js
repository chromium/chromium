// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-lite.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
import './printing_manager.mojom-lite.js';

/**
 * @type {
 *    ?chromeos.printing.printingManager.mojom.PrintingMetadataProviderInterface
 * }
 */
let metadataProvider = null;

/**
 * @param {
 *    !chromeos.printing.printingManager.mojom.PrintingMetadataProviderInterface
 * } testProvider
 */
export function setMetadataProviderForTesting(testProvider) {
  metadataProvider = testProvider;
}

/**
 * @return {
 *    !chromeos.printing.printingManager.mojom.PrintingMetadataProviderInterface
 * }
 */
export function getMetadataProvider() {
  if (metadataProvider) {
    return metadataProvider;
  }
  metadataProvider = chromeos.printing.printingManager.mojom
      .PrintingMetadataProvider.getRemote();
  return metadataProvider;
}
