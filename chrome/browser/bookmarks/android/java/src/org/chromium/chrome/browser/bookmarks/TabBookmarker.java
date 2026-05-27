// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;

import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Objects;
import java.util.function.Supplier;

/**
 * Helper class for managing the UI flow for bookmarking the active tab and kicking off the backend.
 * Shows a snackbar if a new bookmark was added. If the bookmark already exists, kicks off edit
 * bookmark UI. Includes price tracking specific UI if the page is relevant for price tracking.
 */
@NullMarked
public class TabBookmarker {
    private final Activity mActivity;
    private final Supplier<@Nullable BookmarkModel> mBookmarkModelSupplier;
    private final Supplier<BottomSheetController> mBottomSheetControllerSupplier;
    private final Supplier<SnackbarManager> mSnackbarManagerSupplier;
    private final BookmarkManagerOpener mBookmarkManagerOpener;
    private final Supplier<PriceDropNotificationManager> mPriceDropNotificationManagerSupplier;
    private final Supplier<Boolean> mBookmarkBarVisibilitySupplier;

    /**
     * Constructor.
     *
     * @param activity The current activity.
     * @param bookmarkModelSupplier Supplier of the bookmark bridge for the current profile.
     * @param bottomSheetControllerSupplier Supplier of the {@link BottomSheetController} for this
     *     activity.
     * @param snackbarManagerSupplier Supplier of the {@link SnackbarManager}.
     * @param bookmarkManagerOpener Helper to open bookmark activities.
     * @param priceDropNotificationManagerSupplier Supplies the {@link PriceDropNotificationManager}
     *     which manages price drop notifications.
     */
    public TabBookmarker(
            Activity activity,
            NullableObservableSupplier<BookmarkModel> bookmarkModelSupplier,
            Supplier<BottomSheetController> bottomSheetControllerSupplier,
            Supplier<SnackbarManager> snackbarManagerSupplier,
            BookmarkManagerOpener bookmarkManagerOpener,
            Supplier<PriceDropNotificationManager> priceDropNotificationManagerSupplier,
            Supplier<Boolean> bookmarkBarVisibilitySupplier) {
        mActivity = activity;
        mBookmarkModelSupplier = bookmarkModelSupplier;
        mBottomSheetControllerSupplier = bottomSheetControllerSupplier;
        mSnackbarManagerSupplier = snackbarManagerSupplier;
        mBookmarkManagerOpener = bookmarkManagerOpener;
        mPriceDropNotificationManagerSupplier = priceDropNotificationManagerSupplier;
        mBookmarkBarVisibilitySupplier = bookmarkBarVisibilitySupplier;
    }

    /**
     * Add the specified tabs to bookmarks or allows to edit the bookmark if the specified tabs are
     * already bookmarked. If a new bookmark is added, a snackbar will be shown.
     *
     * @param tabsToBookmark The tabs that need to be bookmarked.
     */
    public void addOrEditBookmark(final List<Tab> tabsToBookmark) {
        addOrEditBookmark(tabsToBookmark, BookmarkType.NORMAL, /* fromExplicitTrackUi= */ false);
    }

    public void addOrEditBookmark(final Tab tabToBookmark) {
        addOrEditBookmark(Collections.singletonList(tabToBookmark));
    }

    /**
     * Adds the specified tabs to the Reading List. Opens a new item if an item was added. Opens UI
     * for editing the Reading List item if it was already present on the list.
     *
     * @param tabsToBookmark The tabs that to add to the Reading List.
     */
    public void addToReadingList(final List<Tab> tabsToBookmark) {
        addOrEditBookmark(
                tabsToBookmark, BookmarkType.READING_LIST, /* fromExplicitTrackUi= */ false);
    }

    public void addToReadingList(final Tab tabToAdd) {
        addToReadingList(Collections.singletonList(tabToAdd));
    }

    /**
     * Starts price tracking for the current tab. If the page is already being price tracked, the
     * edit price tracking flow will start.
     *
     * @param currentTab The tab being currently shown.
     */
    public void startOrModifyPriceTracking(@Nullable Tab currentTab) {
        BookmarkId bookmarkId =
                assumeNonNull(mBookmarkModelSupplier.get()).getUserBookmarkIdForTab(currentTab);
        if (bookmarkId == null) {
            addOrEditBookmark(
                    Collections.singletonList(currentTab),
                    BookmarkType.NORMAL,
                    /* fromExplicitTrackUi= */ true);
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
                    /* isNewBookmark= */ false,
                    mBookmarkManagerOpener,
                    mPriceDropNotificationManagerSupplier.get());
        }
    }

    private void addOrEditBookmark(
            final @Nullable List<Tab> tabsToBookmark,
            @BookmarkType int bookmarkType,
            boolean fromExplicitTrackUi) {
        if (tabsToBookmark == null) {
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
                    if (mBottomSheetControllerSupplier.get() == null
                            || mSnackbarManagerSupplier.get() == null) {
                        return;
                    }
                    List<@Nullable BookmarkItem> currentBookmarkItems =
                            new ArrayList<>(tabsToBookmark.size());
                    for (Tab tab : tabsToBookmark) {
                        // Gives up the bookmarking if any tab is being destroyed.
                        if (tab.isClosing() || !tab.isInitialized()) {
                            return;
                        }
                        BookmarkId bookmarkId = bookmarkModel.getUserBookmarkIdForTab(tab);
                        currentBookmarkItems.add(
                                bookmarkId == null
                                        ? null
                                        : bookmarkModel.getBookmarkById(bookmarkId));
                    }

                    onBookmarkModelLoaded(
                            tabsToBookmark,
                            currentBookmarkItems,
                            bookmarkModel,
                            bookmarkType,
                            fromExplicitTrackUi);
                });
    }

    private void onBookmarkModelLoaded(
            final List<Tab> tabsToBookmark,
            final List<@Nullable BookmarkItem> currentBookmarkItems,
            final BookmarkModel bookmarkModel,
            @BookmarkType int bookmarkType,
            boolean fromExplicitTrackUi) {
        BookmarkUtils.addOrEditBookmark(
                currentBookmarkItems,
                bookmarkModel,
                tabsToBookmark,
                mSnackbarManagerSupplier.get(),
                mBottomSheetControllerSupplier.get(),
                mActivity,
                bookmarkType,
                (newBookmarkIds) -> {
                    if (newBookmarkIds == null) return;
                    assert tabsToBookmark.size() == newBookmarkIds.size();
                    for (int i = 0; i < tabsToBookmark.size(); i++) {
                        BookmarkId newBookmarkId = newBookmarkIds.get(i);
                        BookmarkItem currentBookmarkItem = currentBookmarkItems.get(i);
                        BookmarkId currentBookmarkId =
                                (currentBookmarkItem == null) ? null : currentBookmarkItem.getId();
                        // Add offline page for a new bookmark.
                        if (newBookmarkId != null
                                && !Objects.equals(newBookmarkId, currentBookmarkId)) {
                            OfflinePageUtils.saveBookmarkOffline(
                                    newBookmarkId, tabsToBookmark.get(i));
                        }
                    }
                },
                fromExplicitTrackUi,
                mBookmarkManagerOpener,
                mPriceDropNotificationManagerSupplier.get(),
                mBookmarkBarVisibilitySupplier.get());
    }
}
