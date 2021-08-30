// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides Japanese phonetic disambiguation data for ChromeVox.
 */

goog.provide('JaPhoneticData');

JaPhoneticData = class {
  constructor() {}

  /**
   * Initialize phoneticMap_ by |map|.
   * @param {Object<string,string>} map
   */
  static init(map) {
    /**
     * An object containing phonetic disambiguation data for Japanese.
     * @private {Object<string,string>}
     */
    this.phoneticMap_ = map;
  }

  /**
   * Returns a phonetic reading for |char|.
   * @param {string} char
   * @return {string}
   */
  static forCharacter(char) {
    const characterSet = JaPhoneticData.getCharacterSet(char);
    if (characterSet) {
      return characterSet + ' ' + char;
    } else {
      return JaPhoneticData.phoneticMap_[char] || char;
    }
  }

  /**
   * Returns a phonetic reading for |text|.
   * @param {string} text
   * @return {string}
   */
  static forText(text) {
    const result = [];
    const chars = [...text];
    let lastCharacterSet = null;
    for (const char of chars) {
      const currentCharacterSet = JaPhoneticData.getCharacterSet(char);
      if (currentCharacterSet) {
        if (currentCharacterSet !== lastCharacterSet) {
          // If the character set has changed, push the character set first,
          // followed by the character.
          result.push(currentCharacterSet, char);
        } else {
          const lastEntry = result[result.length - 1];
          if (lastEntry) {
            // If the character set stayed the same, then append the character
            // to the last entry in the array.
            result[result.length - 1] = lastEntry.concat(char);
          }
        }
      } else {
        result.push(JaPhoneticData.phoneticMap_[char] || char);
      }

      lastCharacterSet = currentCharacterSet;
    }
    return result.join(' ');
  }

  /**
   * @param {string} char
   * @return {?JaPhoneticData.CharacterSet}
   */
  static getCharacterSet(char) {
    if (JaPhoneticData.isHiragana(char)) {
      return JaPhoneticData.CharacterSet.HIRAGANA;
    } else if (JaPhoneticData.isKatakana(char)) {
      return JaPhoneticData.CharacterSet.KATAKANA;
    } else if (JaPhoneticData.isHalfWidth(char)) {
      return JaPhoneticData.CharacterSet.HALF_WIDTH;
    } else {
      // Return null for all other characters, including Kanji.
      return null;
    }
  }

  /**
   * Returns true if the character is part of the Hiragana character set.
   * Please see the following resource detailing Hiragana unicode values:
   * https://www.unicode.org/charts/PDF/U3040.pdf
   * @return {boolean}
   */
  static isHiragana(character) {
    if (character >= '\u3040' && character <= '\u3096') {
      // The range above only applies to characters あ - ゖ.
      return true;
    }
    return false;
  }

  /**
   * Returns true if the character is part of the Katakana character set.
   * Please see the following resource detailing Katakana unicode values:
   * https://www.unicode.org/charts/PDF/U30A0.pdf
   * @return {boolean}
   */
  static isKatakana(character) {
    if (character >= '\u30a0' && character <= '\u30fa') {
      // The range above only applies to characters ア - ヺ.
      return true;
    }
    return false;
  }

  /**
   * Returns true if the character is part of the half-width kana character set.
   * Please see the following resource detailing half-width kana unicode values:
   * https://unicode.org/charts/PDF/UFF00.pdf
   * @return {boolean}
   */
  static isHalfWidth(character) {
    if (character >= '\uff61' && character <= '\uff9f') {
      return true;
    }
    return false;
  }
};

/** @enum {string} */
JaPhoneticData.CharacterSet = {
  HIRAGANA: 'ひらがな',
  KATAKANA: 'カタカナ',
  HALF_WIDTH: 'ハンカク',
};
