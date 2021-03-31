// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';
import './scanning.mojom-lite.js';

/** @type {?chromeos.scanning.mojom.ScanServiceInterface} */
let scanService = null;

/** @param {!chromeos.scanning.mojom.ScanServiceInterface} testScanService */
export function setScanServiceForTesting(testScanService) {
  scanService = testScanService;
}

/** @return {!chromeos.scanning.mojom.ScanServiceInterface} */
export function getScanService() {
  if (scanService) {
    return scanService;
  }

  scanService = chromeos.scanning.mojom.ScanService.getRemote();
  return scanService;
}
