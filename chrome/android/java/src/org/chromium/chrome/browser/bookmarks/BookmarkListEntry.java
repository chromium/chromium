// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import androidx.annotation.DimenRes;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

import javax.annotation.Nonnull;

/** Represents different type of views in the bookmark UI. */
public final class BookmarkListEntry {
    /** Specifies the view types that the bookmark delegate screen can contain. */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        ViewType.INVALID,
        ViewType.PERSONALIZED_SIGNIN_PROMO,
        ViewType.PERSONALIZED_SYNC_PROMO,
        ViewType.SYNC_PROMO,
        ViewType.DIVIDER,
        ViewType.SECTION_HEADER,
        ViewType.IMPROVED_BOOKMARK_VISUAL,
        ViewType.IMPROVED_BOOKMARK_COMPACT,
        ViewType.SEARCH_BOX,
        ViewType.EMPTY_STATE
    })
    public @interface ViewType {
        int INVALID = -1;
        int PERSONALIZED_SIGNIN_PROMO = 0;
        int PERSONALIZED_SYNC_PROMO = 1;
        int SYNC_PROMO = 2;
        int DIVIDER = 3;
        int SECTION_HEADER = 4;
        int IMPROVED_BOOKMARK_VISUAL = 5;
        int IMPROVED_BOOKMARK_COMPACT = 6;
        int SEARCH_BOX = 7;
        int EMPTY_STATE = 8;
    }

    /** Contains data used by section header in bookmark UI. */
    static final class SectionHeaderData {
        public final @StringRes int titleRes;
        public final @DimenRes int topPaddingRes;

        SectionHeaderData(@StringRes int titleRes, @DimenRes int topPaddingRes) {
            this.titleRes = titleRes;
            this.topPaddingRes = topPaddingRes;
        }
    }

    private final @ViewType int mViewType;
    @Nullable private final BookmarkItem mBookmarkItem;
    @Nullable private final SectionHeaderData mSectionHeaderData;
    @Nullable private final PowerBookmarkMeta mPowerBookmarkMeta;

    private BookmarkListEntry(
            int viewType,
            @Nullable BookmarkItem bookmarkItem,
            @Nullable SectionHeaderData sectionHeaderData,
            @Nullable PowerBookmarkMeta meta) {
        mViewType = viewType;
        mBookmarkItem = bookmarkItem;
        mSectionHeaderData = sectionHeaderData;
        mPowerBookmarkMeta = meta;
    }

    /**
     * Create an entry presenting a bookmark folder or bookmark.
     *
     * @param bookmarkItem The data object created from the bookmark backend.
     * @param meta The PowerBookmarkMeta for the bookmark.
     * @param displayPref The display pref for the bookmark.
     */
    static BookmarkListEntry createBookmarkEntry(
            @Nonnull BookmarkItem bookmarkItem,
            @Nullable PowerBookmarkMeta meta,
            @BookmarkRowDisplayPref int displayPref) {
        @ViewType
        int viewType =
                displayPref == BookmarkRowDisplayPref.VISUAL
                        ? ViewType.IMPROVED_BOOKMARK_VISUAL
                        : ViewType.IMPROVED_BOOKMARK_COMPACT;

        return new BookmarkListEntry(viewType, bookmarkItem, /* sectionHeaderData= */ null, meta);
    }

    /**
     * Create an entry presenting a sync promo header.
     *
     * @param viewType The view type of the sync promo header.
     */
    static BookmarkListEntry createSyncPromoHeader(@ViewType int viewType) {
        assert viewType == ViewType.PERSONALIZED_SIGNIN_PROMO
                || viewType == ViewType.PERSONALIZED_SYNC_PROMO
                || viewType == ViewType.SYNC_PROMO;
        return new BookmarkListEntry(
                viewType,
                /* bookmarkItem= */ null,
                /* sectionHeaderData= */ null,
                /* meta= */ null);
    }

    /** Creates a divider to separate sections in the bookmark list. */
    static BookmarkListEntry createDivider() {
        return new BookmarkListEntry(
                ViewType.DIVIDER,
                /* bookmarkItem= */ null,
                /* sectionHeaderData= */ null,
                /* meta= */ null);
    }

    /**
     * Create an entry representing the reading list read/unread section header.
     *
     * @param titleRes The resource id for the title of the section header.
     * @param topPaddingRes The resource for the top padding of the section header. Ignored if 0.
     */
    static BookmarkListEntry createSectionHeader(
            @StringRes int titleRes, @DimenRes int topPaddingRes) {
        SectionHeaderData sectionHeaderData = new SectionHeaderData(titleRes, topPaddingRes);
        return new BookmarkListEntry(
                ViewType.SECTION_HEADER, null, sectionHeaderData, /* meta= */ null);
    }

    /** Returns the view type used in the bookmark list UI. */
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

    /**
     * @return The {@link SectionHeaderData}. Could be null if this entry is not a section header.
     */
    @Nullable
    SectionHeaderData getSectionHeaderData() {
        return mSectionHeaderData;
    }

    /** Returns the PowerBookmarkMeta for this list entry. */
    @Nullable
    PowerBookmarkMeta getPowerBookmarkMeta() {
        return mPowerBookmarkMeta;
    }
}
