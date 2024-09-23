// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Common types/interfaces used throughout ChromeOS Settings
 */

export type Constructor<T> = new (...args: any[]) => T;

export interface PrefsState {
  [key: string]: any;
}

export type UserActionSettingPrefChangeEvent =
    CustomEvent<{prefKey: string, value: any}>;
