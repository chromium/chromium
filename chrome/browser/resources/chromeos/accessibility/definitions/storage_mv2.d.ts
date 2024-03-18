// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.storage API
 * Generated from: extensions/common/api/storage.json
 * run `tools/json_schema_compiler/compiler.py
 * extensions/common/api/storage.json -g ts_definitions` to regenerate.
 */

// This file exists because MV3 supports promises and MV2 does not.
// TODO(b/260590502): Delete this after MV3 migration.

import {ChromeEvent} from '../../../../../../tools/typescript/definitions/chrome_event.js';

declare global {
  export namespace chrome {

    export namespace storage {

      // eslint-disable-next-line @typescript-eslint/naming-convention
      interface sync_StorageArea extends StorageArea {
        readonly QUOTA_BYTES: number;
        readonly QUOTA_BYTES_PER_ITEM: number;
        readonly MAX_ITEMS: number;
        readonly MAX_WRITE_OPERATIONS_PER_HOUR: number;
        readonly MAX_WRITE_OPERATIONS_PER_MINUTE: number;
        readonly MAX_SUSTAINED_WRITE_OPERATIONS_PER_MINUTE: number;
      }
      export const sync: sync_StorageArea;

      // eslint-disable-next-line @typescript-eslint/naming-convention
      interface local_StorageArea extends StorageArea {
        readonly QUOTA_BYTES: number;
      }
      export const local: local_StorageArea;

      export const managed: StorageArea;

      // eslint-disable-next-line @typescript-eslint/naming-convention
      interface session_StorageArea extends StorageArea {
        readonly QUOTA_BYTES: number;
      }
      export const session: session_StorageArea;

      export enum AccessLevel {
        TRUSTED_CONTEXTS = 'TRUSTED_CONTEXTS',
        TRUSTED_AND_UNTRUSTED_CONTEXTS = 'TRUSTED_AND_UNTRUSTED_CONTEXTS',
      }

      export interface StorageChange {
        oldValue?: any;
        newValue?: any;
      }

      export interface StorageArea {
        get(keys: string|string[]|{[key: string]: any}|undefined,
            callback?: (result: {[key: string]: any}) => void): void;
        getBytesInUse(
            keys: string|string[]|undefined,
            callback?: (bytes: number) => void): void;
        set(items: {[key: string]: any}, callback?: () => void): void;
        remove(keys: string|string[], callback?: () => void): void;
        clear(callback: () => void): void;
        setAccessLevel(
            accessOptions: {
              accessLevel: AccessLevel,
            },
            callback: () => void): void;
        onChanged:
            ChromeEvent<(changes: {[key: string]: StorageChange}) => void>;
      }

      export const onChanged: ChromeEvent<
          (changes: {[key: string]: StorageChange}, areaName: string) => void>;
    }
  }
}
