// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Typedefs for dictionaries and interfaces used by
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
      chrome.languageSettingsPrivate.SpellcheckDictionaryStatus|null;
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
      chrome.languageSettingsPrivate.SpellcheckDictionaryStatus|null;
}

/**
 * Input method data to expose to consumers (Chrome OS only).
 * supported: an array of supported input methods set once at initialization.
 * enabled: an array of the currently enabled input methods.
 * currentId: ID of the currently active input method.
 */
export interface InputMethodsModel {
  supported: chrome.languageSettingsPrivate.InputMethod[];
  enabled: chrome.languageSettingsPrivate.InputMethod[];
  currentId: string;
  /**
   * Mapping from input method ID to language packs status.
   */
  // TODO: b/298884063 - Move this to
  // `chrome.languageSettingsPrivate.InputMethod` if there is a synchronous
  // "cache" of language pack statuses.
  imeLanguagePackStatus:
      Partial<Record<string, chrome.inputMethodPrivate.LanguagePackStatus>>;
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
 * inputMethods: the InputMethodsModel (Chrome OS only).
 * spellCheckOnLanguages: an array of spell check languages that are currently
 *     in use, including the languages force-enabled by policy.
 * spellCheckOffLanguages: an array of spell check languages that are currently
 *     not in use, including the languages force-disabled by policy.
 */
export interface LanguagesModel {
  supported: chrome.languageSettingsPrivate.Language[];
  enabled: LanguageState[];
  translateTarget: string;
  // TODO(b/263824661): Remove undefined from these definitions if we do not
  // share this file with browser settings.
  /** Always defined on CrOS, set in `createModel_()` in `languages.ts`. */
  prospectiveUILanguage: (string|undefined);
  /** Always defined on CrOS, set in `createModel_()` in `languages.ts`. */
  inputMethods: (InputMethodsModel|undefined);
  alwaysTranslate: chrome.languageSettingsPrivate.Language[];
  neverTranslate: chrome.languageSettingsPrivate.Language[];
  spellCheckOnLanguages: SpellCheckLanguageState[];
  spellCheckOffLanguages: SpellCheckLanguageState[];
}

/**
 * Helper methods for reading and writing language settings.
 */
export interface LanguageHelper {
  whenReady(): Promise<void>;

  /**
   * Sets the prospective UI language to the chosen language. This won't affect
   * the actual UI language until a restart.
   */
  setProspectiveUiLanguage(languageCode: string): void;

  /**
   * True if the prospective UI language has been changed.
   */
  requiresRestart(): boolean;

  /**
   * @return The language code for ARC IMEs.
   */
  getArcImeLanguageCode(): string;

  isLanguageCodeForArcIme(languageCode: string): boolean;

  isLanguageTranslatable(language: chrome.languageSettingsPrivate.Language):
      boolean;

  isLanguageEnabled(languageCode: string): boolean;

  /**
   * Enables the language, making it available for spell check and input.
   */
  enableLanguage(languageCode: string): void;

  /**
   * Disables the language.
   */
  disableLanguage(languageCode: string): void;

  /**
   * Returns true iff provided languageState is the only blocked language.
   */
  isOnlyTranslateBlockedLanguage(languageState: LanguageState): boolean;

  /**
   * Returns true iff provided languageState can be disabled.
   */
  canDisableLanguage(languageState: LanguageState): boolean;

  /**
   * @return true if the given language can be enabled
   */
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
   * @return The converted language code.
   */
  convertLanguageCodeForTranslate(languageCode: string): string;

  /**
   * Given a language code, returns just the base language. E.g., converts
   * 'en-GB' to 'en'.
   */
  getLanguageCodeWithoutRegion(languageCode: string): string;

  getLanguage(languageCode: string): chrome.languageSettingsPrivate.Language
      |undefined;

  retryDownloadDictionary(languageCode: string): void;

  addInputMethod(id: string): void;

  removeInputMethod(id: string): void;

  setCurrentInputMethod(id: string): void;

  getCurrentInputMethod(): Promise<string>;

  getInputMethodsForLanguage(languageCode: string):
      chrome.languageSettingsPrivate.InputMethod[];

  /**
   * Returns the input methods that support any of the given languages.
   */
  getInputMethodsForLanguages(languageCodes: string[]):
      chrome.languageSettingsPrivate.InputMethod[];

  /**
   * @return list of enabled language code.
   */
  getEnabledLanguageCodes(): Set<string>;

  /**
   * @param id the input method id
   * @return True if the input method is enabled
   */
  isInputMethodEnabled(id: string): boolean;

  isComponentIme(inputMethod: chrome.languageSettingsPrivate.InputMethod):
      boolean;

  /** @param id Input method ID. */
  openInputMethodOptions(id: string): void;

  /**
   * @param id Input method ID.
   */
  getInputMethodDisplayName(id: string): string;

  getImeLanguagePackStatus(id: string):
      chrome.inputMethodPrivate.LanguagePackStatus;
}
