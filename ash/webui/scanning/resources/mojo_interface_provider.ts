// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AccessibilityFeatures, AccessibilityFeaturesInterface} from './accessibility_features.mojom-webui.js';
import {ScanService, ScanServiceInterface} from './scanning.mojom-webui.js';

let scanService: ScanServiceInterface|null = null;

let accessibilityFeatures: AccessibilityFeaturesInterface|null = null;

export function setScanServiceForTesting(
    testScanService: ScanServiceInterface) {
  scanService = testScanService;
}

export function getScanService(): ScanServiceInterface {
  if (scanService) {
    return scanService;
  }

  scanService = ScanService.getRemote();
  return scanService;
}

export function setAccessibilityFeaturesForTesting(
    testAccessibilityInterface: AccessibilityFeaturesInterface) {
  accessibilityFeatures = testAccessibilityInterface;
}

export function getAccessibilityFeaturesInterface():
    AccessibilityFeaturesInterface {
  if (accessibilityFeatures) {
    return accessibilityFeatures;
  }

  accessibilityFeatures = AccessibilityFeatures.getRemote();
  return accessibilityFeatures;
}
