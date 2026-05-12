// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestImportManager} from './testing/test_import_manager.js';

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

  /**
   * JavaScript strings are encoded in UTF-16, meaning characters can take up 1
   * or 2 code units (for surrogate pairs). Converting a string to UTF-8
   * naturally changes these lengths and indices since a character could take
   * anywhere from 1 to 4 bytes.
   *
   * This builds lookup tables between JS UTF-16 code unit indices and UTF-8
   * byte indices. Useful for mapping byte offsets from UTF-8 based libraries
   * (like WASM modules) back to JS string positions and vice-versa.
   *
   * utf16ToByte[i] = UTF-8 byte offset at the start of JS character i.
   *   Length = str.length + 1; the last entry equals totalBytes.
   * byteToUtf16[b] = JS string index for the character whose UTF-8 encoding
   *   contains byte b.
   *   Length = totalBytes + 1; the last entry equals str.length.
   */
  static buildUtf8OffsetTables(str: string):
      {utf16ToByte: number[], byteToUtf16: number[], totalBytes: number} {
    const utf16ToByte: number[] = [];
    const byteToUtf16: number[] = [];

    // In UTF-16, characters outside the Basic Multilingual Plane (> 0xFFFF,
    // like emojis) are encoded as two 16-bit code units: a "lead surrogate" and
    // a "trail surrogate". These constants define the ranges for these
    // surrogates. We use them to recombine a surrogate pair back into its true
    // Unicode code point: trueCodePoint = (lead - 0xD800) * 0x400 + (trail -
    // 0xDC00) + 0x10000
    const LEAD_SURROGATE_MIN = 0xD800;
    const LEAD_SURROGATE_MAX = 0xDBFF;
    const TRAIL_SURROGATE_MIN = 0xDC00;
    const TRAIL_SURROGATE_MAX = 0xDFFF;
    const SURROGATE_MULTIPLIER = 0x400;
    const SUPPLEMENTARY_PLANE_MIN = 0x10000;

    // Maximum Unicode code point values that can be encoded in 1, 2, or 3 bytes
    // in UTF-8. Code points above UTF8_THREE_BYTE_MAX require 4 bytes.
    const UTF8_ONE_BYTE_MAX = 0x7F;
    const UTF8_TWO_BYTE_MAX = 0x7FF;
    const UTF8_THREE_BYTE_MAX = 0xFFFF;

    let byteIndex = 0;
    for (let i = 0; i < str.length; i++) {
      utf16ToByte.push(byteIndex);

      let codePoint = str.charCodeAt(i);
      let bytesLength = 0;

      // Resolve UTF-16 surrogate pairs into a single Unicode codepoint.
      if (codePoint >= LEAD_SURROGATE_MIN && codePoint <= LEAD_SURROGATE_MAX &&
          i + 1 < str.length) {
        const nextCodePoint = str.charCodeAt(i + 1);
        if (nextCodePoint >= TRAIL_SURROGATE_MIN &&
            nextCodePoint <= TRAIL_SURROGATE_MAX) {
          codePoint = (codePoint - LEAD_SURROGATE_MIN) * SURROGATE_MULTIPLIER +
              (nextCodePoint - TRAIL_SURROGATE_MIN) + SUPPLEMENTARY_PLANE_MIN;
        }
      }

      if (codePoint <= UTF8_ONE_BYTE_MAX) {
        bytesLength = 1;
      } else if (codePoint <= UTF8_TWO_BYTE_MAX) {
        bytesLength = 2;
      } else if (codePoint <= UTF8_THREE_BYTE_MAX) {
        bytesLength = 3;
      } else {
        bytesLength = 4;
      }

      for (let b = 0; b < bytesLength; b++) {
        byteToUtf16.push(i);
      }

      byteIndex += bytesLength;

      // For surrogate pairs, skip the trail surrogate in the next iteration but
      // record its byte position as the end of the 4-byte sequence.
      if (bytesLength === 4) {
        i++;
        utf16ToByte.push(byteIndex);
      }
    }

    utf16ToByte.push(byteIndex);
    byteToUtf16.push(str.length);

    return {utf16ToByte, byteToUtf16, totalBytes: byteIndex};
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

TestImportManager.exportForTesting(StringUtil);
