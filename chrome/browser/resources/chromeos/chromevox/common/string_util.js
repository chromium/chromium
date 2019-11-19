// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utilities for strings.
 */

goog.provide('StringUtil');

/**
 * @constructor
 */
StringUtil = function() {};

/**
 * Returns the length of the longest common prefix of two strings.
 * @param {string} first The first string.
 * @param {string} second The second string.
 * @return {number} The length of the longest common prefix, which may be 0
 *     for an empty common prefix.
 */
StringUtil.longestCommonPrefixLength = function(first, second) {
  var limit = Math.min(first.length, second.length);
  var i;
  for (i = 0; i < limit; ++i) {
    if (first.charAt(i) != second.charAt(i)) {
      break;
    }
  }
  return i;
};

/**
 * The last code point of the Unicode basic multilingual plane.
 * Code points larger than this value are represented in UTF-16 by a surrogate
 * pair, that is two code units.
 * @const {number}
 */
StringUtil.MAX_BMP_CODEPOINT = 65535;

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
StringUtil.nextCodePointOffset = function(str, offset) {
  if (offset >= str.length) {
    return str.length;
  }
  if (str.codePointAt(offset) > StringUtil.MAX_BMP_CODEPOINT) {
    return offset + 2;
  }
  return offset + 1;
};

/**
 * Returns the offset of the first code unit of the last code point before
 * |offset| in a string. If there is no valid code point right before
 * |offset| (including if offset is zero), |offset -1| is returned.
 * @param {string} str String of characters.
 * @param {number} offset A valid character offset into |str|.
 * @return {number} A valid character index into |str| (or -1 in the case
 *     where |offset| is 0).
 */
StringUtil.previousCodePointOffset = function(str, offset) {
  if (offset <= 0) {
    return -1;
  }
  if (offset > 1 &&
      str.codePointAt(offset - 2) > StringUtil.MAX_BMP_CODEPOINT) {
    return offset - 2;
  }
  return offset - 1;
};
