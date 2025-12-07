// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

/**
 * Formats a number as a fixed-point string with a maximum number of digits
 * after the decimal point
 *
 * If the input number is an integer or has fewer number of digits than
 * `maxFloatDigits`, the function returns its original string representation.
 *
 * @param num The number to format.
 * @param maxFloatDigits The maximum number of digits to display after the
 *                       decimal point. Must be in the range 0 - 20, inclusive.
 *
 * @returns A string representation of the number in fixed-point notation.
 */
export function toFixedFloat(num: number, maxFloatDigits: number): string {
  assert(maxFloatDigits >= 0 && maxFloatDigits <= 20);
  const numStr = num.toString();
  const decimalIndex = numStr.indexOf('.');
  // No need to truncate.
  if (decimalIndex === -1 ||
      numStr.length - decimalIndex - 1 <= maxFloatDigits) {
    return numStr;
  }
  return num.toFixed(maxFloatDigits);
}
