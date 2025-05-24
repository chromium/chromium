// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides phonetic disambiguation functionality across multiple
 * languages for ChromeVox.
 */
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {PhoneticDictionaries} from '../phonetic_dictionaries.js';
import {JaPhoneticData} from '../third_party/tamachiyomi/ja_phonetic_data.js';

export class PhoneticData {
  /**
   * Returns the phonetic disambiguation for |char| in |locale|.
   * Returns empty string if disambiguation can't be found.
   * @param {string} char
   * @param {string} locale
   * @return {string}
   */
  static forCharacter(char, locale) {
    if (!char || !locale) {
      throw Error('PhoneticData api requires non-empty arguments.');
    }

    if (locale === 'ja') {
      return JaPhoneticData.forCharacter(char);
    }

    char = char.toLowerCase();
    locale = locale.toLowerCase();
    let map = null;
    // Try a lookup using |locale|, but use only the language component if the
    // lookup fails, e.g. "en-us" -> "en" or "zh-hant-hk" -> "zh".
    map = PhoneticDictionaries.phoneticMap_[locale] ||
        PhoneticDictionaries.phoneticMap_[locale.split('-')[0]];

    if (!map) {
      return '';
    }

    return map[char] || '';
  }

  /**
   * @param {string} text
   * @param {string} locale
   * @return {string}
   */
  static forText(text, locale) {
    if (locale === 'ja') {
      // Japanese phonetic readings require specialized logic.
      return JaPhoneticData.forText(text);
    }

    const result = [];
    const chars = [...text];
    for (const char of chars) {
      const phoneticText = PhoneticData.forCharacter(char, locale);
      result.push(char + ': ' + phoneticText);
    }
    return result.join(', ');
  }
}

TestImportManager.exportForTesting(PhoneticData, JaPhoneticData);
