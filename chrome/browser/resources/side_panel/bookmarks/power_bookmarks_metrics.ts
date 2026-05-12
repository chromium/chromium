// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SortOrder, ViewType} from './bookmarks.mojom-webui.js';

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
export enum SearchAction {
  // LINT.IfChange(SearchAction)
  SHOWN = 0,
  SEARCHED = 1,

  // Must be last.
  COUNT = 2,
  // LINT.ThenChange(//tools/metrics/histograms/metadata/bookmarks/enums.xml:BookmarksSidePanelSearchCTREvent)
}

const ADD_FOLDER_ACTION_UMA = 'Bookmarks.FolderAddedFromSidePanel';
const ADD_URL_ACTION_UMA = 'Bookmarks.AddedFromSidePanel';

export function recordFolderAdded() {
  chrome.metricsPrivate.recordUserAction(ADD_FOLDER_ACTION_UMA);
}

export function recordBookmarkAdded() {
  chrome.metricsPrivate.recordUserAction(ADD_URL_ACTION_UMA);
}

export function recordSearchCTR(action: SearchAction) {
  chrome.metricsPrivate.recordEnumerationValue(
      'PowerBookmarks.SidePanel.Search.CTR', action, SearchAction.COUNT);
}

export function recordSortType(sortOrder: SortOrder) {
  chrome.metricsPrivate.recordEnumerationValue(
      'PowerBookmarks.SidePanel.SortTypeShown', sortOrder, SortOrder.kCount);
}

export function recordViewType(isCompact: boolean) {
  const viewType = isCompact ? ViewType.kCompact : ViewType.kExpanded;
  chrome.metricsPrivate.recordEnumerationValue(
      'PowerBookmarks.SidePanel.ViewTypeShown', viewType, ViewType.kCount);
}

export function recordBookmarksShown(
    count: number, hasSomeActiveFilter: boolean) {
  const metricName = `PowerBookmarks.SidePanel${
      hasSomeActiveFilter ? '.SearchOrFilter' : ''}.BookmarksShown`;
  chrome.metricsPrivate.recordMediumCount(metricName, count);
}
