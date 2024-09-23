// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.clipboard API
 * Generated from: extensions/common/api/clipboard.idl
 * This file exists because MV3 supports promises and MV2 does not.
 * TODO(b/260590502): Delete this after MV3 migration.
 * run `tools/json_schema_compiler/compiler.py
 * extensions/common/api/clipboard.idl -g ts_definitions` to regenerate.
 */

import {ChromeEvent} from '../../../../../../tools/typescript/definitions/chrome_event.js';

declare global {
  export namespace chrome {

    export namespace clipboard {

      export enum ImageType {
        PNG = 'png',
        JPEG = 'jpeg',
      }

      export enum DataItemType {
        TEXT_PLAIN = 'textPlain',
        TEXT_HTML = 'textHtml',
      }

      export interface AdditionalDataItem {
        type: DataItemType;
        data: string;
      }

      export function setImageData(
          imageData: ArrayBuffer, type: ImageType,
          additionalItems: AdditionalDataItem[]|undefined,
          callback: () => void): void;

      export const onClipboardDataChanged: ChromeEvent<() => void>;

    }
  }
}
