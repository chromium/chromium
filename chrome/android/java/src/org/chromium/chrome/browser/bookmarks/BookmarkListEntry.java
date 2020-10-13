// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkItem;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

import javax.annotation.Nonnull;

/**
 * Represents different type of views in the bookmark UI.
 */
final class BookmarkListEntry {
    /**
     * Specifies the view types that the bookmark delegate screen can contain.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ViewType.INVALID, ViewType.PERSONALIZED_SIGNIN_PROMO, ViewType.PERSONALIZED_SYNC_PROMO,
            ViewType.SYNC_PROMO, ViewType.FOLDER, ViewType.BOOKMARK})
    @interface ViewType {
        int INVALID = -1;
        int PERSONALIZED_SIGNIN_PROMO = 0;
        int PERSONALIZED_SYNC_PROMO = 1;
        int SYNC_PROMO = 2;
        int FOLDER = 3;
        int BOOKMARK = 4;
    }

    private final @ViewType int mViewType;
    @Nullable
    private final BookmarkItem mBookmarkItem;

    private BookmarkListEntry(int viewType, @Nullable BookmarkItem bookmarkItem) {
        this.mViewType = viewType;
        this.mBookmarkItem = bookmarkItem;
    }

    /**
     * Create an entry presenting a bookmark folder or bookmark.
     * @param bookmarkItem The data object created from the bookmark backend.
     */
    static BookmarkListEntry createBookmarkEntry(@Nonnull BookmarkItem bookmarkItem) {
        return new BookmarkListEntry(
                bookmarkItem.isFolder() ? ViewType.FOLDER : ViewType.BOOKMARK, bookmarkItem);
    }

    /**
     * Create an entry presenting a sync promo header.
     * @param viewType The view type of the sync promo header.
     */
    static BookmarkListEntry createSyncPromoHeader(@ViewType int viewType) {
        assert viewType == ViewType.PERSONALIZED_SIGNIN_PROMO
                || viewType == ViewType.PERSONALIZED_SYNC_PROMO || viewType == ViewType.SYNC_PROMO;
        return new BookmarkListEntry(viewType, /*bookmarkItem=*/null);
    }

    /**
     * Returns the view type used in the bookmark list UI.
     */
    @ViewType
    int getViewType() {
        return mViewType;
    }

    /**
     * Returns the view type used in the bookmark list UI. Can be null for non bookmark view types.
     */
    @Nullable
    BookmarkItem getBookmarkItem() {
        return mBookmarkItem;
    }
}