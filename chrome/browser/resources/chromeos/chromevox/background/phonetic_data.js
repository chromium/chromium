// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides phonetic disambiguation functionality across multiple
 * languages for ChromeVox.
 *
 */

goog.provide('PhoneticData');

goog.require('JaPhoneticData');

/**
 * Maps languages to their phonetic maps.
 * @type {Object<string,Object<string,string>>}
 * @private
 */
PhoneticData.phoneticMap_ = {};

/**
 * Initialization function for PhoneticData.
 */
PhoneticData.init = function() {
  try {
    // The UI language of the browser. This corresponds to the system language
    // set by the user. Behind the scenes, the getUIlanguage() API retrieves the
    // locale that was passed from the browser to the renderer via the --lang
    // command line flag.
    var browserUILanguage = chrome.i18n.getUILanguage().toLowerCase();
    // Phonetic disambiguation data for the browserUI language.
    // This is loaded from a chromevox_strings_*.xtb file, where * is a variable
    // language code that corresponds to the system language.
    var browserUILanguagePhoneticMap = /** @type {Object<string,string>} */
        (JSON.parse(Msgs.getMsg('phonetic_map')));
    PhoneticData.phoneticMap_[browserUILanguage] = browserUILanguagePhoneticMap;
  } catch (e) {
    console.log('Error: unable to parse phonetic map message.');
  }
  PhoneticData.phoneticMap_['ja'] = JaPhoneticData.phoneticMap_;
};

/**
 * Returns the phonetic disambiguation for the provided character in the
 * provided language. Returns empty string if disambiguation can't be found.
 * @param {string} language
 * @param {string} character
 * @return {string}
 */
PhoneticData.getPhoneticDisambiguation = function(language, character) {
  if (!language || !character) {
    return '';
  }
  language = language.toLowerCase();
  character = character.toLowerCase();
  // If language isn't in the map, try stripping extra information, such as the
  // country and/or script codes (e.g. "en-us" or "zh-hant-hk") and use only the
  // language code to do a lookup.
  if (!PhoneticData.phoneticMap_[language]) {
    language = language.split('-')[0];
  }
  // If language still isn't in the map, return empty string.
  if (!PhoneticData.phoneticMap_[language]) {
    return '';
  }
  return PhoneticData.phoneticMap_[language][character] || '';
};
