// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.contextualTasksPrivate API
 * Generated from: chrome/common/extensions/api/contextual_tasks_private.webidl
 * run `tools/json_schema_compiler/compiler.py
 * chrome/common/extensions/api/contextual_tasks_private.webidl -g
 * ts_definitions` to regenerate.
 */



declare namespace chrome {
  export namespace contextualTasksPrivate {

    export interface ProfileState {
      isEligible: boolean;
    }

    export interface AimParams {
      ntc?: string;
      mstk?: string;
      aioh?: string;
      csuir?: string;
      ved?: string;
      cs?: string;
      sxsrf?: string;
      ei?: string;
    }

    export interface LaunchPanelInNewTabDetails {
      aimParams: AimParams;
      targetUrl: string;
      documentId: string;
    }

    export function getState(documentId: string): Promise<ProfileState>;

    export function launchPanelInNewTab(details: LaunchPanelInNewTabDetails):
        Promise<void>;

  }
}
