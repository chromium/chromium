// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Closure typedefs for dictionaries and interfaces used by
 * language settings.
 */

/**
 * Settings and state for a particular enabled language.
 */
export interface LanguageState {
  language: chrome.languageSettingsPrivate.Language;
  removable: boolean;
  spellCheckEnabled: boolean;
  translateEnabled: boolean;
  isManaged: boolean;
  isForced: boolean;
  downloadDictionaryFailureCount: number;
  downloadDictionaryStatus:
      (chrome.languageSettingsPrivate.SpellcheckDictionaryStatus|null);
}

/**
 * Settings and state for spellcheck languages.
 */
export interface SpellCheckLanguageState {
  language: chrome.languageSettingsPrivate.Language;
  spellCheckEnabled: boolean;
  isManaged: boolean;
  downloadDictionaryFailureCount: number;
  downloadDictionaryStatus:
      (chrome.languageSettingsPrivate.SpellcheckDictionaryStatus|null);
}

/**
 * Languages data to expose to consumers.
 * supported: an array of languages, ordered alphabetically, set once
 *     at initialization.
 * enabled: an array of enabled language states, ordered by preference.
 * translateTarget: the default language to translate into.
 * prospectiveUILanguage: the "prospective" UI language, i.e., the one to be
 *     used on next restart. Matches the current UI language preference unless
 *     the user has chosen a different language without restarting. May differ
 *     from the actually used language (navigator.language). Chrome OS and
 *     Windows only.
 * spellCheckOnLanguages: an array of spell check languages that are currently
 *     in use, including the languages force-enabled by policy.
 * spellCheckOffLanguages: an array of spell check languages that are currently
 *     not in use, including the languages force-disabled by policy.
 */
export interface LanguagesModel {
  supported: chrome.languageSettingsPrivate.Language[];
  enabled: LanguageState[];
  translateTarget: string;
  alwaysTranslate: chrome.languageSettingsPrivate.Language[];
  neverTranslate: chrome.languageSettingsPrivate.Language[];
  neverTranslateSites: string[];
  spellCheckOnLanguages: SpellCheckLanguageState[];
  spellCheckOffLanguages: SpellCheckLanguageState[];
  // TODO(dpapad): Wrap prospectiveUILanguage with if expr "is_win" block.
  prospectiveUILanguage?: string;
}

/**
 * Helper methods for reading and writing language settings.
 */
export interface LanguageHelper {
  languages?: LanguagesModel|undefined;

  whenReady(): Promise<void>;

  // <if expr="is_win">
  /**
   * Sets the prospective UI language to the chosen language. This won't affect
   * the actual UI language until a restart.
   */
  setProspectiveUiLanguage(languageCode: string): void;

  /**
   * True if the prospective UI language has been changed.
   */
  requiresRestart(): boolean;

  // </if>

  /**
   * @return The language code for ARC IMEs.
   */
  getArcImeLanguageCode(): string;

  /**
   * @param language
   * @return the [displayName] - [nativeDisplayName] if displayName and
   * nativeDisplayName are different.
   * If they're the same than only returns the displayName.
   */
  getFullName(language: chrome.languageSettingsPrivate.Language): string;

  isLanguageCodeForArcIme(languageCode: string): boolean;

  isTranslateBaseLanguage(language: chrome.languageSettingsPrivate.Language):
      boolean;
  isLanguageEnabled(languageCode: string): boolean;

  /**
   * Enables the language, making it available for spell check and input.
   */
  enableLanguage(languageCode: string): void;

  disableLanguage(languageCode: string): void;

  /**
   * Returns true iff provided languageState can be disabled.
   */
  canDisableLanguage(languageState: LanguageState): boolean;

  canEnableLanguage(language: chrome.languageSettingsPrivate.Language): boolean;

  /**
   * Moves the language in the list of enabled languages by the given offset.
   * @param upDirection True if we need to move toward the front, false if we
   *     need to move toward the back.
   */
  moveLanguage(languageCode: string, upDirection: boolean): void;

  /**
   * Moves the language directly to the front of the list of enabled languages.
   */
  moveLanguageToFront(languageCode: string): void;

  /**
   * Enables translate for the given language by removing the translate
   * language from the blocked languages preference.
   */
  enableTranslateLanguage(languageCode: string): void;

  /**
   * Disables translate for the given language by adding the translate
   * language to the blocked languages preference.
   */
  disableTranslateLanguage(languageCode: string): void;

  /**
   * Sets the translate target language.
   */
  setTranslateTargetLanguage(languageCode: string): void;

  /**
   * Sets whether a given language should always be automatically translated.
   */
  setLanguageAlwaysTranslateState(
      languageCode: string, alwaysTranslate: boolean): void;

  /**
   * Enables or disables spell check for the given language.
   */
  toggleSpellCheck(languageCode: string, enable: boolean): void;

  /**
   * Converts the language code for translate. There are some differences
   * between the language set the Translate server uses and that for
   * Accept-Language.
   */
  convertLanguageCodeForTranslate(languageCode: string): string;

  /**
   * Converts the language code to Chrome format.
   */
  convertLanguageCodeForChrome(languageCode: string): string;

  /**
   * Given a language code, returns just the base language. E.g., converts
   * 'en-GB' to 'en'.
   */
  getBaseLanguage(languageCode: string): string;

  getLanguage(languageCode: string): chrome.languageSettingsPrivate.Language
      |undefined;

  retryDownloadDictionary(languageCode: string): void;
}
