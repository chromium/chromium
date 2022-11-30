// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrosHotspotConfig, CrosHotspotConfigInterface} from './cros_hotspot_config.mojom-webui.js';

/**
 * @fileoverview
 * Wrapper for CrosHotspotConfig that provides the ability to inject a fake
 * CrosHotspotConfig implementation for tests.
 */

let hotspotConfig: CrosHotspotConfigInterface|null = null;

/**
 * The CrosHotspotConfig implementation used for testing. Passing null reverses
 * the override.
 */
export function setHotspotConfigForTesting(
    testHotspotConfig: CrosHotspotConfigInterface): void {
  hotspotConfig = testHotspotConfig;
}

export function getHotspotConfig(): CrosHotspotConfigInterface {
  if (hotspotConfig) {
    return hotspotConfig;
  }

  hotspotConfig = CrosHotspotConfig.getRemote();
  return hotspotConfig;
}