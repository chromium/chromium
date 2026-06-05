// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.glicPrivate API
 * Generated from: chrome/common/extensions/api/glic_private.webidl
 * run `tools/json_schema_compiler/compiler.py
 * chrome/common/extensions/api/glic_private.webidl -g ts_definitions` to
 * regenerate.
 */



declare namespace chrome {
  export namespace glicPrivate {

    export enum ProfileReadyState {
      ERROR = 'ERROR',
      SIGN_IN_REQUIRED = 'SIGN_IN_REQUIRED',
      READY = 'READY',
      DISABLED_BY_ADMIN = 'DISABLED_BY_ADMIN',
      INELIGIBLE = 'INELIGIBLE',
    }

    export interface ProfileState {
      isEnabled: boolean;
      isEnabledAndConsented: boolean;
      readyState: ProfileReadyState;
      liveAllowed: boolean;
      shareImageAllowed: boolean;
      actuationAllowed: boolean;
    }

    export enum InvocationSource {
      INVOCATION_SOURCE_UNKNOWN = 'INVOCATION_SOURCE_UNKNOWN',
      INVOCATION_SOURCE_UNIVERSAL_CART = 'INVOCATION_SOURCE_UNIVERSAL_CART',
      INVOCATION_SOURCE_PROMOTION_PAGE = 'INVOCATION_SOURCE_PROMOTION_PAGE',
    }

    export interface InvokeDetails {
      promptId: string;
      invocationSource: InvocationSource;
      documentId: string;
      inNewTab?: boolean;
    }

    export enum StatusCode {
      SUCCESS = 'SUCCESS',
      LOCAL_INVALID_INVOCATION_SOURCE = 'LOCAL_INVALID_INVOCATION_SOURCE',
      LOCAL_MISSING_PROMPT_ID = 'LOCAL_MISSING_PROMPT_ID',
      SERVER_MISSING_PROMPT = 'SERVER_MISSING_PROMPT',
      HTTP_ERROR = 'HTTP_ERROR',
      PARSE_ERROR = 'PARSE_ERROR',
      LOCAL_BROWSER_CONTEXT_UNAVAILABLE = 'LOCAL_BROWSER_CONTEXT_UNAVAILABLE',
      LOCAL_NO_ACTIVE_TAB = 'LOCAL_NO_ACTIVE_TAB',
      LOCAL_GLIC_NOT_ENABLED = 'LOCAL_GLIC_NOT_ENABLED',
      LOCAL_GLIC_NOT_READY = 'LOCAL_GLIC_NOT_READY',
      LOCAL_GLIC_ACTUATION_NOT_ALLOWED = 'LOCAL_GLIC_ACTUATION_NOT_ALLOWED',
      LOCAL_GLIC_NOT_ENABLED_AND_CONSENTED =
          'LOCAL_GLIC_NOT_ENABLED_AND_CONSENTED',
    }

    export interface GlicInvokeResult {
      statusCode: StatusCode;
    }

    export function getState(documentId: string): Promise<ProfileState>;

    export function invoke(details: InvokeDetails): Promise<GlicInvokeResult>;
  }
}
