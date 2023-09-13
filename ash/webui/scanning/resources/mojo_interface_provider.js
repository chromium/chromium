// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AccessibilityFeatures, AccessibilityFeaturesInterface} from './accessibility_features.mojom-webui.js';
import {ScanService, ScanServiceInterface} from './scanning.mojom-webui.js';

/** @type {?ScanServiceInterface} */
let scanService = null;

/** @type {?AccessibilityFeaturesInterface} */
let accessibilityFeatures = null;

/** @param {!ScanServiceInterface} testScanService */
export function setScanServiceForTesting(testScanService) {
  scanService = testScanService;
}

/** @return {!ScanServiceInterface} */
export function getScanService() {
  if (scanService) {
    return scanService;
  }

  scanService = ScanService.getRemote();
  return scanService;
}

/**
 * @param {!AccessibilityFeaturesInterface}
 *     testAccessibilityInterface
 */
export function setAccessibilityFeaturesForTesting(testAccessibilityInterface) {
  accessibilityFeatures = testAccessibilityInterface;
}

/** @return {!AccessibilityFeaturesInterface} */
export function getAccessibilityFeaturesInterface() {
  if (accessibilityFeatures) {
    return accessibilityFeatures;
  }

  accessibilityFeatures = AccessibilityFeatures.getRemote();
  return accessibilityFeatures;
}
