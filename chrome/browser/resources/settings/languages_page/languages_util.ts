// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For some codes translate uses a different version from Chrome.  Some are
// ISO 639 codes that have been renamed (e.g. "he" to "iw"). While others are
// languages that Translate considers similar (e.g. "nb" and "no").
// See also: components/language/core/common/language_util.cc.
const kChromeToTranslateCode: Map<string, string> = new Map([
  ['fil', 'tl'],
  ['he', 'iw'],
  ['jv', 'jw'],
  ['kok', 'gom'],
  ['nb', 'no'],
]);

// Reverse of the map above. Just the languages code that translate uses but
// Chrome has a different code for.
const kTranslateToChromeCode: Map<string, string> = new Map([
  ['gom', 'kok'],
  ['iw', 'he'],
  ['jw', 'jv'],
  ['no', 'nb'],
  ['tl', 'fil'],
]);

/**
 * Given a language code, returns just the base language without sub-codes. For
 * example, converts 'en-GB' to 'en'.
 */
export function getBaseLanguage(languageCode: string): string {
  return languageCode.split('-')[0];
}

/**
 * Converts deprecated ISO 639 language codes to Chrome format.
 */
export function convertLanguageCodeForChrome(languageCode: string): string {
  return kTranslateToChromeCode.get(languageCode) || languageCode;
}

/**
 * Converts the language code to Translate server format where some deprecated
 * ISO 639 codes are used. The only sub-codes that Translate supports are for
 * "zh" where zh-HK is equivalent to zh-TW. For all other languages only
 * the base language is returned.
 */
export function convertLanguageCodeForTranslate(languageCode: string): string {
  const base = getBaseLanguage(languageCode);
  if (base === 'zh') {
    return languageCode === 'zh-HK' ? 'zh-TW' : languageCode;
  }

  return kChromeToTranslateCode.get(base) || base;
}

/**
 * @return the [displayName] - [nativeDisplayName] if displayName and
 *     nativeDisplayName are different. If they're the same than only returns
 *     the displayName.
 */
export function getFullName(language: chrome.languageSettingsPrivate.Language):
    string {
  let fullName = language.displayName;
  if (language.displayName !== language.nativeDisplayName) {
    fullName += ' - ' + language.nativeDisplayName;
  }
  return fullName;
}

/**
 *  @return True if the language is supported by Translate as a base and not
 * an extended sub-code (i.e. "it-CH" and "es-MX" are both marked as
 * supporting translation but only "it" and "es" are actually supported by the
 * Translate server.
 */
export function isTranslateBaseLanguage(
    language: chrome.languageSettingsPrivate.Language): boolean {
  // The language must be marked as translatable.
  if (!language.supportsTranslate) {
    return false;
  }

  if (language.code === 'zh-CN' || language.code === 'zh-TW') {
    // In Translate, general Chinese is not used, and the sub code is
    // necessary as a language code for the Translate server.
    return true;
  }

  if (language.code === 'mni-Mtei') {
    // Translate uses the Meitei Mayek script for Manipuri
    return true;
  }

  const baseLanguage = getBaseLanguage(language.code);
  if (baseLanguage === 'nb') {
    // Norwegian Bokmål (nb) is listed as supporting translate but the
    // Translate server only supports Norwegian (no).
    return false;
  }
  // For all other languages only base languages are supported
  return language.code === baseLanguage;
}
