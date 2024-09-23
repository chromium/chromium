// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/chromevox_e2e_test_base.js']);

GEN_INCLUDE(['../testing/mock_feedback.js', '../testing/fake_objects.js']);

/**
 * Test fixture involving settings pages.
 */
ChromeVoxSettingsPagesTest = class extends ChromeVoxE2ETest {
  /** @override */
  testGenCppIncludes() {
    super.testGenCppIncludes();
    GEN(`
      #include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
    `);
  }

  /** @override */
  testGenPreamble() {
    GEN(`
    ash::SystemWebAppManager::GetForTest(GetProfile())
        ->InstallSystemAppsForTesting();
  `);
    super.testGenPreamble();
  }
};

AX_TEST_F(
    'ChromeVoxSettingsPagesTest', 'TtsRateCommandOnSettingsPage',
    async function() {
      const realTts = ChromeVox.tts;
      const mockFeedback = this.createMockFeedback();
      await this.runWithLoadedTree(`unused`);
      const increaseRate = realTts.increaseOrDecreaseProperty.bind(
          realTts, TtsSettings.RATE, true);
      const decreaseRate = realTts.increaseOrDecreaseProperty.bind(
          realTts, TtsSettings.RATE, false);

      mockFeedback.call(doCmd('showTtsSettings'))
          .expectSpeech(
              /(Settings)|(Text-to-Speech voice settings subpage back button)/)

          // ChromeVox presents a 0% to 100% scale.
          // Ensure we have the default rate.
          .call(
              () => chrome.settingsPrivate.setPref(
                  'settings.tts.speech_rate', 1.0))

          .call(increaseRate)
          .expectSpeech('Rate 19 percent')
          .call(increaseRate)
          .expectSpeech('Rate 21 percent')

          // Speed things up...
          .call(
              () => chrome.settingsPrivate.setPref(
                  'settings.tts.speech_rate', 4.9))
          .expectSpeech('Rate 98 percent')
          .call(increaseRate)
          .expectSpeech('Rate 100 percent')

          .call(decreaseRate)
          .expectSpeech('Rate 98 percent')
          .call(decreaseRate)
          .expectSpeech('Rate 96 percent')

          // Slow things down...
          .call(
              () => chrome.settingsPrivate.setPref(
                  'settings.tts.speech_rate', 0.3))
          .expectSpeech('Rate 2 percent')
          .call(decreaseRate)
          .expectSpeech('Rate 0 percent')

          .call(increaseRate)
          .expectSpeech('Rate 2 percent')
          .call(increaseRate)
          .expectSpeech('Rate 4 percent');

      await mockFeedback.replay();
    });
