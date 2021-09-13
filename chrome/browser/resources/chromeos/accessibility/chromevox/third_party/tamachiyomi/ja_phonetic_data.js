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
    if (characterSet !== JaPhoneticData.CharacterSet.OTHER) {
      const prefix = JaPhoneticData.getDefaultPrefix(characterSet);
      return prefix + ' ' + char;
    }
    return JaPhoneticData.phoneticMap_[char] || char;
  }

  /**
   * Returns a phonetic reading for |text|.
   * @param {string} text
   * @return {string}
   */
  static forText(text) {
    const result = [];
    const chars = [...text];
    let lastCharacterSet = JaPhoneticData.CharacterSet.NONE;
    for (const char of chars) {
      const currentCharacterSet = JaPhoneticData.getCharacterSet(char);
      if (currentCharacterSet !== JaPhoneticData.CharacterSet.OTHER) {
        if (currentCharacterSet !== lastCharacterSet) {
          // If the character set has changed, push the prefix first,
          // followed by the character.
          const prefix = JaPhoneticData.getDefaultPrefix(currentCharacterSet);
          result.push(prefix, char);
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
   * @param {string} character
   * @return {JaPhoneticData.CharacterSet}
   */
  static getCharacterSet(character) {
    // See https://www.unicode.org/charts/PDF/U3040.pdf
    if (character >= 'ぁ' && character <= 'ゖ') {
      if (JaPhoneticData.isSmallLetter(character)) {
        return JaPhoneticData.CharacterSet.HIRAGANA_SMALL_LETTER;
      }
      return JaPhoneticData.CharacterSet.HIRAGANA;
    }
    // See https://www.unicode.org/charts/PDF/U30A0.pdf
    if (character >= 'ァ' && character <= 'ヺ') {
      if (JaPhoneticData.isSmallLetter(character)) {
        return JaPhoneticData.CharacterSet.KATAKANA_SMALL_LETTER;
      }
      return JaPhoneticData.CharacterSet.KATAKANA;
    }
    // See https://unicode.org/charts/PDF/UFF00.pdf
    if (character >= 'ｦ' && character <= 'ﾟ') {
      if (JaPhoneticData.isSmallLetter(character)) {
        return JaPhoneticData.CharacterSet.HALF_WIDTH_KATAKANA_SMALL_LETTER;
      }
      return JaPhoneticData.CharacterSet.HALF_WIDTH_KATAKANA;
    }
    if (character >= 'A' && character <= 'Z') {
      return JaPhoneticData.CharacterSet.HALF_WIDTH_ALPHABET_UPPER;
    }
    if (character >= 'a' && character <= 'z') {
      return JaPhoneticData.CharacterSet.HALF_WIDTH_ALPHABET_LOWER;
    }
    if (character >= 'Ａ' && character <= 'Ｚ') {
      return JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_UPPER;
    }
    if (character >= 'ａ' && character <= 'ｚ') {
      return JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_LOWER;
    }
    // Return OTHER for all other characters, including Kanji.
    return JaPhoneticData.CharacterSet.OTHER;
  }

  /**
   * Returns true if the character is part of the small letter character set.
   * @param {string} character
   * @return {boolean}
   */
  static isSmallLetter(character) {
    return JaPhoneticData.SMALL_TO_LARGE.has(character);
  }

  /**
   * @param {JaPhoneticData.CharacterSet} characterSet
   * @return {string}
   */
  static getDefaultPrefix(characterSet) {
    return JaPhoneticData.DEFAULT_PREFIX.get(characterSet);
  }
};

/** @enum {number} */
JaPhoneticData.CharacterSet = {
  NONE: 0,
  HIRAGANA: 1,                          // 'あ'
  KATAKANA: 2,                          // 'ア'
  HIRAGANA_SMALL_LETTER: 3,             // 'ぁ'
  KATAKANA_SMALL_LETTER: 4,             // 'ァ'
  HALF_WIDTH_KATAKANA: 5,               // 'ｱ'
  HALF_WIDTH_KATAKANA_SMALL_LETTER: 6,  // 'ｧ'
  HALF_WIDTH_ALPHABET_UPPER: 7,         // 'A'
  HALF_WIDTH_ALPHABET_LOWER: 8,         // 'a'
  FULL_WIDTH_ALPHABET_UPPER: 9,         // 'Ａ'
  FULL_WIDTH_ALPHABET_LOWER: 10,        // 'ａ'
  OTHER: 11                             // Kanji, number, symbol...
};

/**
 *  @type {Map<JaPhoneticData.CharacterSet, string>}
 *  @const
 */
JaPhoneticData.DEFAULT_PREFIX = new Map([
  [JaPhoneticData.CharacterSet.HIRAGANA, 'ヒラガナ'],  // 'あ'
  [JaPhoneticData.CharacterSet.KATAKANA, 'カタカナ'],  // 'ア'
  [
    JaPhoneticData.CharacterSet.HIRAGANA_SMALL_LETTER,
    'ヒラガナチイサイ'
  ],  // 'ぁ'
  [
    JaPhoneticData.CharacterSet.KATAKANA_SMALL_LETTER,
    'カタカナチイサイ'
  ],                                                              // 'ァ'
  [JaPhoneticData.CharacterSet.HALF_WIDTH_KATAKANA, 'ハンカク'],  // 'ｱ'
  [
    JaPhoneticData.CharacterSet.HALF_WIDTH_KATAKANA_SMALL_LETTER,
    'ハンカクチイサイ'
  ],                                                                    // 'ｧ'
  [JaPhoneticData.CharacterSet.HALF_WIDTH_ALPHABET_UPPER, 'オオモジ'],  // 'A'
  [JaPhoneticData.CharacterSet.HALF_WIDTH_ALPHABET_LOWER, 'ハンカク'],  // 'a'
  [
    JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_UPPER,
    'ゼンカクオオモジ'
  ],                                                                    // 'Ａ'
  [JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_LOWER, 'ゼンカク'],  // 'ａ'
]);

/**
 * This object maps small letters of Kana to their large equivalents.
 * @type {Map<string, string>}
 * @const
 */
JaPhoneticData.SMALL_TO_LARGE = new Map([
  // Hiragana
  ['ぁ', 'あ'],
  ['ぃ', 'い'],
  ['ぅ', 'う'],
  ['ぇ', 'え'],
  ['ぉ', 'お'],
  ['っ', 'つ'],
  ['ゃ', 'や'],
  ['ゅ', 'ゆ'],
  ['ょ', 'よ'],
  ['ゎ', 'わ'],
  ['ゕ', 'か'],
  ['ゖ', 'け'],
  // Katakana
  ['ァ', 'ア'],
  ['ィ', 'イ'],
  ['ゥ', 'ウ'],
  ['ェ', 'エ'],
  ['ォ', 'オ'],
  ['ッ', 'ツ'],
  ['ャ', 'ヤ'],
  ['ュ', 'ユ'],
  ['ョ', 'ヨ'],
  ['ヮ', 'ワ'],
  ['ヵ', 'カ'],
  ['ヶ', 'ケ'],
  // HalfWidthKatakana
  ['ｧ', 'ｱ'],
  ['ｨ', 'ｲ'],
  ['ｩ', 'ｳ'],
  ['ｪ', 'ｴ'],
  ['ｫ', 'ｵ'],
  ['ｬ', 'ﾔ'],
  ['ｭ', 'ﾕ'],
  ['ｮ', 'ﾖ'],
  ['ｯ', 'ﾂ'],
]);
