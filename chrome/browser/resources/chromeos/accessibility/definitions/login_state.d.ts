// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.loginState API
 * Generated from: chrome/common/extensions/api/login_state.idl
 * run `tools/json_schema_compiler/compiler.py
 * chrome/common/extensions/api/login_state.idl -g ts_definitions` to
 * regenerate.
 */

import {ChromeEvent} from '../../../../../../tools/typescript/definitions/chrome_event.js';

declare global {
  export namespace chrome {

    export namespace loginState {

      export enum ProfileType {
        SIGNIN_PROFILE = 'SIGNIN_PROFILE',
        USER_PROFILE = 'USER_PROFILE',
      }

      export enum SessionState {
        UNKNOWN = 'UNKNOWN',
        IN_OOBE_SCREEN = 'IN_OOBE_SCREEN',
        IN_LOGIN_SCREEN = 'IN_LOGIN_SCREEN',
        IN_SESSION = 'IN_SESSION',
        IN_LOCK_SCREEN = 'IN_LOCK_SCREEN',
        IN_RMA_SCREEN = 'IN_RMA_SCREEN',
      }

      export function getProfileType(
          callback: (profileType: ProfileType) => void): void;

      export function getSessionState(
          callback: (sessionState: SessionState) => void): void;

      export const onSessionStateChanged:
          ChromeEvent<(sessionState: SessionState) => void>;

    }
  }
}
