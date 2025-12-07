// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export const RESULTS_PER_PAGE = 150;

/**
 * Amount of time between pageviews that we consider a 'break' in browsing,
 * measured in milliseconds.
 */
export const BROWSING_GAP_TIME = 15 * 60 * 1000;

/**
 * This enum is used to differentiate all the relevant sign-in/history-sync
 * states.
 */
// LINT.IfChange(HistorySignInState)
export enum HistorySignInState {
  SIGNED_OUT = 0,
  WEB_ONLY_SIGNED_IN = 1,
  SIGNED_IN_NOT_SYNCING_TABS = 2,
  SIGNED_IN_SYNCING_TABS = 3,
  SIGN_IN_PENDING_NOT_SYNCING_TABS = 4,
  SIGN_IN_PENDING_SYNCING_TABS = 5,
  SYNC_DISABLED = 6,
}
// LINT.ThenChange(/chrome/browser/ui/webui/history/history_sign_in_state_watcher.h:HistorySignInState)

/**
 * Histogram buckets for UMA tracking of which view is being shown to the user.
 * Keep this in sync with the HistoryPageView enum in histograms.xml.
 * This enum is append-only.
 */
export enum HistoryPageViewHistogram {
  HISTORY = 0,
  DEPRECATED_GROUPED_WEEK = 1,
  DEPRECATED_GROUPED_MONTH = 2,
  SYNCED_TABS = 3,
  SIGNIN_PROMO = 4,
  JOURNEYS = 5,
  PRODUCT_SPECIFICATIONS_LISTS = 6,
  END = 7,  // Should always be last.
}

export const SYNCED_TABS_HISTOGRAM_NAME = 'HistoryPage.OtherDevicesMenu';

/**
 * Histogram buckets for UMA tracking of synced tabs. Keep in sync with
 * chrome/browser/ui/webui/foreign_session_handler.h. These values are persisted
 * to logs. Entries should not be renumbered and numeric values should never be
 * reused.
 */
export enum SyncedTabsHistogram {
  INITIALIZED = 0,
  SHOW_MENU_DEPRECATED = 1,
  LINK_CLICKED = 2,
  LINK_RIGHT_CLICKED = 3,
  SESSION_NAME_RIGHT_CLICKED_DEPRECATED = 4,
  SHOW_SESSION_MENU = 5,
  COLLAPSE_SESSION = 6,
  EXPAND_SESSION = 7,
  OPEN_ALL = 8,
  HAS_FOREIGN_DATA = 9,
  HIDE_FOR_NOW = 10,
  OPENED_LINK_VIA_CONTEXT_MENU = 11,
  LIMIT = 12  // Should always be the last one.
}

/**
 * Contains all context menu interactions for a visit in the history page.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the VisitContextMenuAction enum in
 * histograms/metadata/history/enums.xml
 */
// LINT.IfChange(VisitContextMenuAction)
export enum VisitContextMenuAction {
  MORE_FROM_THIS_SITE_CLICKED = 0,
  REMOVE_FROM_HISTORY_CLICKED = 1,
  REMOVE_BOOKMARK_CLICKED = 2,
  MAX_VALUE = 3,
}
// LINT.ThenChange(/tools/metrics/histograms/metadata/history/enums.xml:VisitContextMenuAction)
