// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertBoolean, assertInstanceof, assertString} from '../assert.js';

/**
 * @param {string} key
 * @param {*} defaultValue
 * @return {*} The value in storage or defaultValue if not found.
 */
function getHelper(key, defaultValue) {
  const rawValue = window.localStorage.getItem(key);
  if (rawValue === null) {
    return defaultValue;
  }
  return JSON.parse(rawValue);
}

/**
 * @param {string} key
 * @param {!Object=} defaultValue
 * @return {!Object} The object in storage or defaultValue if not found.
 */
export function getObject(key, defaultValue = {}) {
  return assertInstanceof(getHelper(key, defaultValue), Object);
}

/**
 * @param {string} key
 * @param {string=} defaultValue
 * @return {string} The string in storage or defaultValue if not found.
 */
export function getString(key, defaultValue = '') {
  return assertString(getHelper(key, defaultValue));
}
/**
 * @param {string} key
 * @param {boolean=} defaultValue
 * @return {boolean} The boolean in storage or defaultValue if not found.
 */
export function getBool(key, defaultValue = false) {
  return assertBoolean(getHelper(key, defaultValue));
}

/**
 * @param {string} key
 * @param {*} value
 */
export function set(key, value) {
  window.localStorage.setItem(key, JSON.stringify(value));
}

/**
 * @param {...string} keys
 */
export function remove(...keys) {
  for (const key of keys) {
    window.localStorage.removeItem(key);
  }
}

/**
 * Clears all the items in the local storage.
 */
export function clear() {
  window.localStorage.clear();
}
