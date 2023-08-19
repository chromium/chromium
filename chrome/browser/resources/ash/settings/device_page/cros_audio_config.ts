// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Provides singleton access to the CrosAudioConfig mojo interface and the
 * ability to override it with a fake implementation needed for tests.
 */

import {CrosAudioConfig, CrosAudioConfigInterface as CrosAudioConfigMojomInterface} from '../mojom-webui/cros_audio_config.mojom-webui.js';

import {FakeCrosAudioConfig, FakeCrosAudioConfigInterface} from './fake_cros_audio_config.js';

// Type alias to enable use of in-progress api when `getCrosAudioConfig` returns
// a `FakeCrosAudioConfig` and ensure additional check not required when the
// current mojom interface is used.
export type CrosAudioConfigInterface = Required<CrosAudioConfigMojomInterface>&
    Partial<FakeCrosAudioConfigInterface>;
let crosAudioConfig: CrosAudioConfigInterface|null = null;

/** Use FakeCrosAudioConfig implementation in `getCrosAudioConfig`. */
const useFakeMojo = false;

/**
 * The CrosAudioConfig implementation used for testing. Passing null reverses
 * the override.
 */
export function setCrosAudioConfigForTesting(
    testCrosAudioConfig: CrosAudioConfigInterface): void {
  crosAudioConfig = testCrosAudioConfig;
}

export function getCrosAudioConfig(): CrosAudioConfigInterface {
  if (!crosAudioConfig && useFakeMojo) {
    crosAudioConfig = new FakeCrosAudioConfig();
  }

  if (!crosAudioConfig) {
    crosAudioConfig = CrosAudioConfig.getRemote();
  }

  // Base case returns a fake CrosAudioConfig used for testing.
  return crosAudioConfig;
}
