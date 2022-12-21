// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Macro} from './macros/macro.js';

const SpeechRecognitionType =
    chrome.speechRecognitionPrivate.SpeechRecognitionType;

/** A class used record metrics for the dictation feature. */
export class MetricsUtils {
  /**
   * @param {!SpeechRecognitionType} type
   * @param {string} locale
   */
  constructor(type, locale) {
    /** @private {boolean} */
    this.onDevice_ = type === SpeechRecognitionType.ON_DEVICE;
    /** @private {string} */
    this.locale_ = locale;
    /** @private {?Date} */
    this.speechRecognitionStartTime_ = null;
  }

  /** @param {!Macro} macro */
  static recordMacroRecognized(macro) {
    chrome.metricsPrivate.recordSparseValue(
        MetricsUtils.MACRO_RECOGNIZED_METRIC, macro.getName());
  }

  /** @param {!Macro} macro */
  static recordMacroSucceeded(macro) {
    chrome.metricsPrivate.recordSparseValue(
        MetricsUtils.MACRO_SUCCEEDED_METRIC, macro.getName());
  }

  /** @param {!Macro} macro */
  static recordMacroFailed(macro) {
    chrome.metricsPrivate.recordSparseValue(
        MetricsUtils.MACRO_FAILED_METRIC, macro.getName());
  }

  /** @param {boolean} used */
  static recordPumpkinUsed(used) {
    chrome.metricsPrivate.recordBoolean(MetricsUtils.PUMPKIN_METRIC, used);
  }

  /** Records metrics when speech recognition starts. */
  recordSpeechRecognitionStarted() {
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
  recordSpeechRecognitionStopped() {
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
    const listeningDuration = new Date() - this.speechRecognitionStartTime_;
    chrome.metricsPrivate.recordLongTime(metricName, listeningDuration);
  }
}

/**
 * The metric used to record whether on-device or network speech recognition
 * was used.
 * @const {string}
 */
MetricsUtils.ON_DEVICE_SPEECH_METRIC =
    'Accessibility.CrosDictation.UsedOnDeviceSpeech';

/**
 * The metric used to record which locale Dictation used for speech recognition.
 * @const {string}
 */
MetricsUtils.LOCALE_METRIC = 'Accessibility.CrosDictation.Language';

/**
 * The metric used to record the listening duration for on-device speech
 * recognition.
 * @const {string}
 */
MetricsUtils.LISTENING_DURATION_METRIC_ON_DEVICE =
    'Accessibility.CrosDictation.ListeningDuration.OnDeviceRecognition';

/**
 * The metric used to record the listening duration for network speech
 * recognition.
 * @const {string}
 */
MetricsUtils.LISTENING_DURATION_METRIC_NETWORK =
    'Accessibility.CrosDictation.ListeningDuration.NetworkRecognition';

/**
 * The metric used to record that a macro was recognized.
 * @const {string}
 */
MetricsUtils.MACRO_RECOGNIZED_METRIC =
    'Accessibility.CrosDictation.MacroRecognized';

/**
 * The metric used to record that a macro succeeded.
 * @const {string}
 */
MetricsUtils.MACRO_SUCCEEDED_METRIC =
    'Accessibility.CrosDictation.MacroSucceeded';

/**
 * The metric used to record that a macro failed.
 * @const {string}
 */
MetricsUtils.MACRO_FAILED_METRIC = 'Accessibility.CrosDictation.MacroFailed';

/**
 * The metric used to record whether or not Pumpkin was used for command
 * parsing.
 * @const {string}
 */
MetricsUtils.PUMPKIN_METRIC = 'Accessibility.CrosDictation.UsedPumpkin';
