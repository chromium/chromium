// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export enum VoicePackStatus {
  NONE, //  no language pack available
  EXISTS, // available, and not installed
  INSTALLING, // we've requested a language pack installation
  DOWNLOADED, // language pack downloaded, waiting for update to voices
  INSTALLED, //  we have natural voices for this language
  REMOVED_BY_USER, // user removed this voice pack outside of reading mode
  INSTALL_ERROR,
}

export function createInitialListOfEnabledLanguages(
    browserOrPageBaseLang: string, storedLanguagesPref: string[],
    availableLangs: string[], langOfDefaultVoice: string|undefined): string[] {
  const initialAvailableLanguages: Set<string> = new Set();

  // Add stored prefs to initial list of enabled languages
  for (const lang of storedLanguagesPref) {
    // Find the version of the lang/locale that maps to a language
    const matchingLang =
        convertLangToAnAvailableLangIfPresent(lang, availableLangs);
    if (matchingLang) {
      initialAvailableLanguages.add(matchingLang);
    }
  }

  // Add browserOrPageBaseLang to initial list of enabled languages
  // If there's no locale/base-lang already matching in
  // initialAvailableLanguages, then add one
  const browserPageLangAlreadyPresent = [...initialAvailableLanguages].some(
      lang => lang.startsWith(browserOrPageBaseLang));
  if (!browserPageLangAlreadyPresent) {
    const matchingLangOfBrowserLang = convertLangToAnAvailableLangIfPresent(
        browserOrPageBaseLang, availableLangs);
    if (matchingLangOfBrowserLang) {
      initialAvailableLanguages.add(matchingLangOfBrowserLang);
    }
  }

  // If initialAvailableLanguages is still empty, add the default voice
  // language
  if (initialAvailableLanguages.size === 0) {
    if (langOfDefaultVoice) {
      initialAvailableLanguages.add(langOfDefaultVoice);
    }
  }

  return [...initialAvailableLanguages];
}

export function convertLangToAnAvailableLangIfPresent(
    langOrLocale: string, availableLangs: string[]): string|undefined {
  // Convert everything to lower case
  langOrLocale = langOrLocale.toLowerCase();
  availableLangs = availableLangs.map(lang => lang.toLowerCase());

  if (availableLangs.includes(langOrLocale)) {
    return langOrLocale;
  }

  const baseLang = extractBaseLang(langOrLocale);
  if (availableLangs.includes(baseLang)) {
    return baseLang;
  }

  // See if there are any matching available locales we can default to
  const matchingLocales: string[] = availableLangs.filter(
      availableLang => extractBaseLang(availableLang) === baseLang);
  if (matchingLocales) {
    return matchingLocales[0];
  }

  return undefined;
}

// The following possible values of "status" is a union of enum values of
// enum InstallationState and enum ErrorCode in read_anything.mojom
export function mojoVoicePackStatusToVoicePackStatusEnum(
    mojoPackStatus: string) {
  if (mojoPackStatus === 'kNotInstalled') {
    return VoicePackStatus.EXISTS;
  } else if (mojoPackStatus === 'kInstalling') {
    return VoicePackStatus.INSTALLING;
  } else if (mojoPackStatus === 'kInstalled') {
    return VoicePackStatus.INSTALLED;
  }
  // The success statuses were not sent so return an Error
  // TODO (b/331795122) Handle install errors on the UI
  return VoicePackStatus.INSTALL_ERROR;
}

// The ChromeOS VoicePackManager labels some voices by locale, and some by
// base-language. The request for each needs to be exact, so this function
// converts a locale or language into the code the VoicePackManager expects.
// This is based on the VoicePackManager code here:
// https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/language_packs/language_pack_manager.cc;l=346;drc=31e516b25930112df83bf09d3d2a868200ecbc6d
export function convertLangOrLocaleForVoicePackManager(langOrLocale: string):
    string|undefined {
  langOrLocale = langOrLocale.toLowerCase();
  if (PACK_MANAGER_SUPPORTED_LANGS_AND_LOCALES.has(langOrLocale)) {
    return langOrLocale;
  }

  if (!isBaseLang(langOrLocale)) {
    const baseLang = langOrLocale.substring(0, langOrLocale.indexOf('-'));
    if (PACK_MANAGER_SUPPORTED_LANGS_AND_LOCALES.has(baseLang)) {
      return baseLang;
    }
    const locale = convertUnsupportedBaseLangToSupportedLocale(baseLang);
    if (locale) {
      return locale;
    }
  }

  const locale = convertUnsupportedBaseLangToSupportedLocale(langOrLocale);
  if (locale) {
    return locale;
  }

  return undefined;
}

function convertUnsupportedBaseLangToSupportedLocale(baseLang: string): string|
    undefined {
  // Check if it's a base lang that supports a locale. These are the only
  // languages that have locales in the Pack Manager per the code link above.
  if (['en', 'es', 'pt'].includes(baseLang)) {
    // TODO (b/335691447) Convert from base-lang to locale based on browser
    // prefs. For now, just default to arbitrary locales.
    if (baseLang === 'en') {
      return 'en-us';
    }
    if (baseLang === 'es') {
      return 'es-es';
    }
    if (baseLang === 'pt') {
      return 'pt-br';
    }
  }
  return undefined;
}

// Returns true if input is base lang, and false if it's a locale
function isBaseLang(langOrLocale: string): boolean {
  return !langOrLocale.includes('-');
}

function extractBaseLang(langOrLocale: string): string {
  if (isBaseLang(langOrLocale)) {
    return langOrLocale;
  }
  return langOrLocale.substring(0, langOrLocale.indexOf('-'));
}

// These are from the Pack Manager. Values should be kept in sync with the code
// link above.
export const PACK_MANAGER_SUPPORTED_LANGS_AND_LOCALES = new Set([
  'bn',    'cs', 'da',  'de', 'el', 'en-au', 'en-gb', 'en-us', 'es-es',
  'es-us', 'fi', 'fil', 'fr', 'hi', 'hu',    'id',    'it',    'ja',
  'km',    'ko', 'nb',  'ne', 'nl', 'pl',    'pt-br', 'pt-pt', 'si',
  'sk',    'sv', 'th',  'tr', 'uk', 'vi',    'yue',
]);
