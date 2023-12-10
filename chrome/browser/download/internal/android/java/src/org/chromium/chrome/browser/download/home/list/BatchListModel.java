// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.BatchingListUpdateCallback;
import androidx.recyclerview.widget.ListUpdateCallback;

import org.chromium.ui.modelutil.ListModel;

/**
 * Helper class to batch updates to ListModel before notifying observers.
 * @see BatchingListUpdateCallback
 * @param <T> The object type that this class manages in a list.
 */
public abstract class BatchListModel<T> extends ListModel<T> {
    final BatchingListUpdateCallback mBatchingCallback;

    /** Creates a new BatchListModel instance. */
    public BatchListModel() {
        mBatchingCallback =
                new BatchingListUpdateCallback(
                        new ListUpdateCallback() {
                            @Override
                            public void onInserted(int position, int count) {
                                super_notifyItemRangeInserted(position, count);
                            }

                            @Override
                            public void onRemoved(int position, int count) {
                                super_notifyItemRangeRemoved(position, count);
                            }

                            @Override
                            public void onMoved(int fromPosition, int toPosition) {
                                assert false : "ListUpdateCallback#onMoved() not supported.";
                            }

                            @Override
                            public void onChanged(
                                    int position, int count, @Nullable Object payload) {
                                assert payload == null;
                                super_notifyItemRangeChanged(position, count);
                            }
                        });
    }

    /**
     * Dispatches any outstanding batched updates to observers.
     * @see BatchingListUpdateCallback#dispatchLastEvent()
     */
    public void dispatchLastEvent() {
        mBatchingCallback.dispatchLastEvent();
    }

    @Override
    protected void notifyItemRangeInserted(int index, int count) {
        mBatchingCallback.onInserted(index, count);
    }

    @Override
    protected void notifyItemRangeRemoved(int index, int count) {
        mBatchingCallback.onRemoved(index, count);
    }

    @Override
    protected void notifyItemRangeChanged(int index, int count, @Nullable Void payload) {
        mBatchingCallback.onChanged(index, count, null);
    }

    private void super_notifyItemRangeInserted(int index, int count) {
        super.notifyItemRangeInserted(index, count);
    }

    private void super_notifyItemRangeRemoved(int index, int count) {
        super.notifyItemRangeRemoved(index, count);
    }

    private void super_notifyItemRangeChanged(int index, int count) {
        super.notifyItemRangeChanged(index, count, null);
    }
}
