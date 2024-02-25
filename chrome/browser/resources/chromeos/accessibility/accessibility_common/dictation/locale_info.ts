// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestImportManager} from '/common/testing/test_import_manager.js';

/** Contains all locale-related information for Dictation. */
export class LocaleInfo {
  static allowSmartCapAndSpacing(): boolean {
    const language = LocaleInfo.locale.split('-')[0];
    return LocaleInfo.SMART_CAP_AND_SPACING_LANGUAGES_.has(language);
  }

  static allowSmartEditing(): boolean {
    // Restrict smart editing commands to left-to-right locales.
    // TODO(crbug.com/1331351): Add support for RTL locales.
    return !LocaleInfo.isRTLLocale();
  }

  /* eslint-disable-next-line @typescript-eslint/naming-convention */
  static isRTLLocale(): boolean {
    const locale = LocaleInfo.locale;
    return LocaleInfo.RTL_LOCALES_.has(locale);
  }

  /* eslint-disable-next-line @typescript-eslint/naming-convention */
  static getUILanguage(): string|undefined {
    const locale = LocaleInfo.locale.toLowerCase();
    return LocaleInfo.LOCALE_TO_UI_LANGUAGE_MAP_[locale];
  }

  /**
   * Determines whether commands are supported for this Dictation language
   * and UI system language.
   * @return Whether or not commands are supported.
   */
  static areCommandsSupported(): boolean {
    // Currently Dictation cannot support commands when the UI language
    // doesn't match the Dictation language. See crbug.com/1340590.
    const systemLocale = chrome.i18n.getUILanguage().toLowerCase();
    const systemLanguage = systemLocale.split('-')[0];
    const dictationLanguage = LocaleInfo.locale.toLowerCase().split('-')[0];
    if (systemLanguage === dictationLanguage) {
      return true;
    }

    return LocaleInfo.alwaysEnableCommandsForTesting ||
        Boolean(LocaleInfo.getUILanguage()) &&
        (LocaleInfo.getUILanguage() === systemLanguage ||
         LocaleInfo.getUILanguage() === systemLocale);
  }

  /** Returns true if we should consider spaces, false otherwise. */
  static considerSpaces(): boolean {
    const language = LocaleInfo.locale.toLowerCase().split('-')[0];
    return !LocaleInfo.NO_SPACE_LANGUAGES_.has(language);
  }
}

export namespace LocaleInfo {
  /** The current Dictation locale. */
  /* eslint-disable-next-line prefer-const */
  export let locale = '';

  export const alwaysEnableCommandsForTesting = false;

  export const SMART_CAP_AND_SPACING_LANGUAGES_ =
      new Set(['en', 'fr', 'it', 'de', 'es']);

  /** All RTL locales from Dictation::GetAllSupportedLocales. */
  export const RTL_LOCALES_ = new Set([
    'ar-AE', 'ar-BH', 'ar-DZ', 'ar-EG', 'ar-IL', 'ar-IQ', 'ar-JO',
    'ar-KW', 'ar-LB', 'ar-MA', 'ar-OM', 'ar-PS', 'ar-QA', 'ar-SA',
    'ar-TN', 'ar-YE', 'fa-IR', 'iw-IL', 'ur-IN', 'ur-PK',
  ]);

  /**
   * TODO: get this data from l10n or i18n.
   * Hebrew in Dictation is 'iw-IL' but 'he' in UI languages.
   * yue-Hant-HK can map to 'zh-TW' because both are written as traditional
   * Chinese. Norwegian in Dictation is 'no-NO' but 'nb' in UI languages.
   */
  export const LOCALE_TO_UI_LANGUAGE_MAP_: Record<string, string> = {
    'iw-il': 'he',
    'yue-hant-hk': 'zh-tw',
    'no-no': 'nb',
  };

  /**
   * TODO(akihiroota): Follow-up with an i18n expert to get a full list of
   * languages.
   * All languages that don't use spaces.
   */
  export const NO_SPACE_LANGUAGES_ = new Set(['ja', 'zh']);
}

TestImportManager.exportForTesting(LocaleInfo);
