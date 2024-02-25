// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Macro} from '/common/action_fulfillment/macros/macro.js';

import SpeechRecognitionType = chrome.speechRecognitionPrivate.SpeechRecognitionType;

/** A class used record metrics for the dictation feature. */
export class MetricsUtils {
  private onDevice_: boolean;
  private locale_: string;
  private speechRecognitionStartTime_: Date|null = null;

  constructor(type: SpeechRecognitionType, locale: string) {
    this.onDevice_ = type === SpeechRecognitionType.ON_DEVICE;
    this.locale_ = locale;
  }

  static recordMacroRecognized(macro: Macro): void {
    chrome.metricsPrivate.recordSparseValue(
        MetricsUtils.MACRO_RECOGNIZED_METRIC, macro.getName());
  }

  static recordMacroSucceeded(macro: Macro): void {
    chrome.metricsPrivate.recordSparseValue(
        MetricsUtils.MACRO_SUCCEEDED_METRIC, macro.getName());
  }

  static recordMacroFailed(macro: Macro): void {
    chrome.metricsPrivate.recordSparseValue(
        MetricsUtils.MACRO_FAILED_METRIC, macro.getName());
  }

  static recordPumpkinUsed(used: boolean): void {
    chrome.metricsPrivate.recordBoolean(MetricsUtils.PUMPKIN_USED_METRIC, used);
  }

  static recordPumpkinSucceeded(succeeded: boolean): void {
    chrome.metricsPrivate.recordBoolean(
        MetricsUtils.PUMPKIN_SUCCEEDED_METRIC, succeeded);
  }

  /** Records metrics when speech recognition starts. */
  recordSpeechRecognitionStarted(): void {
    chrome.metricsPrivate.recordBoolean(
        MetricsUtils.ON_DEVICE_SPEECH_METRIC, this.onDevice_);
    chrome.metricsPrivate.recordSparseValueWithHashMetricName(
        MetricsUtils.LOCALE_METRIC, this.locale_);
    this.speechRecognitionStartTime_ = new Date();
  }

  /**
   * Records metrics when speech recognition stops. Must be called after
   * `recordSpeechRecognitionStarted` is called.
   */
  recordSpeechRecognitionStopped(): void {
    if (this.speechRecognitionStartTime_ === null) {
      // Check that we have called `recordSpeechRecognitionStarted` by
      // checking `speechRecognitionStartTime_`.
      console.warn(
          `Failed to record metrics when speech recognition stopped, valid
          speech recognition start time required.`);
      return;
    }

    const metricName = this.onDevice_ ?
        MetricsUtils.LISTENING_DURATION_METRIC_ON_DEVICE :
        MetricsUtils.LISTENING_DURATION_METRIC_NETWORK;
    const listeningDuration =
        new Date().getTime() - this.speechRecognitionStartTime_.getTime();
    chrome.metricsPrivate.recordLongTime(metricName, listeningDuration);
  }
}

export namespace MetricsUtils {
  /**
   * The metric used to record whether on-device or network speech recognition
   * was used.
   */
  export const ON_DEVICE_SPEECH_METRIC =
      'Accessibility.CrosDictation.UsedOnDeviceSpeech';

  /**
   * The metric used to record which locale Dictation used for speech
   * recognition.
   */
  export const LOCALE_METRIC = 'Accessibility.CrosDictation.Language';

  /**
   * The metric used to record the listening duration for on-device speech
   * recognition.
   */
  export const LISTENING_DURATION_METRIC_ON_DEVICE =
      'Accessibility.CrosDictation.ListeningDuration.OnDeviceRecognition';

  /**
   * The metric used to record the listening duration for network speech
   * recognition.
   */
  export const LISTENING_DURATION_METRIC_NETWORK =
      'Accessibility.CrosDictation.ListeningDuration.NetworkRecognition';

  /** The metric used to record that a macro was recognized. */
  export const MACRO_RECOGNIZED_METRIC =
      'Accessibility.CrosDictation.MacroRecognized';

  /** The metric used to record that a macro succeeded. */
  export const MACRO_SUCCEEDED_METRIC =
      'Accessibility.CrosDictation.MacroSucceeded';

  /** The metric used to record that a macro failed. */
  export const MACRO_FAILED_METRIC = 'Accessibility.CrosDictation.MacroFailed';

  /**
   * The metric used to record whether or not Pumpkin was used for command
   * parsing.
   */
  export const PUMPKIN_USED_METRIC = 'Accessibility.CrosDictation.UsedPumpkin';

  /**
   * The metric used to record whether or not Pumpkin succeeded in parsing a
   * command.
   */
  export const PUMPKIN_SUCCEEDED_METRIC =
      'Accessibility.CrosDictation.PumpkinSucceeded';
}
