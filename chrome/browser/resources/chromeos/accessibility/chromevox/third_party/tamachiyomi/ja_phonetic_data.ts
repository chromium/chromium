// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides Japanese phonetic disambiguation data for ChromeVox.
 */

interface PrefixInfo {
  delimiter: boolean;
  prefix: string | null;
}

export class JaPhoneticData {
  /** A map containing phonetic disambiguation data for Japanese. */
  private static phoneticMap_: Map<string, string>;

  /** Initialize phoneticMap_ by |map|. */
  static init(map: Map<string, string>): void {
    this.phoneticMap_ = map;
  }

  /** Returns a phonetic reading for |char|. */
  static forCharacter(char: string): string {
    const characterSet =
        JaPhoneticData.getCharacterSet(char, JaPhoneticData.CharacterSet.NONE);
    let resultChar = JaPhoneticData.maybeGetLargeLetterKana(char);
    resultChar = JaPhoneticData.phoneticMap_.get(resultChar) || resultChar;
    const prefix = JaPhoneticData.getPrefixForCharacter(characterSet);
    if (prefix) {
      return prefix + ' ' + resultChar;
    }
    return resultChar;
  }

  /** Returns a phonetic reading for |text|. */
  static forText(text: string): string {
    const result: string[] = [];
    const chars = [...text];
    let lastCharacterSet = JaPhoneticData.CharacterSet.NONE;
    for (const char of chars) {
      const currentCharacterSet =
          JaPhoneticData.getCharacterSet(char, lastCharacterSet);
      const info =
          JaPhoneticData.getPrefixInfo(lastCharacterSet, currentCharacterSet);
      if (info.prefix) {
        // Need to announce the new character set explicitly.
        result.push(info.prefix);
      }

      if (info.delimiter === false && result.length > 0) {
        // Does not convert small Kana if it is not the beginning of the
        // element.
        result[result.length - 1] += char;
      } else if (JaPhoneticData.alwaysReadPhonetically(currentCharacterSet)) {
        result.push(JaPhoneticData.phoneticMap_.get(char) || char);
      } else {
        result.push(JaPhoneticData.maybeGetLargeLetterKana(char));
      }

      lastCharacterSet = currentCharacterSet;
    }
    return result.join(' ');
  }

  static getCharacterSet(character: string, lastCharacterSet: JaPhoneticData.CharacterSet): JaPhoneticData.CharacterSet {
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
    if (character >= '0' && character <= '9') {
      return JaPhoneticData.CharacterSet.HALF_WIDTH_NUMERIC;
    }
    // See https://unicode.org/charts/PDF/U0000.pdf
    if (character >= '!' && character <= '~') {
      return JaPhoneticData.CharacterSet.HALF_WIDTH_SYMBOL;
    }
    if (character >= 'Ａ' && character <= 'Ｚ') {
      return JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_UPPER;
    }
    if (character >= 'ａ' && character <= 'ｚ') {
      return JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_LOWER;
    }
    if (character >= '０' && character <= '９') {
      return JaPhoneticData.CharacterSet.FULL_WIDTH_NUMERIC;
    }
    // See https://unicode.org/charts/PDF/UFF00.pdf
    if (character >= '！' && character <= '～') {
      return JaPhoneticData.CharacterSet.FULL_WIDTH_SYMBOL;
    }
    if (character === 'ー') {
      switch (lastCharacterSet) {
        case JaPhoneticData.CharacterSet.HIRAGANA:
        case JaPhoneticData.CharacterSet.KATAKANA:
        case JaPhoneticData.CharacterSet.HIRAGANA_SMALL_LETTER:
        case JaPhoneticData.CharacterSet.KATAKANA_SMALL_LETTER:
          return lastCharacterSet;
      }
    }
    // See https://www.unicode.org/charts/PDF/U0400.pdf and
    // https://www.unicode.org/charts/PDF/U0370.pdf
    if ((character >= 'А' && character <= 'Я') ||
        (character >= 'Α' && character <= 'Ω')) {
      return JaPhoneticData.CharacterSet.FULL_WIDTH_CYRILLIC_OR_GREEK_UPPER;
    }
    if ((character >= 'а' && character <= 'я') ||
        (character >= 'α' && character <= 'ω')) {
      return JaPhoneticData.CharacterSet.FULL_WIDTH_CYRILLIC_OR_GREEK_LOWER;
    }
    // Returns OTHER for all other characters, including Kanji.
    return JaPhoneticData.CharacterSet.OTHER;
  }

  /**
   * Returns true if the character is part of the small letter character set.
   */
  static isSmallLetter(character: string): boolean {
    return JaPhoneticData.SMALL_TO_LARGE.has(character);
  }

  /** Returns a large equivalent if the character is a small letter of Kana. */
  static maybeGetLargeLetterKana(character: string): string {
    return JaPhoneticData.SMALL_TO_LARGE.get(character) || character;
  }

  static getDefaultPrefix(characterSet: JaPhoneticData.CharacterSet): string {
    // TODO(b/314203187): Not null asserted, check that this is correct.
    return JaPhoneticData.DEFAULT_PREFIX.get(characterSet)!;
  }

  static getPrefixForCharacter(characterSet: JaPhoneticData.CharacterSet): string | null {
    // Removing an annoucement of capital because users can distinguish
    // uppercase and lowercase by capiatalStrategy options.
    switch (characterSet) {
      case JaPhoneticData.CharacterSet.HALF_WIDTH_ALPHABET_UPPER:
      case JaPhoneticData.CharacterSet.HALF_WIDTH_ALPHABET_LOWER:
      case JaPhoneticData.CharacterSet.FULL_WIDTH_CYRILLIC_OR_GREEK_UPPER:
      case JaPhoneticData.CharacterSet.FULL_WIDTH_CYRILLIC_OR_GREEK_LOWER:
      case JaPhoneticData.CharacterSet.OTHER:
        return null;
      case JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_UPPER:
        return 'ゼンカク';
    }
    return JaPhoneticData.getDefaultPrefix(characterSet);
  }

  /**
   * Returns an object containing the relationship between the preceding
   * character set and the current character set.
   * @return Object containing prefixes.
   * delimiter: true if a space between preceding character and current
   * character is necessary. A space leaves a pause so users can recognize that
   * the type of characters has changed.
   * prefix: a string that represents the character set. Null if unncessary.
   */
  static getPrefixInfo(lastCharacterSet: JaPhoneticData.CharacterSet, currentCharacterSet: JaPhoneticData.CharacterSet): PrefixInfo {
    // Don't add prefixes for the same character set except for the sets always
    // read phonetically.
    if (lastCharacterSet === currentCharacterSet) {
      return JaPhoneticData.alwaysReadPhonetically(currentCharacterSet) ?
          {delimiter: true, prefix: null} :
          {delimiter: false, prefix: null};
    }
    // Exceptional cases:
    switch (currentCharacterSet) {
      case JaPhoneticData.CharacterSet.HIRAGANA:
        switch (lastCharacterSet) {
          case JaPhoneticData.CharacterSet.NONE:
          case JaPhoneticData.CharacterSet.HIRAGANA_SMALL_LETTER:
            return {delimiter: false, prefix: null};
          case JaPhoneticData.CharacterSet.HALF_WIDTH_ALPHABET_UPPER:
          case JaPhoneticData.CharacterSet.HALF_WIDTH_ALPHABET_LOWER:
          case JaPhoneticData.CharacterSet.HALF_WIDTH_NUMERIC:
          case JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_UPPER:
          case JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_LOWER:
          case JaPhoneticData.CharacterSet.FULL_WIDTH_NUMERIC:
          case JaPhoneticData.CharacterSet.OTHER:
            return {delimiter: true, prefix: null};
        }
        break;
      case JaPhoneticData.CharacterSet.KATAKANA:
        switch (lastCharacterSet) {
          case JaPhoneticData.CharacterSet.KATAKANA_SMALL_LETTER:
            return {delimiter: false, prefix: null};
        }
        break;
      case JaPhoneticData.CharacterSet.HIRAGANA_SMALL_LETTER:
        switch (lastCharacterSet) {
          case JaPhoneticData.CharacterSet.NONE:
          case JaPhoneticData.CharacterSet.HIRAGANA:
            return {delimiter: false, prefix: null};
          case JaPhoneticData.CharacterSet.OTHER:
            return {delimiter: true, prefix: null};
        }
        break;
      case JaPhoneticData.CharacterSet.KATAKANA_SMALL_LETTER:
        switch (lastCharacterSet) {
          case JaPhoneticData.CharacterSet.KATAKANA:
            return {delimiter: false, prefix: null};
        }
        break;
      case JaPhoneticData.CharacterSet.HALF_WIDTH_KATAKANA:
        switch (lastCharacterSet) {
          case JaPhoneticData.CharacterSet.HALF_WIDTH_KATAKANA_SMALL_LETTER:
            return {delimiter: false, prefix: null};
        }
        break;
      case JaPhoneticData.CharacterSet.HALF_WIDTH_KATAKANA_SMALL_LETTER:
        switch (lastCharacterSet) {
          case JaPhoneticData.CharacterSet.HALF_WIDTH_KATAKANA:
            return {delimiter: false, prefix: null};
        }
        break;
      case JaPhoneticData.CharacterSet.HALF_WIDTH_ALPHABET_UPPER:
        switch (lastCharacterSet) {
          case JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_UPPER:
          case JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_LOWER:
          case JaPhoneticData.CharacterSet.FULL_WIDTH_NUMERIC:
          case JaPhoneticData.CharacterSet.FULL_WIDTH_SYMBOL:
            return {delimiter: true, prefix: 'ハンカクオオモジ'};
        }
        break;
      case JaPhoneticData.CharacterSet.HALF_WIDTH_ALPHABET_LOWER:
      case JaPhoneticData.CharacterSet.HALF_WIDTH_NUMERIC:
      case JaPhoneticData.CharacterSet.HALF_WIDTH_SYMBOL:
        switch (lastCharacterSet) {
          case JaPhoneticData.CharacterSet.NONE:
            return {delimiter: false, prefix: null};
          case JaPhoneticData.CharacterSet.HIRAGANA:
          case JaPhoneticData.CharacterSet.KATAKANA:
          case JaPhoneticData.CharacterSet.HIRAGANA_SMALL_LETTER:
          case JaPhoneticData.CharacterSet.KATAKANA_SMALL_LETTER:
          case JaPhoneticData.CharacterSet.HALF_WIDTH_KATAKANA:
          case JaPhoneticData.CharacterSet.HALF_WIDTH_KATAKANA_SMALL_LETTER:
          case JaPhoneticData.CharacterSet.HALF_WIDTH_ALPHABET_UPPER:
          case JaPhoneticData.CharacterSet.HALF_WIDTH_ALPHABET_LOWER:
          case JaPhoneticData.CharacterSet.HALF_WIDTH_NUMERIC:
          case JaPhoneticData.CharacterSet.HALF_WIDTH_SYMBOL:
          case JaPhoneticData.CharacterSet.OTHER:
            return {delimiter: true, prefix: null};
        }
        break;
      case JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_UPPER:
        switch (lastCharacterSet) {
          case JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_LOWER:
          case JaPhoneticData.CharacterSet.FULL_WIDTH_NUMERIC:
          case JaPhoneticData.CharacterSet.FULL_WIDTH_SYMBOL:
            return {delimiter: true, prefix: 'オオモジ'};
        }
        break;
      case JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_LOWER:
      case JaPhoneticData.CharacterSet.FULL_WIDTH_NUMERIC:
      case JaPhoneticData.CharacterSet.FULL_WIDTH_SYMBOL:
        switch (lastCharacterSet) {
          case JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_UPPER:
          case JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_LOWER:
          case JaPhoneticData.CharacterSet.FULL_WIDTH_NUMERIC:
          case JaPhoneticData.CharacterSet.FULL_WIDTH_SYMBOL:
            return {delimiter: true, prefix: null};
        }
        break;
      case JaPhoneticData.CharacterSet.FULL_WIDTH_CYRILLIC_OR_GREEK_LOWER:
        switch (lastCharacterSet) {
          case JaPhoneticData.CharacterSet.FULL_WIDTH_CYRILLIC_OR_GREEK_UPPER:
            return {
              delimiter: true,
              prefix: JaPhoneticData.getDefaultPrefix(currentCharacterSet),
            };
        }
        return {delimiter: true, prefix: null};
      case JaPhoneticData.CharacterSet.OTHER:
        return {delimiter: true, prefix: null};
    }
    // Returns the default prefix.
    return {
      delimiter: true,
      prefix: JaPhoneticData.getDefaultPrefix(currentCharacterSet),
    };
  }

  /**
   * @param {JaPhoneticData.CharacterSet} characterSet
   * @return {boolean}
   */
  static alwaysReadPhonetically(characterSet: JaPhoneticData.CharacterSet): boolean {
    switch (characterSet) {
      case JaPhoneticData.CharacterSet.HALF_WIDTH_SYMBOL:
      case JaPhoneticData.CharacterSet.FULL_WIDTH_SYMBOL:
      case JaPhoneticData.CharacterSet.FULL_WIDTH_CYRILLIC_OR_GREEK_UPPER:
      case JaPhoneticData.CharacterSet.FULL_WIDTH_CYRILLIC_OR_GREEK_LOWER:
      case JaPhoneticData.CharacterSet.OTHER:
        return true;
    }
    return false;
  }
}

export namespace JaPhoneticData {
  export enum CharacterSet {
    NONE = 0,
    HIRAGANA,                             // 'あ'
    KATAKANA,                             // 'ア'
    HIRAGANA_SMALL_LETTER,                // 'ぁ'
    KATAKANA_SMALL_LETTER,                // 'ァ'
    HALF_WIDTH_KATAKANA,                  // 'ｱ'
    HALF_WIDTH_KATAKANA_SMALL_LETTER,     // 'ｧ'
    HALF_WIDTH_ALPHABET_UPPER,            // 'A'
    HALF_WIDTH_ALPHABET_LOWER,            // 'a'
    HALF_WIDTH_NUMERIC,                   // '1'
    HALF_WIDTH_SYMBOL,                    // '@'
    FULL_WIDTH_ALPHABET_UPPER,            // 'Ａ'
    FULL_WIDTH_ALPHABET_LOWER,            // 'ａ'
    FULL_WIDTH_NUMERIC,                   // '１'
    FULL_WIDTH_SYMBOL,                    // '＠'
    FULL_WIDTH_CYRILLIC_OR_GREEK_UPPER,   // 'Α'
    FULL_WIDTH_CYRILLIC_OR_GREEK_LOWER,   // 'α'
    OTHER,                                // Kanji and unsupported symbols
  }

  export const DEFAULT_PREFIX: Map<CharacterSet, string> = new Map([
    // 'あ'
    [CharacterSet.HIRAGANA, 'ヒラガナ'],
    // 'ア'
    [CharacterSet.KATAKANA, 'カタカナ'],
    // 'ぁ'
    [CharacterSet.HIRAGANA_SMALL_LETTER, 'ヒラガナ チイサイ'],
    // 'ァ'
    [CharacterSet.KATAKANA_SMALL_LETTER, 'カタカナ チイサイ'],
    // 'ｱ'
    [CharacterSet.HALF_WIDTH_KATAKANA, 'ハンカク'],
    // 'ｧ'
    [CharacterSet.HALF_WIDTH_KATAKANA_SMALL_LETTER, 'ハンカク チイサイ'],
    // 'A'
    [CharacterSet.HALF_WIDTH_ALPHABET_UPPER, 'オオモジ'],
    // 'a'
    [CharacterSet.HALF_WIDTH_ALPHABET_LOWER, 'ハンカク'],
    // '1'
    [CharacterSet.HALF_WIDTH_NUMERIC, 'ハンカク'],
    // '@'
    [CharacterSet.HALF_WIDTH_SYMBOL, 'ハンカク'],
    // 'Ａ'
    [CharacterSet.FULL_WIDTH_ALPHABET_UPPER, 'ゼンカクオオモジ'],
    // 'ａ'
    [CharacterSet.FULL_WIDTH_ALPHABET_LOWER, 'ゼンカク'],
    // '１'
    [CharacterSet.FULL_WIDTH_NUMERIC, 'ゼンカク'],
    // '＠'
    [CharacterSet.FULL_WIDTH_SYMBOL, 'ゼンカク'],
    // 'Α'
    [CharacterSet.FULL_WIDTH_CYRILLIC_OR_GREEK_UPPER, 'オオモジ'],
    // 'α'
    [CharacterSet.FULL_WIDTH_CYRILLIC_OR_GREEK_LOWER, 'コモジ'],
  ]);

  /** This object maps small letters of Kana to their large equivalents. */
  export const SMALL_TO_LARGE: Map<string, string> = new Map([
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
}