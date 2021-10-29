// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.read_later;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkItem;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUndoController;
import org.chromium.chrome.browser.bookmarks.ReadingListFeatures;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

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

    /**
     * Determines if the given {@link BookmarkId} is a type-swappable reading list item.
     * @param id The BookmarkId to check.
     * @return Whether the given {@link BookmarkId} is a type-swappable reading list item.
     */
    public static boolean isSwappableReadingListItem(@Nullable BookmarkId id) {
        return ReadingListFeatures.shouldAllowBookmarkTypeSwapping()
                && id.getType() == BookmarkType.READING_LIST;
    }

    /**
     * Performs type swapping on the given bookmarks if necessary.
     *
     * @param bookmarkBridge The BookmarkBridge to perform add/delete operations.
     * @param bookmarksToMove The List of bookmarks to potentially type swap.
     * @param newParentId The new parentId to use, the {@link BookmarkType} of this is used to
     *         determine if type-swapping is necessary.
     * @return The new list of bookmarks, some of which may have swapped types.
     */
    public static List<BookmarkId> typeSwapBookmarksIfNecessary(BookmarkBridge bookmarkBridge,
            List<BookmarkId> bookmarksToMove, BookmarkId newParentId) {
        if (!ReadingListFeatures.shouldAllowBookmarkTypeSwapping()) return bookmarksToMove;

        List<BookmarkId> newList = new ArrayList<>();
        for (int i = 0; i < bookmarksToMove.size(); i++) {
            BookmarkId bookmarkId = bookmarksToMove.get(i);
            if (bookmarkId.getType() == newParentId.getType()) {
                newList.add(bookmarkId);
                continue;
            }

            BookmarkItem existingBookmark = bookmarkBridge.getBookmarkById(bookmarkId);
            BookmarkId newBookmark = null;
            if (newParentId.getType() == BookmarkType.NORMAL) {
                newBookmark = bookmarkBridge.addBookmark(newParentId,
                        bookmarkBridge.getChildCount(newParentId), existingBookmark.getTitle(),
                        existingBookmark.getUrl());
            } else if (newParentId.getType() == BookmarkType.READING_LIST) {
                newBookmark = bookmarkBridge.addToReadingList(
                        existingBookmark.getTitle(), existingBookmark.getUrl());
            }

            if (newBookmark != null) {
                bookmarkBridge.deleteBookmark(bookmarkId);
                newList.add(newBookmark);
            }
        }
        return newList;
    }

    /** For cases where GURLs are faked for testing (e.g. test pages). */
    public static void setReadingListSupportedForTesting(Boolean supported) {
        sReadingListSupportedForTesting = supported;
    }
}
