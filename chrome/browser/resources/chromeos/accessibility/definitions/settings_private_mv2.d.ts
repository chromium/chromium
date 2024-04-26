// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.settingsPrivate API in Manifest V2 */
// This file exists because MV3 supports promises and MV2 does not.
// TODO(b/260590502): Delete this after MV3 migration.
// TODO(crbug.com/40179454): Auto-generate this file.

import {ChromeEvent} from '../../../../../../tools/typescript/definitions/chrome_event.js';

declare global {
  export namespace chrome {
    export namespace settingsPrivate {
      export enum PrefType {
        BOOLEAN = 'BOOLEAN',
        NUMBER = 'NUMBER',
        STRING = 'STRING',
        URL = 'URL',
        LIST = 'LIST',
        DICTIONARY = 'DICTIONARY',
      }

      export enum ControlledBy {
        DEVICE_POLICY = 'DEVICE_POLICY',
        USER_POLICY = 'USER_POLICY',
        OWNER = 'OWNER',
        PRIMARY_USER = 'PRIMARY_USER',
        EXTENSION = 'EXTENSION',
        PARENT = 'PARENT',
        CHILD_RESTRICTION = 'CHILD_RESTRICTION',
      }

      export enum Enforcement {
        ENFORCED = 'ENFORCED',
        RECOMMENDED = 'RECOMMENDED',
        PARENT_SUPERVISED = 'PARENT_SUPERVISED',
      }

      // Callback Types
      type GetAllPrefsCallback = (prefs: PrefObject[]) => void;
      type OnPrefSetCallback = (success: boolean) => void;
      type GetPrefCallback = (pref: PrefObject) => void;
      // TODO(crbug.com/40242259) Update existing usages of PrefObject to be typed,
      // removing the need to use any here.
      export interface PrefObject<T = any> {
        key: string;
        type:
            // clang-format off
            T extends boolean ? PrefType.BOOLEAN :
            T extends number ? PrefType.NUMBER :
            T extends string ? PrefType.STRING | PrefType.URL :
            T extends unknown[] ? PrefType.LIST :
            T extends Record<string|number, unknown> ? PrefType.DICTIONARY :
            never;
        // clang-format on
        value: T;
        controlledBy?: ControlledBy;
        controlledByName?: string;
        enforcement?: Enforcement;
        recommendedValue?: T;
        userSelectableValues?: T[];
        userControlDisabled?: boolean;
        extensionId?: string;
        extensionCanBeDisabled?: boolean;
      }

      export function getAllPrefs(callback: GetAllPrefsCallback): void;
      export function getPref(name: string, callback: GetPrefCallback): void;

      export function setPref(
          name: string, value: any, pageId?: string,
          callback?: OnPrefSetCallback): void;

      export function getDefaultZoom(callback: (arg: number) => void): void;
      export function setDefaultZoom(
          zoom: number, callback?: (arg: boolean) => void): void;

      type PrefsCallback = (prefs: PrefObject[]) => void;

      export const onPrefsChanged: ChromeEvent<PrefsCallback>;
    }
  }
}
