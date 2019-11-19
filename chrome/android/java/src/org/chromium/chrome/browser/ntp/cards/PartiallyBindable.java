// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder.PartialBindCallback;
import org.chromium.ui.modelutil.RecyclerViewAdapter;

/**
 * Helper class for implementations of {@link RecyclerViewAdapter.Delegate} using partial
 * binding callbacks as the payload. Implement this interface instead of
 * {@link RecyclerViewAdapter.Delegate} to inherit the default implementation of
 * {@link #onBindViewHolder(NewTabPageViewHolder,int)} that runs the partial bind callback.
 */
public interface PartiallyBindable
        extends RecyclerViewAdapter.Delegate<NewTabPageViewHolder, PartialBindCallback> {
    /**
     * Display the data at {@code position} under this subtree.
     * @param holder The view holder that should be updated.
     * @param position The position of the item under this subtree.
     * @see android.support.v7.widget.RecyclerView.Adapter#onBindViewHolder
     */
    void onBindViewHolder(NewTabPageViewHolder holder, int position);

    @Override
    default void onBindViewHolder(
            NewTabPageViewHolder viewHolder, int position, @Nullable PartialBindCallback payload) {
        if (payload == null) {
            onBindViewHolder(viewHolder, position);
            return;
        }

        payload.onResult(viewHolder);
    }
}
