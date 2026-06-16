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
      ERROR = 'error',
      SIGN_IN_REQUIRED = 'sign-in-required',
      READY = 'ready',
      DISABLED_BY_ADMIN = 'disabled-by-admin',
      INELIGIBLE = 'ineligible',
      LOCATION_MISMATCH = 'location-mismatch',
      INELIGIBLE_ACCOUNT = 'ineligible-account',
    }

    export interface ProfileState {
      isEnabled: boolean;
      isEnabledAndConsented: boolean;
      readyState: ProfileReadyState;
      liveAllowed: boolean;
      shareImageAllowed: boolean;
      actuationAllowed: boolean;
      userEnableActuationOnWeb: boolean;
      invocationSourceEnabled: boolean;
    }

    export enum InvocationSource {
      UNKNOWN = 'unknown',
      UNIVERSAL_CART = 'universal-cart',
      PROMOTION_PAGE = 'promotion-page',
    }

    export interface GetStateParams {
      invocationSource?: InvocationSource;
    }

    export interface InvokeDetails {
      promptId?: string;
      invocationSource: InvocationSource;
      documentId: string;
      inNewTab?: boolean;
    }

    export enum ErrorCode {
      LOCAL_INVALID_INVOCATION_SOURCE = 'local-invalid-invocation-source',
      LOCAL_MISSING_PROMPT_ID = 'local-missing-prompt-id',
      SERVER_MISSING_PROMPT = 'server-missing-prompt',
      HTTP_ERROR = 'http-error',
      PARSE_ERROR = 'parse-error',
      LOCAL_NO_ACTIVE_TAB = 'local-no-active-tab',
      LOCAL_GLIC_NOT_ENABLED = 'local-glic-not-enabled',
      LOCAL_GLIC_NOT_READY = 'local-glic-not-ready',
      LOCAL_GLIC_ACTUATION_NOT_ALLOWED = 'local-glic-actuation-not-allowed',
      LOCAL_GLIC_NOT_ENABLED_AND_CONSENTED =
          'local-glic-not-enabled-and-consented',
      LOCAL_ACCOUNT_MISMATCH = 'local-account-mismatch',
      LOCAL_INVALID_DOCUMENT_ID = 'local-invalid-document-id',
      LOCAL_CONVERSATION_NOT_FOUND = 'local-conversation-not-found',
      LOCAL_NO_BOUND_TABS = 'local-no-bound-tabs',
      LOCAL_TAB_NOT_IN_WINDOW = 'local-tab-not-in-window',
      LOCAL_GLIC_ACCESS_FROM_PAGE_DISABLED =
          'local-glic-access-from-page-disabled',
    }

    export function getState(documentId: string, params?: GetStateParams):
        Promise<ProfileState>;

    export function invoke(details: InvokeDetails): Promise<void>;

    export function hasConversation(conversationId: string): Promise<boolean>;

    export function activateTabWithConversation(conversationId: string):
        Promise<void>;
  }
}
