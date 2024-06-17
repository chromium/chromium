// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

enum UmaName {
  NEW_PAGE_UMA = 'Accessibility.ReadAnything.NewPage',
  LANGUAGE_UMA = 'Accessibility.ReadAnything.ReadAloud.Language',
  VOICE_UMA = 'Accessibility.ReadAnything.ReadAloud.Voice',
  TEXT_SETTINGS_CHANGE_UMA = 'Accessibility.ReadAnything.SettingsChange',
  HIGHLIGHT_STATE_UMA = 'Accessibility.ReadAnything.ReadAloud.HighlightState',
  VOICE_SPEED_UMA = 'Accessibility.ReadAnything.ReadAloud.VoiceSpeed',
  SPEECH_SETTINGS_CHANGE_UMA =
      'Accessibility.ReadAnything.ReadAloud.SettingsChange',
}

// Enum for logging when we play speech on a page.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
export enum ReadAnythingNewPage {
  NEW_PAGE = 0,
  SPEECH_PLAYED_ON_NEW_PAGE = 1,

  // Must be last.
  COUNT = 2,
}

// Enum for logging which kind of voice is being used to read aloud.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
export enum ReadAnythingVoiceType {
  NATURAL = 0,
  ESPEAK = 1,
  CHROMEOS = 2,

  // Must be last.
  COUNT = 3,
}

// Enum for logging when a text style setting is changed.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
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

// Enum for logging the reading highlight state.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
export enum ReadAloudHighlightState {
  HIGHLIGHT_ON = 0,
  HIGHLIGHT_OFF = 1,

  // Must be last.
  COUNT = 2,
}

// Enum for logging when a read aloud speech setting is changed.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
export enum ReadAloudSettingsChange {
  VOICE_SPEED_CHANGE = 0,
  VOICE_NAME_CHANGE = 1,
  HIGHLIGHT_CHANGE = 2,

  // Must be last.
  COUNT = 3,
}

// A proxy for forwarding logging calls to chrome.metricsPrivate.
export interface MetricsBrowserProxy {
  recordNewPage(): void;
  recordNewPageWithSpeech(): void;
  recordHighlightOn(): void;
  recordHighlightOff(): void;
  recordVoiceType(voiceType: ReadAnythingVoiceType): void;
  recordLanguage(lang: string): void;
  recordTextSettingsChange(settingsChange: ReadAnythingSettingsChange): void;
  recordSpeechSettingsChange(settingsChange: ReadAloudSettingsChange): void;
  recordVoiceSpeed(index: number): void;
}

export class MetricsBrowserProxyImpl implements MetricsBrowserProxy {
  recordNewPage() {
    chrome.metricsPrivate.recordEnumerationValue(
        UmaName.NEW_PAGE_UMA, ReadAnythingNewPage.NEW_PAGE,
        ReadAnythingNewPage.COUNT);
  }

  recordNewPageWithSpeech(): void {
    chrome.metricsPrivate.recordEnumerationValue(
        UmaName.NEW_PAGE_UMA, ReadAnythingNewPage.SPEECH_PLAYED_ON_NEW_PAGE,
        ReadAnythingNewPage.COUNT);
  }

  recordHighlightOn() {
    chrome.metricsPrivate.recordEnumerationValue(
        UmaName.HIGHLIGHT_STATE_UMA, ReadAloudHighlightState.HIGHLIGHT_ON,
        ReadAloudHighlightState.COUNT);
  }

  recordHighlightOff() {
    chrome.metricsPrivate.recordEnumerationValue(
        UmaName.HIGHLIGHT_STATE_UMA, ReadAloudHighlightState.HIGHLIGHT_OFF,
        ReadAloudHighlightState.COUNT);
  }

  recordVoiceType(voiceType: ReadAnythingVoiceType) {
    chrome.metricsPrivate.recordEnumerationValue(
        UmaName.VOICE_UMA, voiceType, ReadAnythingVoiceType.COUNT);
  }

  recordLanguage(lang: string) {
    chrome.metricsPrivate.recordSparseValueWithHashMetricName(
        UmaName.LANGUAGE_UMA, lang);
  }

  recordTextSettingsChange(settingsChange: ReadAnythingSettingsChange) {
    chrome.metricsPrivate.recordEnumerationValue(
        UmaName.TEXT_SETTINGS_CHANGE_UMA, settingsChange,
        ReadAnythingSettingsChange.COUNT);
  }

  recordSpeechSettingsChange(settingsChange: ReadAloudSettingsChange) {
    chrome.metricsPrivate.recordEnumerationValue(
        UmaName.SPEECH_SETTINGS_CHANGE_UMA, settingsChange,
        ReadAloudSettingsChange.COUNT);
  }

  recordVoiceSpeed(index: number) {
    chrome.metricsPrivate.recordSmallCount(UmaName.VOICE_SPEED_UMA, index);
  }

  static getInstance(): MetricsBrowserProxy {
    return instance || (instance = new MetricsBrowserProxyImpl());
  }

  static setInstance(obj: MetricsBrowserProxy) {
    instance = obj;
  }
}

let instance: MetricsBrowserProxy|null = null;
