// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.holder;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.UiUtils;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A {@link RecyclerView.ViewHolder} specifically meant to display an image {@code OfflineItem} in
 * full width, mirroring the beautiful material card layout of the video item but without the play
 * button.
 */
@NullMarked
public class ImageFullWidthViewHolder extends OfflineItemViewHolder {
    private final TextView mTitle;
    private final TextView mCaption;

    /** Creates a new {@link ImageFullWidthViewHolder} instance. */
    public static ImageFullWidthViewHolder create(ViewGroup parent) {
        View view =
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.download_manager_image_full_width_item, null);
        return new ImageFullWidthViewHolder(view);
    }

    private ImageFullWidthViewHolder(View view) {
        super(view);
        mTitle = view.findViewById(R.id.title);
        mCaption = view.findViewById(R.id.caption);
    }

    // ListItemViewHolder implementation.
    @Override
    public void bind(PropertyModel properties, ListItem item) {
        super.bind(properties, item);

        OfflineItem offlineItem = ((ListItem.OfflineItemListItem) item).item;

        mTitle.setText(UiUtils.formatGenericItemTitle(offlineItem));
        mCaption.setText(UiUtils.generateGenericCaption(offlineItem));
        mThumbnail.setContentDescription(offlineItem.title);
    }
}
