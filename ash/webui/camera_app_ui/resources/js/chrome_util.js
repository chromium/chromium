// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Verify |condition| is truthy and return |condition| if so.
 * @template T
 * @param {T} condition A condition to check for truthiness.  Note that this
 *     may be used to test whether a value is defined or not, and we don't want
 *     to force a cast to Boolean.
 * @param {string=} optMessage A message to show on failure.
 * @return {T} A non-null |condition|.
 * @closurePrimitive {asserts.truthy}
 */
export function assert(condition, optMessage) {
  if (!condition) {
    let message = 'Assertion failed';
    if (optMessage) {
      message = message + ': ' + optMessage;
    }
    throw new Error(message);
  }
  return condition;
}

/**
 * Call this from places in the code that should never be reached.
 *
 * For example, handling all the values of enum with a switch() like this:
 *
 *   function getValueFromEnum(enum) {
 *     switch (enum) {
 *       case ENUM_FIRST_OF_TWO:
 *         return first
 *       case ENUM_LAST_OF_TWO:
 *         return last;
 *     }
 *     assertNotReached();
 *     return document;
 *   }
 *
 * This code should only be hit in the case of serious programmer error or
 * unexpected input.
 *
 * @param {string=} optMessage A message to show when this is hit.
 * @closurePrimitive {asserts.fail}
 */
export function assertNotReached(optMessage) {
  assert(false, optMessage || 'Unreachable code hit');
}

// Disables eslint check for closure compiler constructor type.
/* eslint-disable valid-jsdoc */

/**
 * @param {*} value The value to check.
 * @param {function(new: T, ...?)} type A user-defined constructor.
 * @param {string=} optMessage A message to show when this is hit.
 * @return {T}
 * @template T
 */
export function assertInstanceof(value, type, optMessage) {
  // We don't use assert immediately here so that we avoid constructing an error
  // message if we don't have to.
  if (!(value instanceof type)) {
    assertNotReached(
        optMessage ||
        'Value ' + value + ' is not a[n] ' + (type.name || typeof type));
  }
  return value;
}

/* eslint-enable valid-jsdoc */

/**
 * @param {*} value The value to check.
 * @param {string=} optMessage A message to show when this is hit.
 * @return {string}
 */
export function assertString(value, optMessage) {
  // We don't use assert immediately here so that we avoid constructing an error
  // message if we don't have to.
  if (typeof value !== 'string') {
    assertNotReached(optMessage || 'Value ' + value + ' is not a string');
  }
  return /** @type {string} */ (value);
}

/**
 * @param {*} value The value to check.
 * @param {string=} optMessage A message to show when this is hit.
 * @return {number}
 */
export function assertNumber(value, optMessage) {
  // We don't use assert immediately here so that we avoid constructing an error
  // message if we don't have to.
  if (typeof value !== 'number') {
    assertNotReached(optMessage || 'Value ' + value + ' is not a number');
  }
  return /** @type {number} */ (value);
}

/**
 * @param {*} value The value to check.
 * @param {string=} optMessage A message to show when this is hit.
 * @return {boolean}
 */
export function assertBoolean(value, optMessage) {
  // We don't use assert immediately here so that we avoid constructing an error
  // message if we don't have to.
  if (typeof value !== 'boolean') {
    assertNotReached(optMessage || 'Value ' + value + ' is not a boolean');
  }
  return /** @type {boolean} */ (value);
}
