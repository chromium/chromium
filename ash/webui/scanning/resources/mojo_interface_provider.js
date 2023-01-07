// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';
import './accessibility_features.mojom-lite.js';
import './scanning.mojom-lite.js';

/** @type {?ash.scanning.mojom.ScanServiceInterface} */
let scanService = null;

/** @type {?ash.common.mojom.AccessibilityFeaturesInterface} */
let accessibilityFeatures = null;

/** @param {!ash.scanning.mojom.ScanServiceInterface} testScanService */
export function setScanServiceForTesting(testScanService) {
  scanService = testScanService;
}

/** @return {!ash.scanning.mojom.ScanServiceInterface} */
export function getScanService() {
  if (scanService) {
    return scanService;
  }

  scanService = ash.scanning.mojom.ScanService.getRemote();
  return scanService;
}

/**
 * @param {!ash.common.mojom.AccessibilityFeaturesInterface}
 *     testAccessibilityInterface
 */
export function setAccessibilityFeaturesForTesting(testAccessibilityInterface) {
  accessibilityFeatures = testAccessibilityInterface;
}

/** @return {!ash.common.mojom.AccessibilityFeaturesInterface} */
export function getAccessibilityFeaturesInterface() {
  if (accessibilityFeatures) {
    return accessibilityFeatures;
  }

  accessibilityFeatures = ash.common.mojom.AccessibilityFeatures.getRemote();
  return accessibilityFeatures;
}
