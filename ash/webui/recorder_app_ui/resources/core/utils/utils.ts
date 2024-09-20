// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertExists} from './assert.js';

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
 * Returns the number of space-delimited words in a given string.
 *
 * TODO(hsuanling): Apply different logic so that it can work for languages like
 * Chinese or Japanese.
 */
export function getWordCount(s: string): number {
  const words = s.match(/\S+/g);
  return words?.length ?? 0;
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

const UNINITIALIZED = Symbol('UNINITIALIZED');

/**
 * Cache function return value so the function would only be called once.
 */
export function lazyInit<T>(fn: () => T): () => T {
  let output: T|typeof UNINITIALIZED = UNINITIALIZED;
  return () => {
    if (output === UNINITIALIZED) {
      output = fn();
    }
    return output;
  };
}

/**
 * Cache async function return value so the function would only be called once.
 */
export function asyncLazyInit<T>(fn: () => Promise<T>): () => Promise<T> {
  let val: T|typeof UNINITIALIZED = UNINITIALIZED;
  return async () => {
    if (val === UNINITIALIZED) {
      val = await fn();
    }
    return val;
  };
}

/**
 * Cache function return value so the function would only be called when the
 * input changes.
 *
 * This can be used when the input is expected to not change often.
 */
export function cacheLatest<T, U>(fn: (input: T) => U): (input: T) => U {
  let output: U|typeof UNINITIALIZED = UNINITIALIZED;
  let lastInput: T|typeof UNINITIALIZED = UNINITIALIZED;
  return (input: T) => {
    if (input !== lastInput) {
      lastInput = input;
      output = fn(input);
    }
    assert(output !== UNINITIALIZED);
    return output;
  };
}

/**
 * Checks if an Object is empty.
 */
export function isObjectEmpty(obj: Record<string, unknown>): boolean {
  // We're explicitly using for (... in ...) here to avoid the cost of having
  // to initialize Object.keys() array. The usage is safe since we check
  // Object.hasOwn afterwards.
  // eslint-disable-next-line no-restricted-syntax
  for (const k in obj) {
    if (Object.hasOwn(obj, k)) {
      return false;
    }
  }
  return true;
}

/**
 * Wrapper for the View Transition API for unsupported browser.
 *
 * @return Promise when the transition is finished.
 */
export function startViewTransition(fn: () => void): Promise<void> {
  if (document.startViewTransition === undefined) {
    fn();
    return Promise.resolve();
  } else {
    const transition = document.startViewTransition(fn);
    return transition.finished;
  }
}
