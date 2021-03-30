// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
  This file is for recording Chrome Browser's language settings metrics on the
  frontend. See languages_metrics_proxy.js for ChromeOS-specific metrics. Note
  that this file and languages_metrics_proxy refer to two separate histograms,
  but there is some overlap in the actions they track. This will be resolved in
  crbug/1109431, when ChromeOS's language settings migration is complete.
*/

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

/**
 * These values are persisted to LanguageSettingsActionType
 * in tools/metrics/histograms/enums.xml. Entries should not be renumbered and
 * should never be reused.
 * @enum {number}
 */
export const LanguageSettingsActionType = {
  // CLICK_ON_ADD_LANGUAGE: 1, // Deprecated, use ADD_LANGUAGE in
  // LanguageSettingsPageImpression
  LANGUAGE_ADDED: 2,
  LANGUAGE_REMOVED: 3,
  DISABLE_TRANSLATE_GLOBALLY: 4,
  ENABLE_TRANSLATE_GLOBALLY: 5,
  DISABLE_TRANSLATE_FOR_SINGLE_LANGUAGE: 6,
  ENABLE_TRANSLATE_FOR_SINGLE_LANGUAGE: 7,
  LANGUAGE_LIST_REORDERED: 8,
};

/**
 * These values are persisted to LanguageSettingsPageImpression
 * in tools/metrics/histograms/enums.xml. Entries should not be renumbered and
 * should never be reused.
 * @enum {number}
 */
export const LanguageSettingsPageImpressionType = {
  MAIN: 0,
  ADD_LANGUAGE: 1,
  // LANGUAGE_DETAILS: 2, // iOS only
  CHROME_LANGUAGE: 3,
  ADVANCED_LANGUAGE_SETTINGS: 4,
  TARGET_LANGUAGE: 5,
  LANGUAGE_OVERFLOW_MENU_OPENED: 6,
};

/** @interface */
export class LanguageSettingsMetricsProxy {
  /**
   * Records the interaction to enumerated histogram.
   * @param {!LanguageSettingsActionType} interaction
   */
  recordSettingsMetric(interaction) {}

  /**
   * Records the interaction to enumerated histogram.
   * @param {!LanguageSettingsPageImpressionType} interaction
   */
  recordPageImpressionMetric(interaction) {}
}

/** @implements {LanguageSettingsMetricsProxy} */
export class LanguageSettingsMetricsProxyImpl {
  /** @override */
  recordSettingsMetric(interaction) {
    chrome.metricsPrivate.recordEnumerationValue(
        'LanguageSettings.Actions', interaction,
        Object.keys(LanguageSettingsActionType).length);
  }

  /** @override */
  recordPageImpressionMetric(interaction) {
    chrome.metricsPrivate.recordEnumerationValue(
        'LanguageSettings.PageImpression', interaction,
        Object.keys(LanguageSettingsPageImpressionType).length);
  }
}

addSingletonGetter(LanguageSettingsMetricsProxyImpl);
