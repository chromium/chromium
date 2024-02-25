// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview TypeScript helper functions that aid in type assertions,
 * type narrowing, etc.
 */

import {assert, assertInstanceof, assertNotReached} from 'chrome://resources/js/assert.js';

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
 * Ensures that `value` can't exist at both compile time and run time.
 *
 * This is useful for checking that all cases of a type are checked, such as
 * enums in switch statements:
 *
 * ```
 * declare const val: Enum.A|Enum.B;
 * switch (val) {
 *   case Enum.A:
 *   case Enum.B:
 *     break;
 *   default:
 *     assertExhaustive(val);
 * }
 * ```
 *
 * or with manual type checks:
 *
 * ```
 * declare const val: string|number;
 * if (typeof val === 'string') {
 *   // ...
 * } else if (typeof val === 'number') {
 *   // ...
 * } else {
 *   assertExhaustive(val);
 * }
 * ```
 *
 * @param value The value to be checked.
 * @param message An optional message to throw with the error.
 */
export function assertExhaustive(
    value: never, message: string = `Unexpected value ${value}.`): never {
  assertNotReached(message);
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
