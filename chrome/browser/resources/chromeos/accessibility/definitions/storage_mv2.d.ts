// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.storage API in Manifest V2 */
// This file exists because MV3 supports promises and MV2 does not.
// TODO(b/260590502): Delete this after MV3 migration.
// TODO(crbug.com/1203307): Auto-generate this file.

import {ChromeEvent} from '../../../../../../tools/typescript/definitions/chrome_event';

declare global {
  export namespace chrome {
    export namespace storage {
      export const sync: StorageArea;
      export const local: StorageArea;
      export const managed: StorageArea;
      export const onChanged: StorageChangeEvent;

      export type StorageChangeEvent = ChromeEvent<
          (changes: {[x: string]: StorageChange}, areaName: string) => void>;

      export type StorageAreaChangeEvent =
          ChromeEvent<(changes: {[x: string]: StorageChange}) => void>;

      export interface StorageChange {
        oldValue?: any;
        newValue?: any;
      }

      export class StorageArea {
        public get(
            keysOrCallback?: string|string[]|object|((obj: any) => void),
            callback?: (obj: any) => void): void;

        public getBytesInUse(
            keysOrCallback?: string|string[]|((obj: any) => void),
            callback?: (num: number) => void): void;

        public set(items: {[x: string]: any}, callback?: () => void): void;
        public remove(keys: string|string[], callback?: () => void): void;
        public clear(callback?: () => void): void;
        public onChanged: StorageAreaChangeEvent;
      }
    }
  }
}
