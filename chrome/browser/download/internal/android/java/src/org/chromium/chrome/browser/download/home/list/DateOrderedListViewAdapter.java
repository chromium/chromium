// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.download.home.list.holder.ListItemViewHolder;
import org.chromium.ui.modelutil.RecyclerViewAdapter;

/**
 * A helper {@link RecyclerView.Adapter} implementation meant to glue a {@link ListItemModel}
 * to the right {@link ViewHolder} and {@link ViewBinder}.
 * TODO(bauerb): Remove this class together with reliance on stable IDs
 */
class DateOrderedListViewAdapter extends RecyclerViewAdapter<ListItemViewHolder, Void> {
    private final DecoratedListItemModel mModel;

    /** Creates an instance of a {@link DateOrderedListViewAdapter}. */
    public DateOrderedListViewAdapter(DecoratedListItemModel model,
            Delegate<ListItemViewHolder, Void> delegate,
            ViewHolderFactory<ListItemViewHolder> factory) {
        super(delegate, factory);
        mModel = model;
        setHasStableIds(true);
    }

    @Override
    public long getItemId(int position) {
        if (!hasStableIds()) return RecyclerView.NO_ID;
        return mModel.get(position).stableId;
    }
}
