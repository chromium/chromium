// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import androidx.annotation.Nullable;

import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewMcp;

/**
 * This ModelChangeProcessor is a specialization of the {@link SimpleRecyclerViewMcp} allows
 * defining a {@link ViewRecycler} that cleans up ViewHolders which are about to be recycled.
 * @see SimpleRecyclerViewMcp
 * @param <T> The type of items in the list.
 * @param <VH> The view holder type that shows items.
 */
class KeyboardAccessoryRecyclerViewMcp<T, VH> extends SimpleRecyclerViewMcp<T, VH> {
    private ViewRecycler<VH> mViewRecycler;

    /**
     * View recycling interface.
     * @param <VH> The view holder type that shows items.
     */
    public interface ViewRecycler<VH> {
        /**
         * This method is called when the ViewHolder is unbound which provides opportunity to clean.
         * @param holder The ViewHolder that is about to be sent for recycling.
         */
        void onRecycleViewHolder(VH holder);
    }

    @Override
    public void onViewRecycled(VH viewHolder) {
        mViewRecycler.onRecycleViewHolder(viewHolder);
    }

    /**
     * @param model The {@link ListModel} model used to retrieve items to display.
     * @param itemViewTypeCallback The callback to return the view type for an item, or null to use
     *         the default view type.
     * @param viewBinder The {@link ViewBinder} binding this adapter to the view holder.
     */
    public KeyboardAccessoryRecyclerViewMcp(
            ListModel<T> model,
            @Nullable ItemViewTypeCallback<T> itemViewTypeCallback,
            ViewBinder<T, VH> viewBinder,
            ViewRecycler<VH> viewRecycler) {
        super(model, itemViewTypeCallback, viewBinder);
        mViewRecycler = viewRecycler;
    }
}
