// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.holder;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.download.R;

/**
 * A {@link RecyclerView.ViewHolder} specifically meant to display an in-progress generic {@code
 * OfflineItem}.
 */
public class InProgressGenericViewHolder extends InProgressViewHolder {
    private final TextView mTitle;

    /**
     * Creates a new {@link InProgressViewHolder} instance.
     */
    public static InProgressGenericViewHolder create(ViewGroup parent) {
        View view = LayoutInflater.from(parent.getContext())
                            .inflate(R.layout.download_manager_in_progress_item, null);
        return new InProgressGenericViewHolder(view);
    }

    /** Constructor. */
    public InProgressGenericViewHolder(View view) {
        super(view, false /* constrainCaption */);
        mTitle = view.findViewById(R.id.title);
    }

    // InProgressViewHolder implementation.
    @Override
    public void bind(PropertyModel properties, ListItem item) {
        super.bind(properties, item);
        mTitle.setText(((ListItem.OfflineItemListItem) item).item.title);
    }
}
