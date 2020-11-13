// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.holder;

import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.ListUtils;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A {@link ViewHolder} responsible for building and setting properties on the underlying Android
 * {@link View}s for the Download Manager list.
 */
public abstract class ListItemViewHolder extends ViewHolder {
    /** Creates an instance of a {@link ListItemViewHolder}. */
    protected ListItemViewHolder(View itemView) {
        super(itemView);
    }

    /**
     * Used as a method reference for ViewHolderFactory.
     * @see
     * RecyclerViewAdapter.ViewHolderFactory#createViewHolder
     */
    public static ListItemViewHolder create(ViewGroup parent, @ListUtils.ViewType int viewType) {
        switch (viewType) {
            case ListUtils.ViewType.IN_PROGRESS:
                return InProgressGenericViewHolder.create(parent);
            case ListUtils.ViewType.GENERIC:
                return GenericViewHolder.create(parent);
            case ListUtils.ViewType.VIDEO:
                return VideoViewHolder.create(parent);
            case ListUtils.ViewType.IMAGE: // intentional fall-through
            case ListUtils.ViewType.IMAGE_FULL_WIDTH:
                return ImageViewHolder.create(parent);
            case ListUtils.ViewType.CUSTOM_VIEW:
                return new CustomViewHolder(parent);
            case ListUtils.ViewType.PREFETCH_ARTICLE:
                return PrefetchArticleViewHolder.create(parent);
            case ListUtils.ViewType.AUDIO:
                return AudioViewHolder.create(parent);
            case ListUtils.ViewType.SECTION_HEADER:
                return SectionTitleViewHolder.create(parent);
            case ListUtils.ViewType.IN_PROGRESS_VIDEO:
                return InProgressVideoViewHolder.create(parent);
            case ListUtils.ViewType.IN_PROGRESS_IMAGE:
                return InProgressImageViewHolder.create(parent);
            case ListUtils.ViewType.PAGINATION_HEADER:
                return PaginationViewHolder.create(parent);
            case ListUtils.ViewType.GROUP_CARD_ITEM:
                return PrefetchGroupedItemViewHolder.create(parent);
            case ListUtils.ViewType.GROUP_CARD_HEADER:
                return CardHeaderViewHolder.create(parent);
            case ListUtils.ViewType.GROUP_CARD_FOOTER:
                return CardFooterViewHolder.create(parent);
            case ListUtils.ViewType.GROUP_CARD_DIVIDER_TOP:
                return CardDividerTopViewHolder.create(parent);
            case ListUtils.ViewType.GROUP_CARD_DIVIDER_MIDDLE:
                return CardDividerMiddleViewHolder.create(parent);
            case ListUtils.ViewType.GROUP_CARD_DIVIDER_BOTTOM:
                return CardDividerBottomViewHolder.create(parent);
        }

        assert false;
        return null;
    }

    /**
     * Binds the currently held {@link View} to {@code item}.
     * @param properties The shared {@link PropertyModel} all items can access.
     * @param item       The {@link ListItem} to visually represent in this {@link ViewHolder}.
     */
    public abstract void bind(PropertyModel properties, ListItem item);

    /**
     * Gives subclasses a chance to free up expensive resources when this {@link ViewHolder} is no
     * longer attached to the parent {@link RecyclerView}.
     */
    public void recycle() {}
}
