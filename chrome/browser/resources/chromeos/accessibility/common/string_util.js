// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utilities for strings.
 */

export class StringUtil {
  /**
   * @param {string} str
   * @return {string}
   */
  static toUpperCamelCase(str) {
    const wordRegex = /(?:^\w|[A-Z]|(?:\b|_)\w)/g;
    const underscoreAndWhitespaceRegex = /(\s|_)+/g;
    return str.replace(wordRegex, word => word.toUpperCase())
        .replace(underscoreAndWhitespaceRegex, '');
  }

  /**
   * Returns the length of the longest common prefix of two strings.
   * @param {string} first The first string.
   * @param {string} second The second string.
   * @return {number} The length of the longest common prefix, which may be 0
   *     for an empty common prefix.
   */
  static longestCommonPrefixLength(first, second) {
    const limit = Math.min(first.length, second.length);
    let i;
    for (i = 0; i < limit; ++i) {
      if (first.charAt(i) !== second.charAt(i)) {
        break;
      }
    }
    return i;
  }

  /**
   * Returns the offset after the code point denoted by |offset|.
   * If |offset| points at a character that is not the first code unit of
   * a valid code point, then |offset + 1| is returned. If there are no
   * characters after the code point denoted by |offset|, then the length of
   * |str| is returned.
   * @param {string} str String of characters.
   * @param {number} offset A valid character index in |str|.
   * @return {number} A valid index of |str| or |str.length|.
   */
  static nextCodePointOffset(str, offset) {
    if (offset >= str.length) {
      return str.length;
    }
    if (str.codePointAt(offset) > StringUtil.MAX_BMP_CODEPOINT) {
      return offset + 2;
    }
    return offset + 1;
  }

  /**
   * Returns the offset of the first code unit of the last code point before
   * |offset| in a string. If there is no valid code point right before
   * |offset| (including if offset is zero), |offset -1| is returned.
   * @param {string} str String of characters.
   * @param {number} offset A valid character offset into |str|.
   * @return {number} A valid character index into |str| (or -1 in the case
   *     where |offset| is 0).
   */
  static previousCodePointOffset(str, offset) {
    if (offset <= 0) {
      return -1;
    }
    if (offset > 1 &&
        str.codePointAt(offset - 2) > StringUtil.MAX_BMP_CODEPOINT) {
      return offset - 2;
    }
    return offset - 1;
  }

  /**
   * Returns a unicode-aware substring of |text|.
   * @param {string} text
   * @param {number} startIndex
   * @param {number} endIndex
   * @return {string}
   */
  static getUnicodeSubstring_(text, startIndex, endIndex) {
    let result = '';
    const textSymbolArray = [...text];
    for (let i = startIndex; i < endIndex; ++i) {
      result += textSymbolArray[i];
    }
    return result;
  }

  /**
   * Converts a camel case string to snake case.
   * @param {string} s A camel case string, e.g. 'brailleTable8'.
   * @return {string} A snake case string, e.g. 'braille_table_8'.
   */
  static camelToSnake(s) {
    return s.replace(/([A-Z0-9])/g, '_$1').toLowerCase();
  }
}

/**
 * The last code point of the Unicode basic multilingual plane.
 * Code points larger than this value are represented in UTF-16 by a surrogate
 * pair, that is two code units.
 * @const {number}
 */
StringUtil.MAX_BMP_CODEPOINT = 65535;
