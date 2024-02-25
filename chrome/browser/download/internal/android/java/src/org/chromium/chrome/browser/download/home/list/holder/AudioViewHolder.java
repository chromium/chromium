// Copyright 2019 The Chromium Authors
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
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A {@link RecyclerView.ViewHolder} specifically meant to display a standalone prefetched audio.
 */
public class AudioViewHolder extends OfflineItemViewHolder {
    private final TextView mTitle;
    private final TextView mCaption;
    private final TextView mTimestamp;

    /** Creates a new instance of a {@link AudioViewHolder}. */
    public static AudioViewHolder create(ViewGroup parent) {
        View view =
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.download_manager_audio, null);
        return new AudioViewHolder(view);
    }

    private AudioViewHolder(View view) {
        super(view);
        mTitle = (TextView) itemView.findViewById(R.id.title);
        mCaption = (TextView) itemView.findViewById(R.id.caption);
        mTimestamp = (TextView) itemView.findViewById(R.id.timestamp);
    }

    // ThumbnailAwareViewHolder implementation.
    @Override
    public void bind(PropertyModel properties, ListItem item) {
        super.bind(properties, item);
        ListItem.OfflineItemListItem offlineItem = (ListItem.OfflineItemListItem) item;

        mTitle.setText(UiUtils.formatGenericItemTitle(offlineItem.item));
        mCaption.setText(UiUtils.generatePrefetchCaption(offlineItem.item));
        mTimestamp.setText(UiUtils.generatePrefetchTimestamp(offlineItem.date));
    }
}
