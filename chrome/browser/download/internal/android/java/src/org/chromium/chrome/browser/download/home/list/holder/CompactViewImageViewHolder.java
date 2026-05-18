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
 * A {@link RecyclerView.ViewHolder} for image {@code OfflineItem}s rendered in the compact list
 * view variant of the Downloads page. Uses a layout similar to {@link GenericViewHolder} (small,
 * row-style item with title and caption) but keeps the image thumbnail square — no rounded
 * clipping, no grey background — with the preview centered inside the thumbnail bounds. Title and
 * caption use the same formatting as the generic item ("size · domain" caption).
 */
@NullMarked
public class CompactViewImageViewHolder extends ImageViewHolder {
    private final TextView mTitle;
    private final TextView mCaption;

    /** Creates a new {@link CompactViewImageViewHolder} instance. */
    public static CompactViewImageViewHolder create(ViewGroup parent) {
        View view =
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.download_manager_compact_view_image_item, null);
        return new CompactViewImageViewHolder(view);
    }

    private CompactViewImageViewHolder(View view) {
        super(view);
        mTitle = view.findViewById(R.id.title);
        mCaption = view.findViewById(R.id.caption);
    }

    @Override
    public void bind(PropertyModel properties, ListItem item) {
        super.bind(properties, item);
        OfflineItem offlineItem = ((ListItem.OfflineItemListItem) item).item;
        mTitle.setText(UiUtils.formatGenericItemTitle(offlineItem));
        mCaption.setText(UiUtils.generateGenericCaption(offlineItem));
    }
}
