// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.tabs API
 * Generated from: chrome/common/extensions/api/tabs.json
 * run `tools/json_schema_compiler/compiler.py
 * chrome/common/extensions/api/tabs.json -g ts_definitions` to regenerate.
 */

import {ChromeEvent} from '../../../../../../tools/typescript/definitions/chrome_event.js';

declare global {
  export namespace chrome {

    export namespace tabs {

      export const MAX_CAPTURE_VISIBLE_TAB_CALLS_PER_SECOND: number;

      export const TAB_ID_NONE: number;

      export enum TabStatus {
        UNLOADED = 'unloaded',
        LOADING = 'loading',
        COMPLETE = 'complete',
      }

      export enum MutedInfoReason {
        USER = 'user',
        CAPTURE = 'capture',
        EXTENSION = 'extension',
      }

      export interface MutedInfo {
        muted: boolean;
        reason?: MutedInfoReason;
        extensionId?: string;
      }

      export interface Tab {
        id?: number;
        index: number;
        groupId: number;
        windowId: number;
        openerTabId?: number;
        selected: boolean;
        lastAccessed?: number;
        highlighted: boolean;
        active: boolean;
        pinned: boolean;
        audible?: boolean;
        discarded: boolean;
        autoDiscardable: boolean;
        mutedInfo?: MutedInfo;
        url?: string;
        pendingUrl?: string;
        title?: string;
        favIconUrl?: string;
        status?: TabStatus;
        incognito: boolean;
        width?: number;
        height?: number;
        sessionId?: string;
      }

      export enum ZoomSettingsMode {
        AUTOMATIC = 'automatic',
        MANUAL = 'manual',
        DISABLED = 'disabled',
      }

      export enum ZoomSettingsScope {
        PER_ORIGIN = 'per-origin',
        PER_TAB = 'per-tab',
      }

      export interface ZoomSettings {
        mode?: ZoomSettingsMode;
        scope?: ZoomSettingsScope;
        defaultZoomFactor?: number;
      }

      export enum WindowType {
        NORMAL = 'normal',
        POPUP = 'popup',
        PANEL = 'panel',
        APP = 'app',
        DEVTOOLS = 'devtools',
      }

      export function get(tabId: number, callback: (tab: Tab) => void): void;

      export function getCurrent(callback: (tab: Tab) => void): void;

      export function connect(tabId: number, connectInfo?: {
        name?: string,
        frameId?: number,
        documentId?: string,
      }): runtime.Port;

      export function sendRequest(
          tabId: number, request: any, callback: (arg: any) => void): void;

      export function sendMessage(
          tabId: number, message: any, options: {
            frameId?: number,
            documentId?: string,
          }|undefined,
          callback: (response: any) => void): void;

      export function getSelected(
          windowId: number|undefined, callback: (tab: Tab) => void): void;

      export function getAllInWindow(windowId?: number): Promise<Tab[]>;

      export function create(
          createProperties: {
            windowId?: number,
            index?: number,
            url?: string,
            active?: boolean,
            selected?: boolean,
            pinned?: boolean,
            openerTabId?: number,
          },
          callback: (tab: Tab) => void): void;

      export function duplicate(tabId: number, callback: (tab: Tab) => void):
          void;

      export function query(
          queryInfo: {
            active?: boolean,
            pinned?: boolean,
            audible?: boolean,
            muted?: boolean,
            highlighted?: boolean,
            discarded?: boolean,
            autoDiscardable?: boolean,
            currentWindow?: boolean,
            lastFocusedWindow?: boolean,
            status?: TabStatus,
            title?: string,
            url?: string|string[],
            groupId?: number,
            windowId?: number,
            windowType?: WindowType,
            index?: number,
          },
          callback: (tabs: Tab[]) => void): void;

      export function highlight(highlightInfo: {
        windowId?: number, tabs: number[]|number,
      }): Promise<windows.Window>;

      export function update(
          tabId: number|undefined, updateProperties: {
            url?: string,
            active?: boolean,
            highlighted?: boolean,
            selected?: boolean,
            pinned?: boolean,
            muted?: boolean,
            openerTabId?: number,
            autoDiscardable?: boolean,
          },
          callback: (tab: Tab) => void): void;

      export function move(tabIds: number|number[], moveProperties: {
        windowId?: number, index: number,
      }): Promise<Tab|Tab[]>;

      export function reload(
          tabId: number|undefined, reloadProperties: ({
            bypassCache?: boolean,
          }|undefined),
          callback: () => void): void;

      export function remove(tabIds: number|number[], callback: () => void):
          void;

      export function group(
          options: {
            tabIds: number|number[],
            groupId?: number,
            createProperties?: {
              windowId?: number,
            },
          },
          callback: (arg: number) => void): void;

      export function ungroup(tabIds: number|number[], callback: () => void):
          void;

      export function detectLanguage(
          tabId: number|undefined, callback: (arg: string) => void): void;

      export function captureVisibleTab(
          windowId: number|undefined,
          options: extensionTypes.ImageDetails|undefined,
          callback: (arg: string) => void): void;

      export function executeScript(
          tabId: number|undefined, details: extensionTypes.InjectDetails,
          callback?: (result?: any[]) => void): Promise<any[]>;

      export function insertCSS(
          tabId: number|undefined, details: extensionTypes.InjectDetails,
          callback: () => void): void;

      export function removeCSS(
          tabId: number|undefined,
          details: extensionTypes.DeleteInjectionDetails,
          callback: () => void): void;

      export function setZoom(tabId: number|undefined, zoomFactor: number):
          Promise<void>;

      export function getZoom(
          tabId: number|undefined, callback: (arg: number) => void): void;

      export function setZoomSettings(
          tabId: number|undefined, zoomSettings: ZoomSettings,
          callback: () => void): void;

      export function getZoomSettings(
          tabId: number|undefined, callback: (arg: ZoomSettings) => void): void;

      export function discard(
          tabId: number|undefined, callback: (tab: Tab) => void): void;

      export function goForward(tabId: number|undefined, callback: () => void):
          void;

      export function goBack(tabId: number|undefined, callback: () => void):
          void;

      export const onCreated: ChromeEvent<(tab: Tab) => void>;

      export const onUpdated: ChromeEvent<
          (tabId: number, changeInfo: {
            status?: TabStatus,
            url?: string,
            groupId?: number,
            pinned?: boolean,
            audible?: boolean,
            discarded?: boolean,
            autoDiscardable?: boolean,
            mutedInfo?: MutedInfo,
            favIconUrl?: string,
            title?: string,
          },
           tab: Tab) => void>;

      export const onMoved: ChromeEvent<(tabId: number, moveInfo: {
                                          windowId: number,
                                          fromIndex: number,
                                          toIndex: number,
                                        }) => void>;

      export const onSelectionChanged: ChromeEvent<(tabId: number, selectInfo: {
                                                     windowId: number,
                                                   }) => void>;

      export const onActiveChanged: ChromeEvent<(tabId: number, selectInfo: {
                                                  windowId: number,
                                                }) => void>;

      export const onActivated: ChromeEvent<(activeInfo: {
                                              tabId: number,
                                              windowId: number,
                                            }) => void>;

      export const onHighlightChanged: ChromeEvent<(selectInfo: {
                                                     windowId: number,
                                                     tabIds: number[],
                                                   }) => void>;

      export const onHighlighted: ChromeEvent<(highlightInfo: {
                                                windowId: number,
                                                tabIds: number[],
                                              }) => void>;

      export const onDetached: ChromeEvent<(tabId: number, detachInfo: {
                                             oldWindowId: number,
                                             oldPosition: number,
                                           }) => void>;

      export const onAttached: ChromeEvent<(tabId: number, attachInfo: {
                                             newWindowId: number,
                                             newPosition: number,
                                           }) => void>;

      export const onRemoved: ChromeEvent<(tabId: number, removeInfo: {
                                            windowId: number,
                                            isWindowClosing: boolean,
                                          }) => void>;

      export const onReplaced:
          ChromeEvent<(addedTabId: number, removedTabId: number) => void>;

      export const onZoomChange: ChromeEvent<(ZoomChangeInfo: {
                                               tabId: number,
                                               oldZoomFactor: number,
                                               newZoomFactor: number,
                                               zoomSettings: ZoomSettings,
                                             }) => void>;

    }
  }
}
