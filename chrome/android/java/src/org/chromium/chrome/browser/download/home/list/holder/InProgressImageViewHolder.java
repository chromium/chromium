// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.holder;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.view.AutoAnimatorDrawable;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.download.R;

/**
 * A {@link RecyclerView.ViewHolder} specifically meant to display an in-progress image {@code
 * OfflineItem}.
 */
public class InProgressImageViewHolder extends InProgressViewHolder {
    private final ImageView mPlaceholder;

    /**
     * Creates a new {@link InProgressViewHolder} instance.
     */
    public static InProgressImageViewHolder create(ViewGroup parent) {
        View view = LayoutInflater.from(parent.getContext())
                            .inflate(R.layout.download_manager_in_progress_image_item, null);
        return new InProgressImageViewHolder(view);
    }

    /** Constructor. */
    public InProgressImageViewHolder(View view) {
        super(view, true /* constrainCaption */);

        mPlaceholder = view.findViewById(R.id.placeholder);
        mPlaceholder.setImageDrawable(AutoAnimatorDrawable.wrap(
                org.chromium.chrome.browser.download.home.list.view.UiUtils.getDrawable(
                        view.getContext(), R.drawable.async_image_view_waiting)));
    }

    // InProgressViewHolder implementation.
    @Override
    public void bind(PropertyModel properties, ListItem item) {
        super.bind(properties, item);
        mPlaceholder.setContentDescription(((ListItem.OfflineItemListItem) item).item.title);
    }
}
