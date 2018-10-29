// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.holder;

import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.ListItem.SectionHeaderListItem;
import org.chromium.chrome.browser.download.home.list.ListProperties;
import org.chromium.chrome.browser.download.home.list.ListUtils;
import org.chromium.chrome.browser.download.home.list.UiUtils;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.widget.ListMenuButton;
import org.chromium.chrome.download.R;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

/**
 * A {@link ViewHolder} specifically meant to display a section header.
 */
public class SectionTitleViewHolder extends ListItemViewHolder implements ListMenuButton.Delegate {
    private final View mDivider;
    private final TextView mDate;
    private final TextView mTitle;
    private final ListMenuButton mMore;
    private final View mTopSpace;
    private final View mBottomSpace;

    private Runnable mShareCallback;
    private Runnable mDeleteCallback;
    private Runnable mShareAllCallback;
    private Runnable mDeleteAllCallback;
    private Runnable mSelectCallback;

    private boolean mHasMultipleItems;
    private boolean mCanSelectItems;

    /** Create a new {@link SectionTitleViewHolder} instance. */
    public static SectionTitleViewHolder create(ViewGroup parent) {
        View view = LayoutInflater.from(parent.getContext())
                            .inflate(R.layout.download_manager_section_header, null);
        return new SectionTitleViewHolder(view);
    }

    private SectionTitleViewHolder(View view) {
        super(view);
        mDivider = view.findViewById(R.id.divider);
        mDate = (TextView) view.findViewById(R.id.date);
        mTitle = (TextView) view.findViewById(R.id.title);
        mMore = (ListMenuButton) view.findViewById(R.id.more);
        mTopSpace = view.findViewById(R.id.top_space);
        mBottomSpace = view.findViewById(R.id.bottom_space);
        if (mMore != null) mMore.setDelegate(this);
    }

    // ListItemViewHolder implementation.
    @Override
    public void bind(PropertyModel properties, ListItem item) {
        SectionHeaderListItem sectionItem = (SectionHeaderListItem) item;
        mTitle.setText(ListUtils.getTextForSection(sectionItem.filter));

        if (sectionItem.showDate) {
            mDate.setText(sectionItem.isJustNow ? itemView.getContext().getResources().getString(
                                                          R.string.download_manager_just_now)
                                                : UiUtils.dateToHeaderString(sectionItem.date));
        }

        updateTopBottomSpacing(sectionItem.showMenu);
        mDivider.setVisibility(sectionItem.showDivider ? ViewGroup.VISIBLE : ViewGroup.GONE);
        mDate.setVisibility(sectionItem.showDate ? View.VISIBLE : View.GONE);
        mTitle.setVisibility((sectionItem.showTitle ? View.VISIBLE : View.GONE));
        if (mMore != null) mMore.setVisibility(sectionItem.showMenu ? View.VISIBLE : View.GONE);

        mHasMultipleItems = sectionItem.items.size() > 1;
        mCanSelectItems = !getCompletedItems(sectionItem.items).isEmpty();

        if (sectionItem.showMenu && mMore != null) {
            assert sectionItem.items.size() > 0;
            mShareCallback = ()
                    -> properties.get(ListProperties.CALLBACK_SHARE)
                               .onResult(sectionItem.items.get(0));
            mDeleteCallback = ()
                    -> properties.get(ListProperties.CALLBACK_REMOVE)
                               .onResult(sectionItem.items.get(0));

            mShareAllCallback = ()
                    -> properties.get(ListProperties.CALLBACK_SHARE_ALL)
                               .onResult(getCompletedItems(sectionItem.items));
            mDeleteAllCallback = ()
                    -> properties.get(ListProperties.CALLBACK_REMOVE_ALL)
                               .onResult(sectionItem.items);
            mSelectCallback = properties.get(ListProperties.CALLBACK_START_SELECTION);

            mMore.setClickable(!properties.get(ListProperties.SELECTION_MODE_ACTIVE));
        }
    }

    @Override
    public ListMenuButton.Item[] getItems() {
        Context context = itemView.getContext();
        if (mHasMultipleItems) {
            return new ListMenuButton.Item[] {
                    new ListMenuButton.Item(context, R.string.select, mCanSelectItems),
                    new ListMenuButton.Item(context, R.string.share_group, mCanSelectItems),
                    new ListMenuButton.Item(context, R.string.delete_group, true)};
        } else {
            return new ListMenuButton.Item[] {
                    new ListMenuButton.Item(context, R.string.share, mCanSelectItems),
                    new ListMenuButton.Item(context, R.string.delete, true)};
        }
    }

    @Override
    public void onItemSelected(ListMenuButton.Item item) {
        if (item.getTextId() == R.string.select) {
            mSelectCallback.run();
        } else if (item.getTextId() == R.string.share) {
            mShareCallback.run();
        } else if (item.getTextId() == R.string.delete) {
            mDeleteCallback.run();
        } else if (item.getTextId() == R.string.share_group) {
            mShareAllCallback.run();
        } else if (item.getTextId() == R.string.delete_group) {
            mDeleteAllCallback.run();
        }
    }

    private void updateTopBottomSpacing(boolean showMenu) {
        Resources resources = itemView.getContext().getResources();
        ViewGroup.LayoutParams topSpaceParams = mTopSpace.getLayoutParams();
        ViewGroup.LayoutParams bottomSpaceParams = mBottomSpace.getLayoutParams();

        topSpaceParams.height = resources.getDimensionPixelSize(showMenu
                        ? R.dimen.download_manager_section_title_padding_image
                        : R.dimen.download_manager_section_title_padding_top);
        bottomSpaceParams.height = resources.getDimensionPixelSize(showMenu
                        ? R.dimen.download_manager_section_title_padding_image
                        : R.dimen.download_manager_section_title_padding_bottom);

        mTopSpace.setLayoutParams(topSpaceParams);
        mBottomSpace.setLayoutParams(bottomSpaceParams);
    }

    private static List<OfflineItem> getCompletedItems(Collection<OfflineItem> items) {
        List<OfflineItem> completedItems = new ArrayList<>();
        for (OfflineItem item : items) {
            if (item.state != OfflineItemState.COMPLETE) continue;
            completedItems.add(item);
        }

        return completedItems;
    }
}
