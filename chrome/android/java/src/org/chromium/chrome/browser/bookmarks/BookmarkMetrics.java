// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowSortOrder;

/** Metrics utils for use in bookmarks. */
public class BookmarkMetrics {
    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused. Keep up-to-date with the MobileBookmarkManagerFilter enum in
    // tools/metrics/histograms/enums.xml.
    @IntDef({BookmarkManagerFilter.SHOPPING, BookmarkManagerFilter.COUNT})
    public @interface BookmarkManagerFilter {
        int SHOPPING = 0;
        // Not reported to, but needed to avoid DCHECKs before another filter is added.
        int PLACEHOLDER = 1;
        int COUNT = 2;
    }

    /** Report a sort option was used in the bookmarks manager. */
    public static void reportBookmarkManagerSortChanged(
            @BookmarkRowSortOrder int bookmarkRowSortOrder) {
        RecordHistogram.recordEnumeratedHistogram("Bookmarks.MobileBookmarkManager.SortOptionUsed",
                bookmarkRowSortOrder, BookmarkRowSortOrder.COUNT);
    }

    /** Report a visuals option was used in the bookmarks manager. */
    public static void reportBookmarkManagerDisplayPrefChanged(
            @BookmarkRowDisplayPref int bookmarkRowDisplayPref) {
        RecordHistogram.recordEnumeratedHistogram(
                "Bookmarks.MobileBookmarkManager.DisplayOptionUsed", bookmarkRowDisplayPref,
                BookmarkRowDisplayPref.COUNT);
    }

    /** Report a filter was used in the bookmarks manager. */
    public static void reportBookmarkManagerFilterUsed(
            @BookmarkManagerFilter int bookmarkManagerFilter) {
        RecordHistogram.recordEnumeratedHistogram("Bookmarks.MobileBookmarkManager.FilterUsed",
                bookmarkManagerFilter, BookmarkManagerFilter.COUNT);
    }
}
