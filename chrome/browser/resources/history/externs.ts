// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Externs for objects sent from C++ to chrome://history.
 */

/**
 * The type of the debug object nested in the history result object. The
 * definition is based on chrome/browser/ui/webui/browsing_history_handler.cc:
 * HistoryEntryToValue()
 */
export type HistoryEntryDebug = {
  isUrlInLocalDatabase: boolean,
  visitCount: number,
  typedCount: number,
};

/**
 * The type of the history result object. The definition is based on
 * chrome/browser/ui/webui/browsing_history_handler.cc: HistoryEntryToValue()
 */
export type HistoryEntry = {
  allTimestamps: Array<number>,
  remoteIconUrlForUma: string,
  isUrlInRemoteUserData: boolean,
  blockedVisit: boolean,
  dateRelativeDay: string,
  dateShort: string,
  dateTimeOfDay: string,
  deviceName: string,
  deviceType: string,
  domain: string,
  fallbackFaviconText: string,
  hostFilteringBehavior: number,
  snippet: string,
  starred: boolean,
  time: number,
  title: string,
  url: string,
  selected: boolean,
  readableTimestamp: string,
  debug?: HistoryEntryDebug,
};

/**
 * The type of the history results info object. The definition is based on
 * chrome/browser/ui/webui/browsing_history_handler.cc:
 *     BrowsingHistoryHandler::QueryComplete()
 */
export type HistoryQuery = {
  finished: boolean,
  term: string,
};

/**
 * The type of the foreign session tab object. This definition is based on
 * chrome/browser/ui/webui/foreign_session_handler.cc:
 */
export type ForeignSessionTab = {
  direction: string,
  remoteIconUrlForUma: string,
  sessionId: number,
  timestamp: number,
  title: string,
  type: string,
  url: string,
  windowId: number,
};

/**
 * The type of the foreign session tab object. This definition is based on
 * chrome/browser/ui/webui/foreign_session_handler.cc:
 */
export type ForeignSessionWindow = {
  timestamp: number,
  sessionId: number,
  tabs: Array<ForeignSessionTab>,
};

/**
 * The type of the foreign session info object. This definition is based on
 * chrome/browser/ui/webui/foreign_session_handler.cc:
 */
export type ForeignSession = {
  collapsed: boolean,
  deviceType: string,
  name: string,
  modifiedTime: string,
  tag: string,
  timestamp: number,
  windows: Array<ForeignSessionWindow>,
};

export type QueryState = {
  incremental: boolean,
  querying: boolean,
  searchTerm: string,
};

export type QueryResult = {
  info?: HistoryQuery,
  results?: Array<HistoryEntry>,
  sessionList?: Array<ForeignSession>,
};
