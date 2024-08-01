// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Schema} from './schema.js';

export type JsonSerializable =|JsonSerializable[]|boolean|number|string|
  {[key: string]: JsonSerializable}|null;

/**
 * Keys for local storage.
 * Do not delete a key even if it's unused anymore to avoid confliction.
 */
export enum Key {
  SETTINGS = 'settings',
  SUMMARIZATION = 'summarization',

  /**
   * Settings that are only used by dev mode.
   *
   * TODO(pihsun): Move this into platforms/dev/.
   */
  DEV_SETTINGS = 'dev-settings',
}

/**
 * Gets a value from local storage.
 */
export function get<T, I extends JsonSerializable>(
  key: Key,
  schema: Schema<T, I>,
  defaultValue: T,
): T {
  const item = window.localStorage.getItem(key);
  if (item === null) {
    return defaultValue;
  }
  return schema.parseJson(item);
}

/**
 * Sets a value in local storage.
 */
export function set<T, I extends JsonSerializable>(
  key: Key,
  schema: Schema<T, I>,
  value: T,
): void {
  const item = schema.stringifyJson(value);
  window.localStorage.setItem(key, item);
}

/**
 * Removes a value in local storage.
 */
export function remove(key: Key): void {
  window.localStorage.removeItem(key);
}
