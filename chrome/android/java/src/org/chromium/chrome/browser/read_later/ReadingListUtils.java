// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.read_later;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.url.GURL;

/** Utility functions for reading list feature. */
public final class ReadingListUtils {
    private static final String TAG = "ReadingListUtils";

    private static Boolean sReadingListSupportedForTesting;
    private static Boolean sSkipShowSaveFlowForTesting;

    /** Returns whether the URL can be added as reading list article. */
    public static boolean isReadingListSupported(GURL url) {
        if (sReadingListSupportedForTesting != null) return sReadingListSupportedForTesting;
        if (url == null || url.isEmpty() || !url.isValid()) return false;

        // This should match ReadingListModel::IsUrlSupported(), having a separate function since
        // the UI may not load native library.
        return UrlUtilities.isHttpOrHttps(url);
    }

    /** For cases where GURLs are faked for testing (e.g. test pages). */
    public static void setReadingListSupportedForTesting(Boolean supported) {
        sReadingListSupportedForTesting = supported;
        ResettersForTesting.register(() -> sReadingListSupportedForTesting = null);
    }

    /**
     * Opens the Reading list folder in the bookmark manager.
     *
     * @param isIncognito Whether the bookmark manager should open in incognito mode.
     */
    public static void showReadingList(boolean isIncognito) {
        BookmarkUtils.showBookmarkManager(
                null, new BookmarkId(0, BookmarkType.READING_LIST), /* isIncognito= */ isIncognito);
    }

    /** For cases where we don't want to mock the entire bookmarks save flow infra. */
    public static void setSkipShowSaveFlowForTesting(Boolean skipShowSaveFlowForTesting) {
        sSkipShowSaveFlowForTesting = skipShowSaveFlowForTesting;
        ResettersForTesting.register(() -> sSkipShowSaveFlowForTesting = null);
    }
}
