// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.holder;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.UiUtils;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A {@link RecyclerView.ViewHolder} specifically meant to display a prefetch item that is part of a
 * group card.
 */
public class PrefetchGroupedItemViewHolder extends OfflineItemViewHolder {
    private final TextView mTitle;
    private final TextView mTimestamp;

    /** Creates a new instance of a {@link PrefetchGroupedItemViewHolder}. */
    public static PrefetchGroupedItemViewHolder create(ViewGroup parent) {
        View view =
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.download_manager_prefetch_grouped_item, null);
        return new PrefetchGroupedItemViewHolder(view);
    }

    private PrefetchGroupedItemViewHolder(View view) {
        super(view);
        mTitle = itemView.findViewById(R.id.title);
        mTimestamp = (TextView) itemView.findViewById(R.id.timestamp);
    }

    // ThumbnailAwareViewHolder implementation.
    @Override
    public void bind(PropertyModel properties, ListItem item) {
        super.bind(properties, item);
        ListItem.OfflineItemListItem listItem = (ListItem.OfflineItemListItem) item;

        mTitle.setText(UiUtils.formatGenericItemTitle(listItem.item));
        mTimestamp.setText(UiUtils.generatePrefetchTimestamp(listItem.date));

        OfflineItem offlineItem = ((ListItem.OfflineItemListItem) item).item;
        ImageView mediaButton = itemView.findViewById(R.id.media_button);
        mediaButton.setImageResource(UiUtils.getMediaPlayIconForPrefetchCards(offlineItem));
    }
}
