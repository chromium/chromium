// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import androidx.annotation.Nullable;

import org.chromium.ui.modelutil.ListObservableImpl;
import org.chromium.ui.modelutil.RecyclerViewAdapter;

/**
 * A node in the tree that has a parent and can notify it about changes.
 *
 * This class mostly serves as a convenience base class for implementations of {@link
 * RecyclerViewAdapter.Delegate}.
 *
 * @param <VH> The view holder type.
 * @param <P> The payload type for partial updates, or Void if the node doesn't support partial
 *         updates.
 */
public abstract class ChildNode<VH, P>
        extends ListObservableImpl<P> implements RecyclerViewAdapter.Delegate<VH, P> {
    private int mNumItems;

    @Override
    public int getItemCount() {
        assert mNumItems
                == getItemCountForDebugging()
            : "cached number of items: " + mNumItems + "; actual number of items: "
              + getItemCountForDebugging();
        return mNumItems;
    }

    @Override
    protected void notifyItemRangeChanged(int index, int count, @Nullable P callback) {
        assert isRangeValid(index, count);
        super.notifyItemRangeChanged(index, count, callback);
    }

    @Override
    protected void notifyItemRangeInserted(int index, int count) {
        mNumItems += count;
        assert mNumItems == getItemCountForDebugging();
        assert isRangeValid(index, count);
        super.notifyItemRangeInserted(index, count);
    }

    @Override
    protected void notifyItemRangeRemoved(int index, int count) {
        assert isRangeValid(index, count);
        mNumItems -= count;
        assert mNumItems == getItemCountForDebugging();
        super.notifyItemRangeRemoved(index, count);
    }

    protected void checkIndex(int position) {
        if (!isRangeValid(position, 1)) {
            throw new IndexOutOfBoundsException(position + "/" + getItemCount());
        }
    }

    private boolean isRangeValid(int index, int count) {
        // Uses |mNumItems| to be able to call the method when checking deletion range, as we still
        // have the previous number of items.
        return index >= 0 && index + count <= mNumItems;
    }

    /**
     * @return The actual (non-cached) number of items under this node. The implementation of this
     * method should not rely on {@link #getItemCount}, but instead derive the number of items
     * directly from the underlying data model. Any time this value changes, an appropriate
     * notification should be sent.
     */
    protected abstract int getItemCountForDebugging();
}
