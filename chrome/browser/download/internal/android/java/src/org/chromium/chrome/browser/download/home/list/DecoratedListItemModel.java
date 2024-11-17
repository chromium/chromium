// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.download.home.list.ListItem.ViewListItem;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.ListObservable.ListObserver;
import org.chromium.ui.modelutil.ListObservableImpl;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleList;

import java.util.ArrayList;
import java.util.List;

/**
 * A wrapper class that adds decoration {@link ListItem}s to a {@link ListItemModel}.
 * TODO(bauerb): Replace this with InnerNode (once it has been migrated to the UI architecture)
 */
class DecoratedListItemModel extends ListObservableImpl<Void>
        implements ListObserver<Void>, SimpleList<ListItem> {
    private final ListItemModel mModel;

    private final List<ViewListItem> mHeaderItems = new ArrayList<>();

    /** Creates a {@link DecoratedListItemModel} instance that wraos {@code model}. */
    public DecoratedListItemModel(ListItemModel model) {
        mModel = model;
        mModel.addObserver(this);
    }

    /** @see ListItemModel#getProperties() */
    public PropertyModel getProperties() {
        return mModel.getProperties();
    }

    /** Adds {@code item} as a header for the list. */
    public void addHeader(ViewListItem item) {
        int index = mHeaderItems.size();
        mHeaderItems.add(item);
        notifyItemInserted(index);
    }

    // SimpleList implementation.
    @Override
    public int size() {
        return mModel.size() + mHeaderItems.size();
    }

    @Override
    public ListItem get(int index) {
        if (index < mHeaderItems.size()) return mHeaderItems.get(index);
        return mModel.get(convertIndexForSource(index));
    }

    // ListObserver implementation.
    @Override
    public void onItemRangeInserted(ListObservable source, int index, int count) {
        notifyItemRangeInserted(convertIndexFromSource(index), count);
    }

    @Override
    public void onItemRangeRemoved(ListObservable source, int index, int count) {
        notifyItemRangeRemoved(convertIndexFromSource(index), count);
    }

    @Override
    public void onItemRangeChanged(
            ListObservable source, int index, int count, @Nullable Void payload) {
        notifyItemRangeChanged(convertIndexFromSource(index), count, null);
    }

    private int convertIndexForSource(int index) {
        return index - mHeaderItems.size();
    }

    private int convertIndexFromSource(int index) {
        return index + mHeaderItems.size();
    }
}
