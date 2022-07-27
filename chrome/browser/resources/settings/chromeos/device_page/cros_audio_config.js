// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrosAudioConfig, CrosAudioConfigInterface} from '../../mojom-webui/audio/cros_audio_config.mojom-webui.js';

/**
 * @fileoverview
 * Provides singleton access to the CrosAudioConfig mojo interface and the
 * ability to override it with a fake implementation needed for tests.
 */

/** @type {?CrosAudioConfigInterface} */
let crosAudioConfig = null;

/**
 * @param {?CrosAudioConfigInterface} testCrosAudioConfig
 * The CrosAudioConfig implementation used for testing. Passing null reverses
 * the override.
 */
export function setCrosAudioConfigForTesting(testCrosAudioConfig) {
  crosAudioConfig = testCrosAudioConfig;
}

/** @return {!CrosAudioConfigInterface} */
export function getCrosAudioConfig() {
  if (!crosAudioConfig) {
    crosAudioConfig = CrosAudioConfig.getRemote();
  }

  // Base case returns a fake CrosAudioConfig used for testing.
  return crosAudioConfig;
}
