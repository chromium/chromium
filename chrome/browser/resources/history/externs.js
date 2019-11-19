// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Externs for objects sent from C++ to chrome://history.
 * @externs
 */

/**
 * The type of the history result object. The definition is based on
 * chrome/browser/ui/webui/browsing_history_handler.cc:
 *     BrowsingHistoryHandler::HistoryEntry::ToValue()
 * @typedef {{allTimestamps: Array<number>,
 *            remoteIconUrlForUma: string,
 *            isUrlInRemoteUserData: boolean,
 *            blockedVisit: boolean,
 *            dateRelativeDay: string,
 *            dateShort: string,
 *            dateTimeOfDay: string,
 *            deviceName: string,
 *            deviceType: string,
 *            domain: string,
 *            fallbackFaviconText: string,
 *            hostFilteringBehavior: number,
 *            snippet: string,
 *            starred: boolean,
 *            time: number,
 *            title: string,
 *            url: string}}
 */
let HistoryEntry;

/**
 * The type of the history results info object. The definition is based on
 * chrome/browser/ui/webui/browsing_history_handler.cc:
 *     BrowsingHistoryHandler::QueryComplete()
 * @typedef {{finished: boolean,
 *            term: string}}
 */
let HistoryQuery;

/**
 * The type of the foreign session tab object. This definition is based on
 * chrome/browser/ui/webui/foreign_session_handler.cc:
 * @typedef {{direction: string,
 *            remoteIconUrlForUma: string,
 *            sessionId: number,
 *            timestamp: number,
 *            title: string,
 *            type: string,
 *            url: string}}
 */
let ForeignSessionTab;

/**
 * The type of the foreign session tab object. This definition is based on
 * chrome/browser/ui/webui/foreign_session_handler.cc:
 * @typedef {{timestamp: number,
 *            sessionId: number,
 *            tabs: Array<ForeignSessionTab>}}
 */
let ForeignSessionWindow;

/**
 * The type of the foreign session info object. This definition is based on
 * chrome/browser/ui/webui/foreign_session_handler.cc:
 * @typedef {{collapsed: boolean,
 *            deviceType: string,
 *            name: string,
 *            modifiedTime: string,
 *            tag: string,
 *            timestamp: number,
 *            windows: Array<ForeignSessionWindow>}}
 */
let ForeignSession;

/**
 * @typedef {{incremental: boolean,
 *            querying: boolean,
 *            searchTerm: string}}
 */
let QueryState;

/**
 * @typedef {{info: ?HistoryQuery,
 *            results: ?Array<!HistoryEntry>,
 *            sessionList: ?Array<!ForeignSession>}}
 */
let QueryResult;

/**
 * @constructor
 * @extends {MouseEvent}
 */
const DomRepeatClickEvent = function() {};

/** @type {Object} */
DomRepeatClickEvent.prototype.model;
