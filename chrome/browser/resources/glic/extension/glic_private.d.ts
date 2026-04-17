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

    export function getState(): Promise<ProfileState>;

  }
}
