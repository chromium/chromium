// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

enum UmaName {
  NEW_PAGE = 'Accessibility.ReadAnything.NewPage',
  LANGUAGE = 'Accessibility.ReadAnything.ReadAloud.Language',
  VOICE = 'Accessibility.ReadAnything.ReadAloud.Voice',
  TEXT_SETTINGS_CHANGE = 'Accessibility.ReadAnything.SettingsChange',
  HIGHLIGHT_STATE = 'Accessibility.ReadAnything.ReadAloud.HighlightState',
  VOICE_SPEED = 'Accessibility.ReadAnything.ReadAloud.VoiceSpeed',
  SPEECH_SETTINGS_CHANGE =
      'Accessibility.ReadAnything.ReadAloud.SettingsChange',
  SPEECH_PLAYBACK = 'Accessibility.ReadAnything.SpeechPlaybackSession',
  SPEECH_ERROR = 'Accessibility.ReadAnything.SpeechError',
}

// Enum for logging when we play speech on a page.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ReadAnythingNewPage)
export enum ReadAnythingNewPage {
  NEW_PAGE = 0,
  SPEECH_PLAYED_ON_NEW_PAGE = 1,

  // Must be last.
  COUNT = 2,
}
// LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:ReadAnythingNewPage)

// Enum for logging which kind of voice is being used to read aloud.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ReadAnythingVoiceType)
export enum ReadAnythingVoiceType {
  NATURAL = 0,
  ESPEAK = 1,
  CHROMEOS = 2,

  // Must be last.
  COUNT = 3,
}
// LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:ReadAnythingReadAloudVoice)

// Enum for logging when a text style setting is changed.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ReadAnythingSettingsChange)
export enum ReadAnythingSettingsChange {
  FONT_CHANGE = 0,
  FONT_SIZE_CHANGE = 1,
  THEME_CHANGE = 2,
  LINE_HEIGHT_CHANGE = 3,
  LETTER_SPACING_CHANGE = 4,
  LINKS_ENABLED_CHANGE = 5,
  IMAGES_ENABLED_CHANGE = 6,

  // Must be last.
  COUNT = 7,
}
// LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:ReadAnythingSettingsChange)

// Enum for logging the reading highlight state.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ReadAloudHighlightState)
export enum ReadAloudHighlightState {
  HIGHLIGHT_ON = 0,
  HIGHLIGHT_OFF = 1,

  // Must be last.
  COUNT = 2,
}
// LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:ReadAnythingHighlightState)

// Enum for logging when a read aloud speech setting is changed.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ReadAloudSettingsChange)
export enum ReadAloudSettingsChange {
  VOICE_SPEED_CHANGE = 0,
  VOICE_NAME_CHANGE = 1,
  HIGHLIGHT_CHANGE = 2,

  // Must be last.
  COUNT = 3,
}
// LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:ReadAnythingReadAloudSettingsChange)

// Enum for logging when a speech error event occurs.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ReadAnythingSpeechError)
export enum ReadAnythingSpeechError {
  TEXT_TOO_LONG = 0,
  LANGUAGE_UNAVAILABLE = 1,
  VOICE_UNAVAILABE = 2,
  INVALID_ARGUMENT = 3,
  SYNTHESIS_FAILED = 4,
  SYNTHESIS_UNVAILABLE = 5,
  AUDIO_BUSY = 6,
  AUDIO_HARDWARE = 7,
  NETWORK = 8,

  // Must be last.
  COUNT = 9,
}
// LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:ReadAnythingSpeechError)

// A proxy for forwarding logging calls to chrome.metricsPrivate or
// chrome.readingMode.
export interface MetricsBrowserProxy {
  incrementMetricCount(action: string): void;
  recordHighlightOff(): void;
  recordHighlightOn(): void;
  recordLanguage(lang: string): void;
  recordNewPage(): void;
  recordNewPageWithSpeech(): void;
  recordSpeechError(error: ReadAnythingSpeechError): void;
  recordSpeechPlaybackLength(time: number): void;
  recordSpeechSettingsChange(settingsChange: ReadAloudSettingsChange): void;
  recordTextSettingsChange(settingsChange: ReadAnythingSettingsChange): void;
  recordTime(umaName: string, time: number): void;
  recordVoiceSpeed(index: number): void;
  recordVoiceType(voiceType: ReadAnythingVoiceType): void;
}

export class MetricsBrowserProxyImpl implements MetricsBrowserProxy {
  incrementMetricCount(umaName: string) {
    chrome.readingMode.incrementMetricCount(umaName);
  }

  recordSpeechError(error: ReadAnythingSpeechError) {
    chrome.metricsPrivate.recordEnumerationValue(
        UmaName.SPEECH_ERROR, error, ReadAnythingSpeechError.COUNT);
  }

  recordTime(umaName: string, time: number) {
    chrome.metricsPrivate.recordTime(umaName, time);
  }

  recordNewPage() {
    chrome.metricsPrivate.recordEnumerationValue(
        UmaName.NEW_PAGE, ReadAnythingNewPage.NEW_PAGE,
        ReadAnythingNewPage.COUNT);
  }

  recordNewPageWithSpeech(): void {
    chrome.metricsPrivate.recordEnumerationValue(
        UmaName.NEW_PAGE, ReadAnythingNewPage.SPEECH_PLAYED_ON_NEW_PAGE,
        ReadAnythingNewPage.COUNT);
  }

  recordHighlightOn() {
    chrome.metricsPrivate.recordEnumerationValue(
        UmaName.HIGHLIGHT_STATE, ReadAloudHighlightState.HIGHLIGHT_ON,
        ReadAloudHighlightState.COUNT);
  }

  recordHighlightOff() {
    chrome.metricsPrivate.recordEnumerationValue(
        UmaName.HIGHLIGHT_STATE, ReadAloudHighlightState.HIGHLIGHT_OFF,
        ReadAloudHighlightState.COUNT);
  }

  recordVoiceType(voiceType: ReadAnythingVoiceType) {
    chrome.metricsPrivate.recordEnumerationValue(
        UmaName.VOICE, voiceType, ReadAnythingVoiceType.COUNT);
  }

  recordLanguage(lang: string) {
    chrome.metricsPrivate.recordSparseValueWithHashMetricName(
        UmaName.LANGUAGE, lang);
  }

  recordTextSettingsChange(settingsChange: ReadAnythingSettingsChange) {
    chrome.metricsPrivate.recordEnumerationValue(
        UmaName.TEXT_SETTINGS_CHANGE, settingsChange,
        ReadAnythingSettingsChange.COUNT);
  }

  recordSpeechSettingsChange(settingsChange: ReadAloudSettingsChange) {
    chrome.metricsPrivate.recordEnumerationValue(
        UmaName.SPEECH_SETTINGS_CHANGE, settingsChange,
        ReadAloudSettingsChange.COUNT);
  }

  recordVoiceSpeed(index: number) {
    chrome.metricsPrivate.recordSmallCount(UmaName.VOICE_SPEED, index);
  }

  recordSpeechPlaybackLength(time: number) {
    chrome.metricsPrivate.recordLongTime(UmaName.SPEECH_PLAYBACK, time);
  }

  static getInstance(): MetricsBrowserProxy {
    return instance || (instance = new MetricsBrowserProxyImpl());
  }

  static setInstance(obj: MetricsBrowserProxy) {
    instance = obj;
  }
}

let instance: MetricsBrowserProxy|null = null;
