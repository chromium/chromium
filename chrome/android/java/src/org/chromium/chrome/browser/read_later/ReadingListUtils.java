// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.read_later;

import android.app.Activity;

import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUndoController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.url.GURL;

/** Utility functions for reading list feature. */
public final class ReadingListUtils {
    private static Boolean sReadingListSupportedForTesting;

    /** Returns whether the URL can be added as reading list article. */
    public static boolean isReadingListSupported(GURL url) {
        if (sReadingListSupportedForTesting != null) return sReadingListSupportedForTesting;
        if (url == null || url.isEmpty() || !url.isValid()) return false;

        // This should match ReadingListModel::IsUrlSupported(), having a separate function since
        // the UI may not load native library.
        return UrlUtilities.isHttpOrHttps(url);
    }

    /** Removes from the reading list the entry for the current tab. */
    public static void deleteFromReadingList(
            SnackbarManager snackbarManager, Activity activity, Tab currentTab) {
        final BookmarkModel bookmarkModel = new BookmarkModel();
        // This undo controller will dismiss itself when any action is taken.
        BookmarkUndoController.createOneshotBookmarkUndoController(
                activity, bookmarkModel, snackbarManager);
        bookmarkModel.finishLoadingBookmarkModel(() -> {
            BookmarkBridge.BookmarkItem bookmarkItem =
                    bookmarkModel.getReadingListItem(currentTab.getOriginalUrl());
            bookmarkModel.deleteBookmarks(bookmarkItem.getId());
        });
    }

    /** For cases where GURLs are faked for testing (e.g. test pages). */
    public static void setReadingListSupportedForTesting(Boolean supported) {
        sReadingListSupportedForTesting = supported;
    }
}
