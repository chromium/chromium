// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utilities for strings.
 */

export class StringUtil {
  static toUpperCamelCase(str: string): string {
    const wordRegex = /(?:^\w|[A-Z]|(?:\b|_)\w)/g;
    const underscoreAndWhitespaceRegex = /(\s|_)+/g;
    return str.replace(wordRegex, word => word.toUpperCase())
        .replace(underscoreAndWhitespaceRegex, '');
  }

  /**
   * Returns the length of the longest common prefix of two strings.
   * TODO(b/319783585): This doesn't work well if there's a character
   * represented with a surrogate pair.
   * @param first The first string.
   * @param second The second string.
   * @return The length of the longest common prefix, which may be 0
   *     for an empty common prefix.
   */
  static longestCommonPrefixLength(first: string, second: string): number {
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
   * Returns the length of the longest common suffix of two strings.
   * TODO(b/319783585): This doesn't work well if there's a character
   * represented with a surrogate pair.
   * @param first The first string.
   * @param second The second string.
   * @return The length of the longest common suffix, which may be 0
   *     for an empty common suffix.
   */
  static longestCommonSuffixLength(first: string, second: string): number {
    const limit = Math.min(first.length, second.length);
    let i;
    for (i = 0; i < limit; ++i) {
      if (first.charAt(first.length - i - 1) !==
          second.charAt(second.length - i - 1)) {
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
   * @param str String of characters.
   * @param offset A valid character index in |str|.
   * @return A valid index of |str| or |str.length|.
   */
  static nextCodePointOffset(str: string, offset: number): number {
    if (offset >= str.length) {
      return str.length;
    }
    // TODO(b/314203187): Not null asserted, check these to make sure this is
    // correct.
    if (str.codePointAt(offset)! > StringUtil.MAX_BMP_CODEPOINT) {
      return offset + 2;
    }
    return offset + 1;
  }

  /**
   * Returns the offset of the first code unit of the last code point before
   * |offset| in a string. If there is no valid code point right before
   * |offset| (including if offset is zero), |offset -1| is returned.
   * @param str String of characters.
   * @param offset A valid character offset into |str|.
   * @return A valid character index into |str| (or -1 in the case
   *     where |offset| is 0).
   */
  static previousCodePointOffset(str: string, offset: number): number {
    if (offset <= 0) {
      return -1;
    }
    // TODO(b/314203187): Not null asserted, check these to make sure this is
    // correct.
    if (offset > 1 &&
        str.codePointAt(offset - 2)! > StringUtil.MAX_BMP_CODEPOINT) {
      return offset - 2;
    }
    return offset - 1;
  }

  static toTitleCase(title: string): string {
    return title.replace(
        /\w\S*/g, word => word.charAt(0).toUpperCase() + word.substr(1));
  }

  /**
   * Converts a camel case string to snake case.
   * @param s A camel case string, e.g. 'brailleTable8'.
   * @return A snake case string, e.g. 'braille_table_8'.
   */
  static camelToSnake(s: string): string {
    return s.replace(/([A-Z0-9])/g, '_$1').toLowerCase();
  }

  /**
   * @param ch The character to test.
   * @return True if a character breaks a word, used to determine
   *     if the previous word should be spoken.
   */
  static isWordBreakChar(ch: string): boolean {
    return Boolean(ch.match(/^\W$/));
  }
}

export namespace StringUtil {
  /**
   * The last code point of the Unicode basic multilingual plane.
   * Code points larger than this value are represented in UTF-16 by a surrogate
   * pair, that is two code units.
   */
  export const MAX_BMP_CODEPOINT = 65535;
}
