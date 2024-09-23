// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RecorderApp} from '../pages/recorder-app.js';

import {usePlatformHandler, useRecordingDataManager} from './lit/context.js';
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
    navigateTo('main');
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

  /**
   * Installs the model used for transcription.
   */
  static installTranscriptionModel(): void {
    usePlatformHandler().installSoda();
  }

  /**
   * Returns whether the transcription model is installed.
   *
   * @return Boolean indicating if the transcription model is installed.
   */
  static isTranscriptionModelInstalled(): boolean {
    const state = usePlatformHandler().sodaState.value;
    return state.kind === 'installed';
  }

  /**
   * Installs the model used for summarize recordings.
   */
  static installSummaryModel(): void {
    usePlatformHandler().summaryModelLoader.download();
  }

  /**
   * Returns whether the summary model is installed.
   *
   * @return Boolean indicating if the summary model is installed.
   */
  static isSummaryModelInstalled(): boolean {
    const state = usePlatformHandler().summaryModelLoader.state.value;
    return state.kind === 'installed';
  }

  /**
   * Installs the model used for suggest recording titles.
   */
  static installTitleSuggestionModel(): void {
    usePlatformHandler().titleSuggestionModelLoader.download();
  }

  /**
   * Returns whether the title suggestion model is installed.
   *
   * @return Boolean indicating if the title suggestion model is installed.
   */
  static isTitleSuggestionModelInstalled(): boolean {
    const state = usePlatformHandler().titleSuggestionModelLoader.state.value;
    return state.kind === 'installed';
  }

  // UI-related functions.
  /**
   * Returns the UI from the given key, throws an error if not exists.
   *
   * @param key UI key listed in `UI_COMPONENTS` object.
   * @return The resolved UI element.
   */
  static resolveComponent(key: ComponentKey): Element {
    return UI_COMPONENTS[key]();
  }

  /**
   * Returns the number of the recording files in the main page.
   *
   * @return Number of recording files in the main Page.
   */
  static getRecordingFileCount(): number {
    return app().mainPage.recordingFileListForTest.recordingFileCountForTest();
  }

  /**
   * Returns the title suggestion given the index.
   *
   * @param index Zero-based index.
   * @return An element of the n-th title suggestion.
   */
  static getNthSuggestedTitle(index: number): Element {
    return app()
      .playbackPageForTest.recordingTitleForTest.titleSuggestionForTest
      .nthSuggestedTitleForTest(
        index,
      );
  }

  /**
   * Returns the summary shown in the playback page.
   *
   * @return A string containing the recording summary, may contains `\n`.
   */
  static getSummaryContent(): string {
    return app()
      .playbackPageForTest.summarizationViewForTest.getSummaryContentForTest();
  }
}

/**
 * Returns the `RecorderApp` queried from the document.
 *
 * @return `RecorderApp` object.
 */
function app(): RecorderApp {
  return assertExists(document.querySelector('recorder-app'));
}

// TODO: b/361015174 - Simplify the approach to access UI components.
const UI_COMPONENTS = {
  firstSuggestedTitle: () =>
    app()
      .playbackPageForTest.recordingTitleForTest.titleSuggestionForTest
      .firstSuggestedTitleForTest,
  firstRecordingCard: () => app()
                              .mainPage.recordingFileListForTest
                              .firstRecordingForTest.recordingCardForTest,
  mainPage: () => app().mainPage,
  playbackBackButton: () => app().playbackPageForTest.backButtonForTest,
  playbackPage: () => app().playbackPageForTest,
  playbackPauseButton: () => app().playbackPageForTest.pauseButtonForTest,
  playbackRecordingTitle: () => app().playbackPageForTest.recordingTitleForTest,
  playbackTranscriptionToggleButton: () =>
    app().playbackPageForTest.transcriptionToggleButtonForTest,
  recordPage: () => app().recordPageForTest,
  renameTitleText: () =>
    app().playbackPageForTest.recordingTitleForTest.renameContainerForTest,
  suggestTitleButton: () =>
    app().playbackPageForTest.recordingTitleForTest.suggestTitleButtonForTest,
  summaryContainer: () =>
    app().playbackPageForTest.summarizationViewForTest.summaryContainerForTest,
  startRecordingButton: () => app().mainPage.startRecordingButtonForTest,
  stopRecordingButton: () => app().recordPageForTest.stopRecordingButtonForTest,
  // TODO: b/355374546 - Add back the toggleSummary UI component.
  // Currently tast side doesn't use the toggleSummaryButton component, and
  // directly selects the inner cros-icon-button in cros-accordion-item, but we
  // might only want to expose the cros-accordion-item and click on it.
};
type ComponentKey = keyof typeof UI_COMPONENTS;
