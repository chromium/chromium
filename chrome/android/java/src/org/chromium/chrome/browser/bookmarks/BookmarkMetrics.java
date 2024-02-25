// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowSortOrder;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.profile_metrics.BrowserProfileType;

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
        RecordHistogram.recordEnumeratedHistogram(
                "Bookmarks.MobileBookmarkManager.SortOptionUsed",
                bookmarkRowSortOrder,
                BookmarkRowSortOrder.COUNT);
    }

    /** Report a visuals option was used in the bookmarks manager. */
    public static void reportBookmarkManagerDisplayPrefChanged(
            @BookmarkRowDisplayPref int bookmarkRowDisplayPref) {
        RecordHistogram.recordEnumeratedHistogram(
                "Bookmarks.MobileBookmarkManager.DisplayOptionUsed",
                bookmarkRowDisplayPref,
                BookmarkRowDisplayPref.COUNT);
    }

    /** Report a filter was shown in the bookmarks manager. */
    public static void reportBookmarkManagerFilterShown(
            @BookmarkManagerFilter int bookmarkManagerFilter) {
        RecordHistogram.recordEnumeratedHistogram(
                "Bookmarks.MobileBookmarkManager.FilterShown",
                bookmarkManagerFilter,
                BookmarkManagerFilter.COUNT);
    }

    /** Report a filter was used in the bookmarks manager. */
    public static void reportBookmarkManagerFilterUsed(
            @BookmarkManagerFilter int bookmarkManagerFilter) {
        RecordHistogram.recordEnumeratedHistogram(
                "Bookmarks.MobileBookmarkManager.FilterUsed2",
                bookmarkManagerFilter,
                BookmarkManagerFilter.COUNT);
    }

    /** Report when a bookmark has been added through {@link BookmarkUtils#addBookmarkInternal}. */
    public static void recordBookmarkAdded(Profile profile, BookmarkId bookmarkId) {
        RecordHistogram.recordEnumeratedHistogram(
                "Bookmarks.AddBookmarkType", bookmarkId.getType(), BookmarkType.LAST + 1);

        @BrowserProfileType int type = Profile.getBrowserProfileTypeFromProfile(profile);
        RecordHistogram.recordEnumeratedHistogram(
                "Bookmarks.AddedPerProfileType", type, BrowserProfileType.MAX_VALUE + 1);
    }
}
