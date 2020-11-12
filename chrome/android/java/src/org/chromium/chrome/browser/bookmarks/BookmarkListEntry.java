// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
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
            ViewType.SYNC_PROMO, ViewType.FOLDER, ViewType.BOOKMARK, ViewType.DIVIDER,
            ViewType.SECTION_HEADER})
    @interface ViewType {
        int INVALID = -1;
        int PERSONALIZED_SIGNIN_PROMO = 0;
        int PERSONALIZED_SYNC_PROMO = 1;
        int SYNC_PROMO = 2;
        int FOLDER = 3;
        int BOOKMARK = 4;
        int DIVIDER = 5;
        int SECTION_HEADER = 6;
    }

    private final @ViewType int mViewType;
    @Nullable
    private final BookmarkItem mBookmarkItem;
    @Nullable
    private String mHeaderTitle;
    @Nullable
    private String mHeaderDescription;

    private BookmarkListEntry(int viewType, @Nullable BookmarkItem bookmarkItem) {
        this.mViewType = viewType;
        this.mBookmarkItem = bookmarkItem;
    }

    /** Constructor for section headers. */
    private BookmarkListEntry(
            int viewType, String headerTitle, @Nullable String headerDescription) {
        assert viewType == ViewType.SECTION_HEADER;
        this.mViewType = viewType;
        this.mBookmarkItem = null;
        this.mHeaderTitle = headerTitle;
        this.mHeaderDescription = headerDescription;
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
     * Creates a divider to separate sections in the bookmark list.
     */
    static BookmarkListEntry createDivider() {
        return new BookmarkListEntry(ViewType.DIVIDER, /*bookmarkItem=*/null);
    }

    /**
     * Helper function that returns whether the view type represents a bookmark or bookmark folder.
     * Returns false for other view holder types like divider, promo headers, etc.
     * @param viewType The type of the view in the bookmark list UI.
     */
    static boolean isBookmarkEntry(@ViewType int viewType) {
        return viewType == ViewType.BOOKMARK || viewType == ViewType.FOLDER;
    }

    /**
     * Create an entry representing the reading list read/unread section header.
     * @param titleRes The string resource for title text.
     * @param descriptionRes The string resource for description.
     * @param context The context to use.
     */
    static BookmarkListEntry createSectionHeader(@StringRes Integer titleRes,
            @Nullable @StringRes Integer descriptionRes, Context context) {
        return new BookmarkListEntry(ViewType.SECTION_HEADER, context.getString(titleRes),
                descriptionRes == null ? null : context.getString(descriptionRes));
    }

    /**
     * Create an entry representing the reading list read/unread section header.
     * @param read True if it represents read section, false for unread section.
     */
    static BookmarkListEntry createReadingListSectionHeader(boolean read) {
        Context context = ContextUtils.getApplicationContext();
        return read ? new BookmarkListEntry(
                       ViewType.SECTION_HEADER, context.getString(R.string.reading_list_read), null)
                    : new BookmarkListEntry(ViewType.SECTION_HEADER,
                            context.getString(R.string.reading_list_unread),
                            context.getString(R.string.reading_list_ready_for_offline));
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

    /** @return The title text to be shown if it is a section header. */
    @Nullable
    String getHeaderTitle() {
        return mHeaderTitle;
    }

    /** @return The description text to be shown if it is a section header. */
    @Nullable
    String getHeaderDescription() {
        return mHeaderDescription;
    }
}
