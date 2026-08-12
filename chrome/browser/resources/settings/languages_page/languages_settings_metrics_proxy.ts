// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
  This file is for recording Chrome Browser's language settings metrics on the
  frontend.
*/

// LINT.IfChange(LanguageSettingsActionType)
/**
 * These values are persisted to LanguageSettingsActionType
 * in tools/metrics/histograms/metadata/language/enums.xml. Entries should not be renumbered and
 * should never be reused.
 */
export enum LanguageSettingsActionType {
  // CLICK_ON_ADD_LANGUAGE = 1, // Deprecated, use ADD_LANGUAGE in
  // LanguageSettingsPageImpression
  LANGUAGE_ADDED = 2,
  LANGUAGE_REMOVED = 3,
  DISABLE_TRANSLATE_GLOBALLY = 4,
  ENABLE_TRANSLATE_GLOBALLY = 5,
  DISABLE_TRANSLATE_FOR_SINGLE_LANGUAGE = 6,
  ENABLE_TRANSLATE_FOR_SINGLE_LANGUAGE = 7,
  LANGUAGE_LIST_REORDERED = 8,
  CHANGE_CHROME_LANGUAGE = 9,
  CHANGE_TRANSLATE_TARGET = 10,
  REMOVE_FROM_NEVER_TRANSLATE = 11,
  ADD_TO_NEVER_TRANSLATE = 12,
  REMOVE_FROM_ALWAYS_TRANSLATE = 13,
  ADD_TO_ALWAYS_TRANSLATE = 14,
  REMOVE_FROM_NEVER_TRANSLATE_SITES = 15,
  RESTART_FOR_SPLIT_INSTALL = 16,
  ENABLE_SPELL_CHECK_GLOBALLY = 17,
  DISABLE_SPELL_CHECK_GLOBALLY = 18,
  ENABLE_SPELL_CHECK_FOR_LANGUAGE = 19,
  DISABLE_SPELL_CHECK_FOR_LANGUAGE = 20,
  SELECT_BASIC_SPELL_CHECK = 21,
  SELECT_ENHANCED_SPELL_CHECK = 22,
  COUNT = SELECT_ENHANCED_SPELL_CHECK + 1,
}
// LINT.ThenChange(
//   //tools/metrics/histograms/metadata/language/enums.xml:LanguageSettingsActionType,
//   //chrome/browser/language/android/java/src/org/chromium/chrome/browser/language/settings/LanguagesManager.java:LanguageSettingsActionType
// )

/**
 * These values are persisted to LanguageSettingsPageImpression
 * in tools/metrics/histograms/metadata/language/enums.xml. Entries should not be renumbered and
 * should never be reused.
 */
// LINT.IfChange(LanguageSettingsPageType)
export enum LanguageSettingsPageImpressionType {
  MAIN = 0,
  ADD_LANGUAGE = 1,
  LANGUAGE_DETAILS = 2,
  CHROME_LANGUAGE = 3,
  ADVANCED_LANGUAGE_SETTINGS = 4,
  TARGET_LANGUAGE = 5,
  LANGUAGE_OVERFLOW_MENU_OPENED = 6,
  VIEW_NEVER_TRANSLATE_LANGUAGES = 7,
  NEVER_TRANSLATE_ADD_LANGUAGE = 8,
  VIEW_ALWAYS_TRANSLATE_LANGUAGES = 9,
  ALWAYS_TRANSLATE_ADD_LANGUAGE = 10,
  VIEW_NEVER_TRANSLATE_SITES = 11,
  NUM_ENTRIES = 12,
}
// LINT.ThenChange(
//   //tools/metrics/histograms/metadata/language/enums.xml:LanguageSettingsPageType,
//   //chrome/browser/language/android/java/src/org/chromium/chrome/browser/language/settings/LanguagesManager.java:LanguageSettingsPageType
// )

export interface LanguageSettingsMetricsProxy {
  /**
   * Records the interaction to enumerated histogram.
   */
  recordSettingsMetric(interaction: LanguageSettingsActionType): void;

  /**
   * Records the interaction to enumerated histogram.
   */
  recordPageImpressionMetric(interaction: LanguageSettingsPageImpressionType):
      void;
}

export class LanguageSettingsMetricsProxyImpl implements
    LanguageSettingsMetricsProxy {
  recordSettingsMetric(interaction: LanguageSettingsActionType) {
    chrome.metricsPrivate.recordEnumerationValue(
        'LanguageSettings.Actions', interaction,
        LanguageSettingsActionType.COUNT);
  }

  recordPageImpressionMetric(interaction: LanguageSettingsPageImpressionType) {
    chrome.metricsPrivate.recordEnumerationValue(
        'LanguageSettings.PageImpression', interaction,
        LanguageSettingsPageImpressionType.NUM_ENTRIES);
  }

  static getInstance(): LanguageSettingsMetricsProxy {
    return instance || (instance = new LanguageSettingsMetricsProxyImpl());
  }

  static setInstance(obj: LanguageSettingsMetricsProxy) {
    instance = obj;
  }
}

let instance: LanguageSettingsMetricsProxy|null = null;
