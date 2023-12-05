// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.extension API
 * Generated from: chrome/common/extensions/api/extension.json
 * run `tools/json_schema_compiler/compiler.py
 * chrome/common/extensions/api/extension.json -g ts_definitions` to regenerate.
 */

import {ChromeEvent} from '../../../../../../tools/typescript/definitions/chrome_event.js';

declare global {
  export namespace chrome {

    export namespace extension {

      export const lastError: {
        message: string,
      };

      export const inIncognitoContext: boolean;

      export enum ViewType {
        TAB = 'tab',
        POPUP = 'popup',
      }

      export function sendRequest(extensionId: string|undefined, request: any):
          Promise<any>;

      export function getURL(path: string): string;

      export function getViews(fetchProperties?: {
        type?: ViewType,
        windowId?: number,
        tabId?: number,
      }): Array<{[key: string]: any}>;

      export function getBackgroundPage(): {[key: string]: any};

      export function getExtensionTabs(windowId?: number):
          Array<{[key: string]: any}>;

      export function isAllowedIncognitoAccess(
          callback: (isAllowed: boolean) => void): void;

      export function isAllowedFileSchemeAccess(
          callback: (isAllowed: boolean) => void): void;

      export function setUpdateUrlData(data: string): void;

      export const onRequest: ChromeEvent<
          (request: any|undefined, sender: runtime.MessageSender) =>
              Promise<void>>;

      export const onRequestExternal: ChromeEvent<
          (request: any|undefined, sender: runtime.MessageSender) =>
              Promise<void>>;

    }
  }
}
