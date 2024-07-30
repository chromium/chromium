// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.windows API
 * Generated from: chrome/common/extensions/api/windows.json
 * run `tools/json_schema_compiler/compiler.py
 * chrome/common/extensions/api/windows.json -g ts_definitions` to regenerate.
 */

import {ChromeEvent} from '../../../../../../tools/typescript/definitions/chrome_event.js';

declare global {
  export namespace chrome {

    export namespace windows {

      export const WINDOW_ID_NONE: number;

      export const WINDOW_ID_CURRENT: number;

      export enum WindowType {
        NORMAL = 'normal',
        POPUP = 'popup',
        PANEL = 'panel',
        APP = 'app',
        DEVTOOLS = 'devtools',
      }

      export enum WindowState {
        NORMAL = 'normal',
        MINIMIZED = 'minimized',
        MAXIMIZED = 'maximized',
        FULLSCREEN = 'fullscreen',
        LOCKED_FULLSCREEN = 'locked-fullscreen',
      }

      export interface Window {
        id?: number;
        focused: boolean;
        top?: number;
        left?: number;
        width?: number;
        height?: number;
        tabs?: tabs.Tab[];
        incognito: boolean;
        type?: WindowType;
        state?: WindowState;
        alwaysOnTop: boolean;
        sessionId?: string;
      }

      export enum CreateType {
        NORMAL = 'normal',
        POPUP = 'popup',
        PANEL = 'panel',
      }

      export interface QueryOptions {
        populate?: boolean;
        windowTypes?: WindowType[];
      }

      export function get(
          windowId: number, queryOptions?: QueryOptions,
          callback?: (window: Window) => void): void;

      export function getCurrent(
          queryOptions?: QueryOptions,
          callback?: (window: Window) => void): void;

      export function getLastFocused(
          queryOptions?: QueryOptions,
          callback?: (window: Window) => void): void;

      export function getAll(
          queryOptionsOrCallback?: QueryOptions|((windows: Window[]) => void),
          callback?: (windows: Window[]) => void): void;

      export function create(
          createData?: {
            url?: string|string[],
            tabId?: number,
            left?: number,
            top?: number,
            width?: number,
            height?: number,
            focused?: boolean,
            incognito?: boolean,
            type?: CreateType,
            state?: WindowState,
            setSelfAsOpener?: boolean,
          },
          callback?: (window: Window) => void): void;

      export function update(
          windowId: number, updateInfo: {
            left?: number,
            top?: number,
            width?: number,
            height?: number,
            focused?: boolean,
            drawAttention?: boolean,
            state?: WindowState,
          },
          callback?: (window: Window) => void): void;

      export function remove(windowId: number, callback?: () => void): void;

      export const onCreated: ChromeEvent<(window: Window) => void>;

      export const onRemoved: ChromeEvent<(windowId: number) => void>;

      export const onFocusChanged: ChromeEvent<(windowId: number) => void>;

      export const onBoundsChanged: ChromeEvent<(window: Window) => void>;

    }
  }
}
