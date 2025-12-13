// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {HistorySignInState, SyncState} from './constants.js';

/**
 * @fileoverview Externs for objects sent from C++ to chrome://history.
 */

/**
 * The type of the foreign session tab object. This definition is based on
 * chrome/browser/ui/webui/foreign_session_handler.cc:
 */
export interface ForeignSessionTab {
  direction: string;
  remoteIconUrlForUma: string;
  sessionId: number;
  timestamp: number;
  title: string;
  type: string;
  url: string;
  windowId: number;
}

/**
 * The type of the foreign session tab object. This definition is based on
 * chrome/browser/ui/webui/foreign_session_handler.cc:
 */
export interface ForeignSessionWindow {
  timestamp: number;
  sessionId: number;
  tabs: ForeignSessionTab[];
}

/**
 * The type of the foreign session info object. This definition is based on
 * chrome/browser/ui/webui/foreign_session_handler.cc:
 */
export interface ForeignSession {
  collapsed: boolean;
  name: string;
  modifiedTime: string;
  tag: string;
  timestamp: number;
  windows: ForeignSessionWindow[];
}

/**
 * The type of the history identity state object. This definition is based on
 * chrome/browser/ui/webui/history/history_sign_in_state_watcher.h:
 */
// LINT.IfChange(HistoryIdentityState)
export interface HistoryIdentityState {
  signIn: HistorySignInState;
  tabsSync: SyncState;
  historySync: SyncState;
}
// LINT.ThenChange(/chrome/browser/ui/webui/history/history_login_handler.cc:GetHistoryIdentityStateDict)
