// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../common/testing/e2e_test_base.js']);
GEN_INCLUDE(['mock_mojo_private.js']);

/**
 * Test fixture for Enhanced Network TTS E2E tests.
 */
EnhancedNetworkTE2ETestBase = class extends E2ETestBase {
  /** @override */
  constructor() {
    super();
    this.mockMojoPrivate = MockMojoPrivate;
    chrome.mojoPrivate = this.mockMojoPrivate;
    this.onSpeakWithAudioStreamListeners = [];
    this.onStopListeners = [];
    chrome.ttsEngine = {
      onSpeakWithAudioStream: {
        addListener: callback => {
          this.onSpeakWithAudioStreamListeners.push(callback);
        },
      },
      onStop: {
        addListener: callback => {
          this.onStopListeners.push(callback);
        },
      },
    };
  }

  /** @override */
  testGenPreamble() {
    super.testGenPreamble();
    GEN(`
    base::OnceClosure load_cb =
        base::BindOnce(
            &ash::AccessibilityManager::LoadEnhancedNetworkTtsForTest,
            base::Unretained(ash::AccessibilityManager::Get()));
    `);
  }
};
