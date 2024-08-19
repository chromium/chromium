// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  useRecordingDataManager,
} from './lit/context.js';
import {TextToken, Transcription} from './soda/soda.js';
import {navigateTo} from './state/route.js';
import {
  settings,
  SpeakerLabelEnableState,
  SummaryEnableState,
  TranscriptionEnableState,
} from './state/settings.js';
import {assertExists} from './utils/assert.js';

interface ConfigForTest {
  includeSystemAudio: boolean;
  showOnboardingDialog: boolean;
  speakerLabelForceEnabled: boolean;
  summaryForceEnabled: boolean;
  transcriptionForceEnabled: boolean;
}

interface RecordingDataForTest {
  audio: string;
  durationMs: number;
  title: string;
  powers: number[];
  textTokens?: TextToken[];
}

function base64ToBlob(data: string): Blob {
  const mimeType = 'audio/webm;codecs=opus';
  const dataPart = assertExists(data.split(',')[1], 'Invalid audio data');
  const byteCharacters = atob(dataPart);
  const n = byteCharacters.length;
  const byteArray = new Uint8Array(n);
  for (let i = 0; i < n; i++) {
    byteArray[i] = byteCharacters.charCodeAt(i);
  }
  return new Blob([byteArray], {type: mimeType});
}

/**
 * TestHelper is a helper class used by Tast test only.
 */
export class TestHelper {
  /**
   * Redirect to the main page. It is called when the set up is done and ready
   * to start the test.
   */
  static goToMainPage(): void {
    navigateTo('/');
  }

  /**
   * Removes the local settings and recordings.
   */
  static async removeCacheData(): Promise<void> {
    localStorage.clear();
    await useRecordingDataManager().clear();
  }

  /**
   * Configures local settings requested for the test.
   *
   * @param config Configuration sent from the test.
   */
  static configureSettingsForTest(config: ConfigForTest): void {
    settings.mutate((s) => {
      // Pre-set the mic option to include/exclude system audio.
      s.includeSystemAudio = config.includeSystemAudio;
      // Skip onboarding dialog at the start of the test.
      if (!config.showOnboardingDialog) {
        s.onboardingDone = true;
      }
      // Force enable models for testing.
      if (config.speakerLabelForceEnabled) {
        s.speakerLabelEnabled = SpeakerLabelEnableState.ENABLED;
      }
      if (config.summaryForceEnabled) {
        s.summaryEnabled = SummaryEnableState.ENABLED;
      }
      if (config.transcriptionForceEnabled) {
        s.transcriptionEnabled = TranscriptionEnableState.ENABLED;
      }
    });
  }

  /**
   * Imports the recordings sent from Tast side.
   *
   * @param recordings A list of recordings.
   */
  static async importRecordings(
    recordings: RecordingDataForTest[],
  ): Promise<void> {
    for (const data of recordings) {
      const {audio, durationMs, powers, title, textTokens: tokens} = data;
      const blob = base64ToBlob(audio);

      const params = {
        title: title,
        durationMs: durationMs,
        recordedAt: Date.now(),
        powers: powers,
        transcription: tokens !== undefined ? new Transcription(tokens) : null,
      };
      await useRecordingDataManager().createRecording(params, blob);
    }
  }
}
