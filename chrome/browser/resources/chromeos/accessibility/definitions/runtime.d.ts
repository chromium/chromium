// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.runtime API
 * Generated from: extensions/common/api/runtime.json
 * run `tools/json_schema_compiler/compiler.py
 * extensions/common/api/runtime.json -g ts_definitions` to regenerate.
 */

import {ChromeEvent} from '../../../../../../tools/typescript/definitions/chrome_event.js';

declare global {
  export namespace chrome {

    export namespace runtime {

      export const lastError: {
        message?: string,
      };

      export const id: string;

      export interface Port {
        name: string;
        disconnect: () => void;
        postMessage: (message: any) => void;
        sender?: MessageSender;
        onDisconnect: ChromeEvent<(port: Port) => void>;
        onMessage: ChromeEvent<(message: any, port: Port) => void>;
      }

      export interface MessageSender {
        tab?: tabs.Tab;
        frameId?: number;
        guestProcessId?: number;
        guestRenderFrameRoutingId?: number;
        id?: string;
        url?: string;
        nativeApplication?: string;
        tlsChannelId?: string;
        origin?: string;
        documentId?: string;
        documentLifecycle?: string;
      }

      export enum PlatformOs {
        MAC = 'mac',
        WIN = 'win',
        ANDROID = 'android',
        CROS = 'cros',
        LINUX = 'linux',
        OPENBSD = 'openbsd',
        FUCHSIA = 'fuchsia',
      }

      export enum PlatformArch {
        ARM = 'arm',
        ARM64 = 'arm64',
        X86_32 = 'x86-32',
        X86_64 = 'x86-64',
        MIPS = 'mips',
        MIPS64 = 'mips64',
      }

      export enum PlatformNaclArch {
        ARM = 'arm',
        X86_32 = 'x86-32',
        X86_64 = 'x86-64',
        MIPS = 'mips',
        MIPS64 = 'mips64',
      }

      export interface PlatformInfo {
        os: PlatformOs;
        arch: PlatformArch;
        nacl_arch: PlatformNaclArch;
      }

      export enum RequestUpdateCheckStatus {
        THROTTLED = 'throttled',
        NO_UPDATE = 'no_update',
        UPDATE_AVAILABLE = 'update_available',
      }

      export enum OnInstalledReason {
        INSTALL = 'install',
        UPDATE = 'update',
        CHROME_UPDATE = 'chrome_update',
        SHARED_MODULE_UPDATE = 'shared_module_update',
      }

      export enum OnRestartRequiredReason {
        APP_UPDATE = 'app_update',
        OS_UPDATE = 'os_update',
        PERIODIC = 'periodic',
      }

      export enum ContextType {
        TAB = 'TAB',
        POPUP = 'POPUP',
        BACKGROUND = 'BACKGROUND',
        OFFSCREEN_DOCUMENT = 'OFFSCREEN_DOCUMENT',
        SIDE_PANEL = 'SIDE_PANEL',
      }

      export interface ExtensionContext {
        contextType: ContextType;
        contextId: string;
        tabId: number;
        windowId: number;
        documentId?: string;
        frameId: number;
        documentUrl?: string;
        documentOrigin?: string;
        incognito: boolean;
      }

      export interface ContextFilter {
        contextTypes?: ContextType[];
        contextIds?: string[];
        tabIds?: number[];
        windowIds?: number[];
        documentIds?: string[];
        frameIds?: number[];
        documentUrls?: string[];
        documentOrigins?: string[];
        incognito?: boolean;
      }

      export function getBackgroundPage(): Promise<{[key: string]: any}>;

      export function openOptionsPage(): Promise<void>;

      export function getManifest(): {[key: string]: any};

      export function getURL(path: string): string;

      export function setUninstallURL(url: string): Promise<void>;

      export function reload(): void;

      export function requestUpdateCheck(): Promise<{
        status: RequestUpdateCheckStatus,
        version?: string,
      }>;

      export function restart(): void;

      export function restartAfterDelay(seconds: number): Promise<void>;

      export function connect(extensionId?: string, connectInfo?: {
        name?: string,
        includeTlsChannelId?: boolean,
      }): Port;

      export function connectNative(application: string): Port;

      export function sendMessage(
          extensionId: string|undefined, message: any, options?: {
            includeTlsChannelId?: boolean,
          },
          callback?: Function): Promise<any>;

      export function sendNativeMessage(
          application: string, message: {[key: string]: any}): Promise<any>;

      export function getPlatformInfo(): Promise<PlatformInfo>;

      export function getPackageDirectoryEntry(): Promise<{[key: string]: any}>;

      export function getContexts(filter: ContextFilter):
          Promise<ExtensionContext[]>;

      export const onStartup: ChromeEvent<() => void>;

      export const onInstalled: ChromeEvent<(details: {
                                              reason: OnInstalledReason,
                                              previousVersion?: string,
                                              id?: string,
                                            }) => void>;

      export const onSuspend: ChromeEvent<() => void>;

      export const onSuspendCanceled: ChromeEvent<() => void>;

      export const onUpdateAvailable: ChromeEvent<(details: {
                                                    version: string,
                                                  }) => void>;

      export const onBrowserUpdateAvailable: ChromeEvent<() => void>;

      export const onConnect: ChromeEvent<(port: Port) => void>;

      export const onConnectExternal: ChromeEvent<(port: Port) => void>;

      export const onUserScriptConnect: ChromeEvent<(port: Port) => void>;

      export const onConnectNative: ChromeEvent<(port: Port) => void>;

      export const onMessage: ChromeEvent<
          (message: any|undefined, sender: MessageSender,
           sendResponse: (value: any) => void) => boolean>;

      export const onMessageExternal: ChromeEvent<
          (message: any|undefined, sender: MessageSender) => boolean>;

      export const onUserScriptMessage: ChromeEvent<
          (message: any|undefined, sender: MessageSender) => boolean>;

      export const onRestartRequired:
          ChromeEvent<(reason: OnRestartRequiredReason) => void>;

    }
  }
}
