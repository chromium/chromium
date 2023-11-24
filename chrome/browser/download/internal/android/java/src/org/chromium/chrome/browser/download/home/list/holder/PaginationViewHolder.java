// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.holder;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.ListProperties;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.ui.modelutil.PropertyModel;

/** A {@link RecyclerView.ViewHolder} specifically meant to display a pagination header. */
public class PaginationViewHolder extends ListItemViewHolder {
    /** Creates a new {@link PaginationViewHolder} instance. */
    public static PaginationViewHolder create(ViewGroup parent) {
        View view =
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.download_manager_pagination_header, null);
        return new PaginationViewHolder(view);
    }

    public PaginationViewHolder(View view) {
        super(view);
    }

    @Override
    public void bind(PropertyModel properties, ListItem item) {
        itemView.setOnClickListener(
                v -> {
                    properties.get(ListProperties.CALLBACK_PAGINATION_CLICK).run();
                });
    }
}
