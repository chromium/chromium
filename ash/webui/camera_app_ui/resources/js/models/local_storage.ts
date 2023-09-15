// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  assertBoolean,
  assertInstanceof,
  assertNumber,
  assertString,
  checkEnumVariant,
} from '../assert.js';
import {LocalStorageKey} from '../type.js';

/**
 * @return The value in storage or |defaultValue| if not found.
 */
function getHelper(key: LocalStorageKey, defaultValue: unknown): unknown {
  const rawValue = window.localStorage.getItem(key);
  if (rawValue === null) {
    return defaultValue;
  }
  return JSON.parse(rawValue);
}

/**
 * @return The object in storage or |defaultValue| if not found.
 */
export function getObject<T>(
    key: LocalStorageKey,
    defaultValue: Record<string, T> = {}): Record<string, T> {
  // We assume that all object written to local storage will be always by CCA,
  // and the same key will corresponds to the same / compatible types, so the
  // type assertion will always hold.
  // TODO(pihsun): actually verify the type at runtime here?
  // eslint-disable-next-line @typescript-eslint/consistent-type-assertions
  return assertInstanceof(getHelper(key, defaultValue), Object) as
      Record<string, T>;
}

/**
 * @return The string in storage or |defaultValue| if not found.
 */
export function getString(key: LocalStorageKey, defaultValue = ''): string {
  return assertString(getHelper(key, defaultValue));
}

/**
 * @return The boolean in storage or |defaultValue| if not found.
 */
export function getBool(key: LocalStorageKey, defaultValue = false): boolean {
  return assertBoolean(getHelper(key, defaultValue));
}

/**
 * @return The number in storage or |defaultValue| if not found.
 */
export function getNumber(key: LocalStorageKey, defaultValue = 0): number {
  return assertNumber(getHelper(key, defaultValue));
}

/**
 * Sets the |value| of localStorage for the given |key|.
 */
export function set(key: LocalStorageKey, value: unknown): void {
  window.localStorage.setItem(key, JSON.stringify(value));
}

/**
 * Removes values of localStorage for the given |keys|.
 */
export function remove(...keys: string[]): void {
  for (const key of keys) {
    window.localStorage.removeItem(key);
  }
}

/**
 * Clears all the items in the local storage.
 */
export function clear(): void {
  window.localStorage.clear();
}

/**
 * Remove undefined keys in enum in local storage.
 */
export function cleanup(): void {
  // Iteration order is not defined and can change upon most mutations. See
  // https://html.spec.whatwg.org/multipage/webstorage.html#the-storage-interface
  const undefinedKeys = [];
  for (let i = 0; i < window.localStorage.length; i++) {
    const key = window.localStorage.key(i);
    if (key !== null && checkEnumVariant(LocalStorageKey, key) === null) {
      undefinedKeys.push(key);
    }
  }
  remove(...undefinedKeys);
}
