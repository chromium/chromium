// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Provides singleton access to the CrosAudioConfig mojo interface and the
 * ability to override it with a fake implementation needed for tests.
 */

import {assert} from 'chrome://resources/js/assert_ts.js';

import {CrosAudioConfig, CrosAudioConfigInterface} from '../../mojom-webui/audio/cros_audio_config.mojom-webui.js';

import {FakeCrosAudioConfig} from './fake_cros_audio_config.js';

let crosAudioConfig: CrosAudioConfigInterface|null = null;

/** Use FakeCrosAudioConfig implementation in `getCrosAudioConfig`. */
// TODO(b/260277007): When mojo is stable set useFakeMojo to false and remove
// assert on `useFakeMojo` value.
const useFakeMojo = true;

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
  assert(useFakeMojo, '`useFakeMojo` should be false until mojo is stable.');
  if (!crosAudioConfig) {
    crosAudioConfig = CrosAudioConfig.getRemote();
  }

  // Base case returns a fake CrosAudioConfig used for testing.
  return crosAudioConfig;
}
