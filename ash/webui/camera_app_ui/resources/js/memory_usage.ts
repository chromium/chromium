// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from './assert.js';
import {measureUntrustedScriptsMemory} from './untrusted_scripts.js';

export interface CCAMemoryMeasurement {
  main: MemoryMeasurement;
  untrusted: MemoryMeasurement;
}

/**
 * Measures memory usage from trusted and untrusted frames.
 */
export async function memoryAppMemoryUsage(): Promise<CCAMemoryMeasurement> {
  assert(self.crossOriginIsolated);
  const usages = await Promise.all([
    performance.measureUserAgentSpecificMemory(),
    measureUntrustedScriptsMemory(),
  ]);
  return {
    main: usages[0],
    untrusted: usages[1],
  };
}
