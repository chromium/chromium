// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview TypeScript helper functions that aid in type assertions,
 * type narrowing, etc.
 */

import {assert, assertInstanceof} from 'chrome://resources/js/assert_ts.js';

/**
 * @param arg An argument to check for existence.
 * @throws If |arg| is undefined or null.
 */
export function assertExists<T>(
    arg: T, message: string = `Expected ${arg} to be defined.`):
    asserts arg is NonNullable<T> {
  assert(arg !== undefined && arg !== null, message);
}

/**
 * @param arg A argument to check the type of.
 * @return |arg| with the type narrowed to |type|
 * @throws If |arg| is not an instance of |type|
 */
export function cast<T>(
    arg: unknown, type: {new (...args: any): T}, message?: string): T {
  assertInstanceof(arg, type, message);
  return arg;
}

/**
 * @param arg A argument to check for existence.
 * @return |arg| with the type narrowed as non-nullable.
 * @throws If |arg| is undefined or null.
 */
export function castExists<T>(arg: T, message?: string): NonNullable<T> {
  assertExists(arg, message);
  return arg;
}
