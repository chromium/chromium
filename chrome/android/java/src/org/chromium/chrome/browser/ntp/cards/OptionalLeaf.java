// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import androidx.annotation.CallSuper;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder.PartialBindCallback;

import java.util.Collections;
import java.util.Set;

/**
 * An optional leaf (i.e. single item) in the tree. Depending on its internal state (see
 * {@link #isVisible()}), the item will be present or absent from the tree, by manipulating the
 * values returned from {@link ChildNode} methods. This allows the parent node to not have to add or
 * remove the optional leaf from its children manually.
 *
 * For a non optional leaf, see {@link Leaf}. They have similar interfaces.
 */
public abstract class OptionalLeaf
        extends ChildNode<NewTabPageViewHolder, PartialBindCallback> implements PartiallyBindable {
    private boolean mVisible;

    @Override
    protected int getItemCountForDebugging() {
        return isVisible() ? 1 : 0;
    }

    @Override
    public int getItemViewType(int position) {
        checkIndex(position);
        return getItemViewType();
    }

    @Override
    public void onBindViewHolder(NewTabPageViewHolder holder, int position) {
        checkIndex(position);
        onBindViewHolder(holder);
    }

    @Override
    public Set<Integer> getItemDismissalGroup(int position) {
        checkIndex(position);
        return canBeDismissed() ? Collections.singleton(0) : Collections.<Integer>emptySet();
    }

    @Override
    public void dismissItem(int position, Callback<String> itemRemovedCallback) {
        checkIndex(position);
        dismiss(itemRemovedCallback);
    }

    @Override
    public String describeItemForTesting(int position) {
        checkIndex(position);
        return describeForTesting();
    }

    protected abstract String describeForTesting();

    /** @return Whether the optional item is currently visible. */
    public final boolean isVisible() {
        return mVisible;
    }

    /**
     * Notifies the parents in the tree about whether the visibility of this leaf changed. Call this
     * after a data change that could affect the return value of {@link #isVisible()}. The leaf is
     * initially considered hidden.
     */
    @CallSuper
    protected void setVisibilityInternal(boolean visible) {
        if (mVisible == visible) return;
        mVisible = visible;

        if (visible) {
            notifyItemInserted(0);
        } else {
            notifyItemRemoved(0);
        }
    }

    /**
     * Display the data for this item.
     * @param holder The view holder that should be updated.
     * @see #onBindViewHolder(NewTabPageViewHolder, int)
     * @see android.support.v7.widget.RecyclerView.Adapter#onBindViewHolder
     */
    protected abstract void onBindViewHolder(NewTabPageViewHolder holder);

    /**
     * @return The view type of this item.
     * @see android.support.v7.widget.RecyclerView.Adapter#getItemViewType
     */
    @ItemViewType
    protected abstract int getItemViewType();

    /**
     * @return Whether the item can be dismissed.
     */
    protected boolean canBeDismissed() {
        return false;
    }

    /**
     * Dismiss this item. The default implementation asserts, as by default items can't be
     * dismissed.
     * @param itemRemovedCallback Should be called with the title of the dismissed item, to announce
     * it for accessibility purposes.
     * @see TreeNode#dismissItem
     */
    protected void dismiss(Callback<String> itemRemovedCallback) {
        assert false;
    }
}
