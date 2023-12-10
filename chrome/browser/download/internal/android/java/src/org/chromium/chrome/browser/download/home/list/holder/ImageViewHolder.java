// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.holder;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.ui.modelutil.PropertyModel;

/** A {@link RecyclerView.ViewHolder} specifically meant to display an image {@code OfflineItem}. */
public class ImageViewHolder extends OfflineItemViewHolder {
    public static ImageViewHolder create(ViewGroup parent) {
        View view =
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.download_manager_image_item, null);
        return new ImageViewHolder(view);
    }

    public ImageViewHolder(View view) {
        super(view);
    }

    // ListItemViewHolder implementation.
    @Override
    public void bind(PropertyModel properties, ListItem item) {
        super.bind(properties, item);
        mThumbnail.setContentDescription(((ListItem.OfflineItemListItem) item).item.title);
    }
}
