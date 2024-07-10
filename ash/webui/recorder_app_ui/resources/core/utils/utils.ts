// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertExists} from './assert.js';

/**
 * Clamps `val` into the range `[low, high]`.
 */
export function clamp(val: number, low: number, high: number): number {
  return Math.min(high, Math.max(low, val));
}

/**
 * Slices an array into subarrays.
 *
 * A new slice is added when `shouldSlice` returns true on two consecutive
 * elements.
 * TODO(pihsun): Unit test.
 */
export function sliceWhen<T>(
  values: T[],
  shouldSlice: (before: T, after: T) => boolean,
): T[][] {
  if (values.length === 0) {
    return [];
  }

  const ret: T[][] = [];
  let slice: T[] = [assertExists(values[0])];
  for (let i = 1; i < values.length; i++) {
    const val = assertExists(values[i]);
    if (shouldSlice(assertExists(slice[slice.length - 1]), val)) {
      ret.push(slice);
      slice = [];
    }
    slice.push(val);
  }
  ret.push(slice);
  return ret;
}

/**
 * Parses a string into a number.
 *
 * @return The parsed number. Returns null if number parsing failed.
 */
export function parseNumber(val: string|null|undefined): number|null {
  if (val === null || val === undefined) {
    return null;
  }
  const num = Number(val);
  if (isNaN(num)) {
    return null;
  }
  return num;
}

/**
 * Shorten the given string to at most `maxWords` space-delimited words by
 * snipping the middle of string as "(...)".
 */
export function shorten(s: string, maxWords: number): string {
  // Split the string into words, keeping whitespace intact.
  // TODO(shik): Try not to cut in the middle of a sentence. This should be easy
  // once we have accurate speaker label for sections.
  const words = s.match(/\s*\S+\s*/g);

  if (words === null || words.length <= maxWords) {
    return s;
  }

  const half = Math.floor(maxWords / 2);
  const begin = words.slice(0, half).join('');
  const end = words.slice(-half).join('');

  return `${begin}\n(...)\n${end}`;
}

/**
 * Sleeps for the given duration in milliseconds.
 */
export function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => {
    setTimeout(resolve, ms);
  });
}

/**
 * Downloads a file with the given name and content.
 */
export function downloadFile(filename: string, blob: Blob): void {
  const url = URL.createObjectURL(blob);

  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  a.style.display = 'none';

  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);

  URL.revokeObjectURL(url);
}
