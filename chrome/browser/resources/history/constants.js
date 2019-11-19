// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Globals:
/** @type {number} */ const RESULTS_PER_PAGE = 150;

/**
 * Amount of time between pageviews that we consider a 'break' in browsing,
 * measured in milliseconds.
 * @type {number}
 */
const BROWSING_GAP_TIME = 15 * 60 * 1000;

/**
 * The largest bucket value for UMA histogram, based on entry ID. All entries
 * with IDs greater than this will be included in this bucket.
 * Keep consistent with the UMA constants on the Android history page
 * (see HistoryManager.java).
 * @type {number}
 */
const UMA_MAX_BUCKET_VALUE = 1000;

/**
 * The largest bucket value for a UMA histogram that is a subset of above.
 * Keep consistent with the UMA constants on the Android history page
 * (see HistoryManager.java).
 * @type {number}
 */
const UMA_MAX_SUBSET_BUCKET_VALUE = 100;

/**
 * Histogram buckets for UMA tracking of which view is being shown to the user.
 * Keep this in sync with the HistoryPageView enum in histograms.xml.
 * This enum is append-only.
 * @enum {number}
 */
const HistoryPageViewHistogram = {
  HISTORY: 0,
  DEPRECATED_GROUPED_WEEK: 1,
  DEPRECATED_GROUPED_MONTH: 2,
  SYNCED_TABS: 3,
  SIGNIN_PROMO: 4,
  END: 5,  // Should always be last.
};

/** @type {string} */
const SYNCED_TABS_HISTOGRAM_NAME = 'HistoryPage.OtherDevicesMenu';

/**
 * Histogram buckets for UMA tracking of synced tabs. Keep in sync with
 * chrome/browser/ui/webui/foreign_session_handler.h. These values are persisted
 * to logs. Entries should not be renumbered and numeric values should never be
 * reused.
 * @enum {number}
 */
const SyncedTabsHistogram = {
  INITIALIZED: 0,
  SHOW_MENU_DEPRECATED: 1,
  LINK_CLICKED: 2,
  LINK_RIGHT_CLICKED: 3,
  SESSION_NAME_RIGHT_CLICKED_DEPRECATED: 4,
  SHOW_SESSION_MENU: 5,
  COLLAPSE_SESSION: 6,
  EXPAND_SESSION: 7,
  OPEN_ALL: 8,
  HAS_FOREIGN_DATA: 9,
  HIDE_FOR_NOW: 10,
  OPENED_LINK_VIA_CONTEXT_MENU: 11,
  LIMIT: 12  // Should always be the last one.
};
