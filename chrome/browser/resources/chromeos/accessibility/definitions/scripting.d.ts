// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.scripting API
 * Generated from: chrome/common/extensions/api/scripting.idl
 * run `tools/json_schema_compiler/compiler.py
 * chrome/common/extensions/api/scripting.idl -g ts_definitions` to regenerate.
 */



declare namespace chrome {
  export namespace scripting {

    export const globalParams: number;

    export enum StyleOrigin {
      AUTHOR = 'AUTHOR',
      USER = 'USER',
    }

    export enum ExecutionWorld {
      ISOLATED = 'ISOLATED',
      MAIN = 'MAIN',
    }

    export interface InjectionTarget {
      tabId: number;
      frameIds?: number[];
      documentIds?: string[];
      allFrames?: boolean;
    }

    export interface ScriptInjection {
      func?: () => void;
      args?: any[];
      function?: () => void;
      files?: string[];
      target: InjectionTarget;
      world?: ExecutionWorld;
      injectImmediately?: boolean;
    }

    export interface CSSInjection {
      target: InjectionTarget;
      css?: string;
      files?: string[];
      origin?: StyleOrigin;
    }

    export interface InjectionResult {
      result?: any;
      frameId: number;
      documentId: string;
    }

    export interface RegisteredContentScript {
      id: string;
      matches?: string[];
      excludeMatches?: string[];
      css?: string[];
      js?: string[];
      allFrames?: boolean;
      matchOriginAsFallback?: boolean;
      runAt?: extensionTypes.RunAt;
      persistAcrossSessions?: boolean;
      world?: ExecutionWorld;
    }

    export interface ContentScriptFilter {
      ids?: string[];
    }

    export function executeScript(injection: ScriptInjection):
        Promise<InjectionResult[]>;

    export function insertCSS(injection: CSSInjection): Promise<void>;

    export function removeCSS(injection: CSSInjection): Promise<void>;

    export function registerContentScripts(scripts: RegisteredContentScript[]):
        Promise<void>;

    export function getRegisteredContentScripts(filter?: ContentScriptFilter):
        Promise<RegisteredContentScript[]>;

    export function unregisterContentScripts(filter?: ContentScriptFilter):
        Promise<void>;

    export function updateContentScripts(scripts: RegisteredContentScript[]):
        Promise<void>;

  }
}
