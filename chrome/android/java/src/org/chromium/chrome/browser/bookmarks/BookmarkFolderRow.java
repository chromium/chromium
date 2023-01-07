// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.R;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;

/**
 * A row view that shows folder info in the bookmarks UI.
 */
public class BookmarkFolderRow extends BookmarkRow {

    /**
     * Constructor for inflating from XML.
     */
    public BookmarkFolderRow(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        setStartIconDrawable(BookmarkUtils.getFolderIcon(getContext(), BookmarkType.NORMAL));
    }

    // BookmarkRow implementation.

    @Override
    public void onClick() {
        mDelegate.openFolder(mBookmarkId);
    }

    @Override
    BookmarkItem setBookmarkId(
            BookmarkId bookmarkId, @Location int location, boolean fromFilterView) {
        BookmarkItem item = super.setBookmarkId(bookmarkId, location, fromFilterView);
        mTitleView.setText(item.getTitle());

        // Set description and icon.
        if (item.getId().getType() == BookmarkType.READING_LIST) {
            int unreadCount = mDelegate.getModel().getUnreadCount(bookmarkId);
            mDescriptionView.setText(unreadCount > 0
                            ? getResources().getQuantityString(
                                    R.plurals.reading_list_unread_page_count, unreadCount,
                                    unreadCount)
                            : getResources().getString(R.string.reading_list_no_unread_pages));
        } else {
            int childCount = mDelegate.getModel().getTotalBookmarkCount(bookmarkId);
            mDescriptionView.setText((childCount > 0)
                            ? getResources().getQuantityString(
                                    R.plurals.bookmarks_count, childCount, childCount)
                            : getResources().getString(R.string.no_bookmarks));
        }

        setStartIconDrawable(BookmarkUtils.getFolderIcon(getContext(), item.getId().getType()));
        return item;
    }

    @Override
    protected ColorStateList getDefaultStartIconTint() {
        @BookmarkType
        int type = (mBookmarkId == null) ? BookmarkType.NORMAL : mBookmarkId.getType();
        return AppCompatResources.getColorStateList(
                getContext(), BookmarkUtils.getFolderIconTint(type));
    }
}
