// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.app.Activity;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/**
 * Helper class for managing the UI flow for bookmarking the active tab and kicking off the backend.
 * Shows a snackbar if a new bookmark was added. If the bookmark already exists, kicks off edit
 * bookmark UI. Includes price tracking specific UI if the page is relevant for price tracking.
 */
public class TabBookmarker {
    private final Activity mActivity;
    private final Supplier<BookmarkModel> mBookmarkModelSupplier;
    private final Supplier<BottomSheetController> mBottomSheetControllerSupplier;
    private final Supplier<SnackbarManager> mSnackbarManagerSupplier;
    private final boolean mIsCustomTab;

    /**
     * Constructor.
     *
     * @param activity The current activity.
     * @param bookmarkModelSupplier Supplier of the bookmark bridge for the current profile.
     * @param bottomSheetControllerSupplier Supplier of the {@link BottomSheetController} for this
     *     activity.
     * @param snackbarManagerSupplier Supplier of the {@link SnackbarManager}.
     * @param isCustomTab Whether this is a custom tab activity.
     */
    public TabBookmarker(
            @NonNull Activity activity,
            @NonNull ObservableSupplier<BookmarkModel> bookmarkModelSupplier,
            @NonNull Supplier<BottomSheetController> bottomSheetControllerSupplier,
            @NonNull Supplier<SnackbarManager> snackbarManagerSupplier,
            boolean isCustomTab) {
        mActivity = activity;
        mBookmarkModelSupplier = bookmarkModelSupplier;
        mBottomSheetControllerSupplier = bottomSheetControllerSupplier;
        mSnackbarManagerSupplier = snackbarManagerSupplier;
        mIsCustomTab = isCustomTab;
    }

    /**
     * Add the specified tab to bookmarks or allows to edit the bookmark if the specified tab is
     * already bookmarked. If a new bookmark is added, a snackbar will be shown.
     *
     * @param tabToBookmark The tab that needs to be bookmarked.
     */
    public void addOrEditBookmark(final Tab tabToBookmark) {
        addOrEditBookmark(tabToBookmark, BookmarkType.NORMAL, /* fromExplicitTrackUi= */ false);
    }

    /**
     * Adds the specified tab to the Reading List. Opens a new item if an item was added. Opens UI
     * for editing the Reading List item if it was already present on the list.
     *
     * @param tabToAdd The tab that to add to the Reading List.
     */
    public void addToReadingList(final Tab tabToAdd) {
        addOrEditBookmark(tabToAdd, BookmarkType.READING_LIST, /* fromExplicitTrackUi= */ false);
    }

    /**
     * Starts price tracking for the current tab. If the page is already being price tracked, the
     * edit price tracking flow will start.
     *
     * @param currentTab The tab being currently shown.
     */
    public void startOrModifyPriceTracking(Tab currentTab) {
        BookmarkId bookmarkId = mBookmarkModelSupplier.get().getUserBookmarkIdForTab(currentTab);
        if (bookmarkId == null) {
            addOrEditBookmark(currentTab, BookmarkType.NORMAL, /* fromExplicitTrackUi= */ true);
        } else {
            // In the case where the bookmark exists, re-show the save flow with price-tracking
            // enabled.
            assert currentTab != null : "currentTab cannot be null";
            BookmarkUtils.showSaveFlow(
                    mActivity,
                    mBottomSheetControllerSupplier.get(),
                    currentTab.getProfile(),
                    bookmarkId,
                    /* fromExplicitTrackUi= */ true,
                    /* wasBookmarkMoved= */ false,
                    /* isNewBookmark= */ false);
        }
    }

    private void addOrEditBookmark(
            final Tab tabToBookmark, @BookmarkType int bookmarkType, boolean fromExplicitTrackUi) {
        if (tabToBookmark == null || tabToBookmark.isFrozen()) {
            return;
        }

        // Defense in depth against the UI being erroneously enabled.
        final BookmarkModel bookmarkModel = mBookmarkModelSupplier.get();
        if (bookmarkModel == null || !bookmarkModel.isEditBookmarksEnabled()) {
            assert false;
            return;
        }

        bookmarkModel.finishLoadingBookmarkModel(
                () -> {
                    // Gives up the bookmarking if the tab is being destroyed.
                    if (tabToBookmark.isClosing()
                            || !tabToBookmark.isInitialized()
                            || mBottomSheetControllerSupplier.get() == null
                            || mSnackbarManagerSupplier.get() == null) {
                        return;
                    }

                    BookmarkId bookmarkId = bookmarkModel.getUserBookmarkIdForTab(tabToBookmark);
                    BookmarkItem currentBookmarkItem =
                            bookmarkId == null ? null : bookmarkModel.getBookmarkById(bookmarkId);
                    onBookmarkModelLoaded(
                            tabToBookmark,
                            currentBookmarkItem,
                            bookmarkModel,
                            bookmarkType,
                            fromExplicitTrackUi);
                });
    }

    private void onBookmarkModelLoaded(
            final Tab tabToBookmark,
            @Nullable final BookmarkItem currentBookmarkItem,
            final BookmarkModel bookmarkModel,
            @BookmarkType int bookmarkType,
            boolean fromExplicitTrackUi) {
        BookmarkUtils.addOrEditBookmark(
                currentBookmarkItem,
                bookmarkModel,
                tabToBookmark,
                mBottomSheetControllerSupplier.get(),
                mActivity,
                bookmarkType,
                (newBookmarkId) -> {
                    BookmarkId currentBookmarkId =
                            (currentBookmarkItem == null) ? null : currentBookmarkItem.getId();
                    // Add offline page for a new bookmark.
                    if (newBookmarkId != null && !newBookmarkId.equals(currentBookmarkId)) {
                        OfflinePageUtils.saveBookmarkOffline(newBookmarkId, tabToBookmark);
                    }
                },
                fromExplicitTrackUi);
    }
}
