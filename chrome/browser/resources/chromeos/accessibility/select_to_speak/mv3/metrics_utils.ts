// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PrefsManager} from './prefs_manager.js';

// Utilities for UMA metrics.

export class MetricsUtils {
  /**
   * Records a cancel event if speech was in progress.
   */
  static recordCancelIfSpeaking(): void {
    // TODO(b/1157214): Use select-to-speak's internal state instead of TTS
    // state.
    chrome.tts.isSpeaking(speaking => {
      if (speaking) {
        MetricsUtils.recordCancelEvent_();
      }
    });
  }

  /**
   * Records an event that Select-to-Speak has begun speaking.
   * @param method The CrosSelectToSpeakStartSpeechMethod enum
   *    that reflects how this event was triggered by the user.
   * @param prefsManager A PrefsManager with the users's current
   *    preferences.
   */
  static recordStartEvent(method: number, prefsManager: PrefsManager): void {
    chrome.metricsPrivate.recordUserAction(MetricsUtils.START_SPEECH_METRIC);
    chrome.metricsPrivate.recordEnumerationValue(
        MetricsUtils.START_SPEECH_METHOD_METRIC.METRIC_NAME, method,
        MetricsUtils.START_SPEECH_METHOD_METRIC.EVENT_COUNT);
    chrome.metricsPrivate.recordBoolean(
        MetricsUtils.BACKGROUND_SHADING_METRIC,
        prefsManager.backgroundShadingEnabled());
    chrome.metricsPrivate.recordBoolean(
        MetricsUtils.NAVIGATION_CONTROLS_METRIC,
        prefsManager.navigationControlsEnabled());
    chrome.metricsPrivate.recordBoolean(
        MetricsUtils.ENHANCED_NETWORK_VOICES_METRIC,
        prefsManager.enhancedNetworkVoicesEnabled());
  }

  /**
   * Records an event that Select-to-Speak speech has been canceled.
   */
  private static recordCancelEvent_(): void {
    chrome.metricsPrivate.recordUserAction(MetricsUtils.CANCEL_SPEECH_METRIC);
  }

  /**
   * Records an event that Select-to-Speak speech has been paused.
   */
  static recordPauseEvent(): void {
    chrome.metricsPrivate.recordUserAction(MetricsUtils.PAUSE_SPEECH_METRIC);
  }

  /**
   * Records an event that Select-to-Speak speech has been resumed from pause.
   */
  static recordResumeEvent(): void {
    chrome.metricsPrivate.recordUserAction(MetricsUtils.RESUME_SPEECH_METRIC);
  }

  /**
   * Records a user-requested state change event from a given state.
   */
  static recordSelectToSpeakStateChangeEvent(changeType: number): void {
    chrome.metricsPrivate.recordEnumerationValue(
        MetricsUtils.STATE_CHANGE_METRIC.METRIC_NAME, changeType,
        MetricsUtils.STATE_CHANGE_METRIC.EVENT_COUNT);
  }

  /**
   * Converts the speech multiplier into an enum based on
   * tools/metrics/histograms/enums.xml.
   * The value returned by this function is persisted to logs. Log entries
   * should not be renumbered and numeric values should never be reused, so this
   * function should not be changed.
   * @param speechRate The current speech rate.
   * @return The current speech rate as an int for metrics.
   */
  private static speechMultiplierToSparseHistogramInt_(speechRate: number):
      number {
    return Math.floor(speechRate * 100);
  }

  /**
   * Records the speed override chosen by the user.
   */
  static recordSpeechRateOverrideMultiplier(rate: number): void {
    chrome.metricsPrivate.recordSparseValue(
        MetricsUtils.OVERRIDE_SPEECH_RATE_MULTIPLIER_METRIC,
        MetricsUtils.speechMultiplierToSparseHistogramInt_(rate));
  }

  /**
   * Records the TTS engine used for a single speech utterance.
   * @param voiceName voice in TTS
   * @param prefsManager A PrefsManager with the users's current preferences.
   */
  static recordTtsEngineUsed(voiceName: string, prefsManager: PrefsManager):
      void {
    let ttsEngine: MetricsUtils.TtsEngineUsed;
    if (voiceName === '') {
      // No voice name passed to TTS, default voice is used
      ttsEngine = MetricsUtils.TtsEngineUsed.SYSTEM_DEFAULT;
    } else {
      const extensionId = prefsManager.ttsExtensionForVoice(voiceName);
      ttsEngine = MetricsUtils.ttsEngineForExtensionId_(extensionId);
    }
    chrome.metricsPrivate.recordEnumerationValue(
        MetricsUtils.TTS_ENGINE_USED_METRIC.METRIC_NAME, ttsEngine,
        MetricsUtils.TTS_ENGINE_USED_METRIC.EVENT_COUNT);
  }

  /**
   * Converts extension id of TTS voice into metric for logging.
   * @param extensionId Extension ID of TTS engine
   * @returns Enum used in TtsEngineUsed histogram.
   */
  private static ttsEngineForExtensionId_(extensionId: string):
      MetricsUtils.TtsEngineUsed {
    switch (extensionId) {
      case PrefsManager.ENHANCED_TTS_EXTENSION_ID:
        return MetricsUtils.TtsEngineUsed.GOOGLE_NETWORK;
      case PrefsManager.ESPEAK_EXTENSION_ID:
        return MetricsUtils.TtsEngineUsed.ESPEAK;
      case PrefsManager.GOOGLE_TTS_EXTENSION_ID:
        return MetricsUtils.TtsEngineUsed.GOOGLE_LOCAL;
      default:
        return MetricsUtils.TtsEngineUsed.UNKNOWN;
    }
  }

  /**
   * Record the number of OCRed pages in the PDF file opened with STS in Chrome
   * PDF Viewer.
   * @param numOcredPages Number of OCRed pages in the PDF file
   */
  static recordNumPdfPagesOcred(numOcredPages: number): void {
    chrome.metricsPrivate.recordMediumCount(
        MetricsUtils.PDF_OCR_PAGES_OCRED_METRIC, numOcredPages);
  }
}

export namespace MetricsUtils {
  /**
   * Defines an enumeration metric. The |EVENT_COUNT| must be kept in sync
   * with the number of enum values for each metric in
   * tools/metrics/histograms/enums.xml.
   */
  export interface EnumerationMetric {
    EVENT_COUNT: number;
    METRIC_NAME: string;
  }

  /**
   * CrosSelectToSpeakStartSpeechMethod enums.
   * These values are persisted to logs and should not be renumbered or re-used.
   * See tools/metrics/histograms/enums.xml.
   */
  export enum StartSpeechMethod {
    MOUSE,
    KEYSTROKE,
    CONTEXT_MENU,
  }

  /**
   * Constants for the start speech method metric,
   * CrosSelectToSpeakStartSpeechMethod.
   */
  export const START_SPEECH_METHOD_METRIC: EnumerationMetric = {
    EVENT_COUNT: Object.keys(StartSpeechMethod).length,
    METRIC_NAME: 'Accessibility.CrosSelectToSpeak.StartSpeechMethod',
  };

  /**
   * CrosSelectToSpeakStateChangeEvent enums.
   * These values are persisted to logs and should not be renumbered or re-used.
   * See tools/metrics/histograms/enums.xml.
   */
  export enum StateChangeEvent {
    START_SELECTION,
    CANCEL_SPEECH,
    CANCEL_SELECTION,
  }

  /**
   * Constants for the state change metric, CrosSelectToSpeakStateChangeEvent.
   */
  export const STATE_CHANGE_METRIC: EnumerationMetric = {
    EVENT_COUNT: Object.keys(StateChangeEvent).length,
    METRIC_NAME: 'Accessibility.CrosSelectToSpeak.StateChangeEvent',
  };

  /**
   * CrosSelectToSpeakTtsEngineUsed enums.
   * These values are persisted to logs and should not be renumbered or re-used.
   * See tools/metrics/histograms/enums.xml.
   */
  export enum TtsEngineUsed {
    UNKNOWN,
    SYSTEM_DEFAULT,
    ESPEAK,
    GOOGLE_LOCAL,
    GOOGLE_NETWORK,
  }

  /**
   * Constants for the TTS engine metric, CrosSelectToSpeak.TtsEngineUsed.
   */
  export const TTS_ENGINE_USED_METRIC: EnumerationMetric = {
    EVENT_COUNT: Object.keys(TtsEngineUsed).length,
    METRIC_NAME: 'Accessibility.CrosSelectToSpeak.TtsEngineUsed',
  };

  /**
   * The start speech metric name.
   */
  export const START_SPEECH_METRIC =
      'Accessibility.CrosSelectToSpeak.StartSpeech';

  /**
   * The cancel speech metric name.
   */
  export const CANCEL_SPEECH_METRIC =
      'Accessibility.CrosSelectToSpeak.CancelSpeech';

  /**
   * The pause speech metric name.
   */
  export const PAUSE_SPEECH_METRIC =
      'Accessibility.CrosSelectToSpeak.PauseSpeech';

  /**
   * The resume speech after pausing metric name.
   */
  export const RESUME_SPEECH_METRIC =
      'Accessibility.CrosSelectToSpeak.ResumeSpeech';

  /**
   * The background shading metric name.
   */
  export const BACKGROUND_SHADING_METRIC =
      'Accessibility.CrosSelectToSpeak.BackgroundShading';

  /**
   * The navigation controls metric name.
   */
  export const NAVIGATION_CONTROLS_METRIC =
      'Accessibility.CrosSelectToSpeak.NavigationControls';

  /**
   * The metric name for enhanced network TTS voices.
   */
  export const ENHANCED_NETWORK_VOICES_METRIC =
      'Accessibility.CrosSelectToSpeak.EnhancedNetworkVoices';

  /**
   * The speech rate override histogram metric name.
   */
  export const OVERRIDE_SPEECH_RATE_MULTIPLIER_METRIC =
      'Accessibility.CrosSelectToSpeak.OverrideSpeechRateMultiplier';

  export const PDF_OCR_PAGES_OCRED_METRIC =
      'Accessibility.PdfOcr.CrosSelectToSpeak.PagesOcred';
}
