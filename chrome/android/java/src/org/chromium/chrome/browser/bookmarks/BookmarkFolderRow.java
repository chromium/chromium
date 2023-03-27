// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.widget.ImageView;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.R;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.TintedDrawable;

/**
 * A row view that shows folder info in the bookmarks UI.
 */
public class BookmarkFolderRow extends BookmarkRow {
    /**
     * Factory constructor for building the view programmatically.
     * @param context The calling context, usually the parent view.
     * @param isVisualRefreshEnabled Whether to show the visual or compact bookmark row.
     */
    public static BookmarkFolderRow buildView(Context context, boolean isVisualRefreshEnabled) {
        BookmarkFolderRow row = new BookmarkFolderRow(context, null);
        BookmarkRow.buildView(row, context, isVisualRefreshEnabled);
        return row;
    }

    /** Constructor for inflating from XML. */
    public BookmarkFolderRow(Context context, AttributeSet attrs) {
        super(context, attrs);

        setIconDrawable(BookmarkUtils.getFolderIcon(getContext(), BookmarkType.NORMAL));
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

        setIconDrawable(BookmarkUtils.getFolderIcon(getContext(), item.getId().getType()));
        return item;
    }

    // CheckableSelectableItemView implementation.

    @Override
    protected ColorStateList getDefaultIconTint() {
        @BookmarkType
        int type = (mBookmarkId == null) ? BookmarkType.NORMAL : mBookmarkId.getType();
        return AppCompatResources.getColorStateList(
                getContext(), BookmarkUtils.getFolderIconTint(type));
    }

    /**
     * Sets the icon for the image view: the default icon if unselected, the check mark if selected.
     *
     * @param imageView     The image view in which the icon will be presented.
     * @param defaultIcon   The default icon that will be displayed if not selected.
     * @param isSelected    Whether the item is selected or not.
     */
    public static void applyModernIconStyle(
            ImageView imageView, Drawable defaultIcon, boolean isSelected) {
        imageView.setBackgroundResource(R.drawable.list_item_icon_modern_bg);
        Drawable drawable;
        if (isSelected) {
            drawable = TintedDrawable.constructTintedDrawable(
                    imageView.getContext(), R.drawable.ic_check_googblue_24dp);
            drawable.setTint(SemanticColorUtils.getDefaultIconColorInverse(imageView.getContext()));
        } else {
            drawable = defaultIcon;
        }
        imageView.setImageDrawable(drawable);
        imageView.getBackground().setLevel(isSelected
                        ? imageView.getResources().getInteger(R.integer.list_item_level_selected)
                        : imageView.getResources().getInteger(R.integer.list_item_level_default));
    }
}
