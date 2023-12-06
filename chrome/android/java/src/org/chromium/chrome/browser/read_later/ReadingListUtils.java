// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.read_later;

import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

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

    /**
     * Performs type swapping on the given bookmarks if necessary. The input list will be modified,
     * removing bookmarks that have been type swapped and thus don't need to be moved.
     *
     * @param bookmarkModel The BookmarkModel to perform add/delete operations.
     * @param bookmarksToMove The List of bookmarks to potentially type swap.
     * @param typeSwappedBookmarks The list of bookmarks which have been type-swapped and thus don't
     *         need to be moved.
     * @param newParentId The new parentId to use, the {@link BookmarkType} of this is used to
     *         determine if type-swapping is necessary.
     */
    public static void typeSwapBookmarksIfNecessary(
            BookmarkModel bookmarkModel,
            List<BookmarkId> bookmarksToMove,
            List<BookmarkId> typeSwappedBookmarks,
            BookmarkId newParentId) {
        List<BookmarkId> outputList = new ArrayList<>();
        while (!bookmarksToMove.isEmpty()) {
            BookmarkId bookmarkId = bookmarksToMove.remove(0);
            if (bookmarkId.getType() == newParentId.getType()) {
                outputList.add(bookmarkId);
                continue;
            }

            BookmarkItem existingBookmark = bookmarkModel.getBookmarkById(bookmarkId);
            BookmarkId newBookmark = null;
            if (newParentId.getType() == BookmarkType.NORMAL) {
                newBookmark =
                        bookmarkModel.addBookmark(
                                newParentId,
                                bookmarkModel.getChildCount(newParentId),
                                existingBookmark.getTitle(),
                                existingBookmark.getUrl());
            } else if (newParentId.getType() == BookmarkType.READING_LIST) {
                newBookmark =
                        bookmarkModel.addToReadingList(
                                newParentId,
                                existingBookmark.getTitle(),
                                existingBookmark.getUrl());
            }

            if (newBookmark == null) {
                Log.e(TAG, "Null bookmark after typeswapping.");
                continue;
            }
            bookmarkModel.deleteBookmark(bookmarkId);
            typeSwappedBookmarks.add(newBookmark);
        }

        bookmarksToMove.addAll(outputList);
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
