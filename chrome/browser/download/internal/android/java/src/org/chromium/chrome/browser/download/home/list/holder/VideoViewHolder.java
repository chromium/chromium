// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.holder;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.UiUtils;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.ui.modelutil.PropertyModel;

/** A {@link RecyclerView.ViewHolder} specifically meant to display a video {@code OfflineItem}. */
public class VideoViewHolder extends OfflineItemViewHolder {
    private final TextView mTitle;
    private final TextView mCaption;

    /** Creates a new {@link VideoViewHolder} instance. */
    public static VideoViewHolder create(ViewGroup parent) {
        View view =
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.download_manager_video_item, null);

        return new VideoViewHolder(view);
    }

    public VideoViewHolder(View view) {
        super(view);

        mTitle = itemView.findViewById(R.id.title);
        mCaption = itemView.findViewById(R.id.caption);
    }

    // MoreButtonViewHolder implementation.
    @Override
    public void bind(PropertyModel properties, ListItem item) {
        super.bind(properties, item);
        OfflineItem offlineItem = ((ListItem.OfflineItemListItem) item).item;

        mTitle.setText(UiUtils.formatGenericItemTitle(offlineItem));
        mCaption.setText(UiUtils.generateGenericCaption(offlineItem));
        mThumbnail.setContentDescription(offlineItem.title);
    }
}
