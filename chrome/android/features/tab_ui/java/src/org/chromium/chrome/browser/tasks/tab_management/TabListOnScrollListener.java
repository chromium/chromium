// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;

/**
 * An on scroll listener that reports non-zero y offsets to a callback. Updates are only provided if
 * the state changes.
 */
public class TabListOnScrollListener extends RecyclerView.OnScrollListener {
    private ObservableSupplierImpl<Boolean> mYOffsetNonZeroSupplier =
            new ObservableSupplierImpl<>();

    public ObservableSupplier<Boolean> getYOffsetNonZeroSupplier() {
        return mYOffsetNonZeroSupplier;
    }

    /** Post an update out-of-band. */
    void postUpdate(RecyclerView recyclerView) {
        recyclerView.post(
                () -> {
                    int yOffset = recyclerView.computeVerticalScrollOffset();
                    // Just early out if settling there should be another event soon.
                    if (recyclerView.getScrollState() == RecyclerView.SCROLL_STATE_SETTLING) return;
                    mYOffsetNonZeroSupplier.set(yOffset > 0);
                });
    }

    @Override
    public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
        final int yOffset = recyclerView.computeVerticalScrollOffset();
        if (yOffset == 0) {
            mYOffsetNonZeroSupplier.set(false);
            return;
        }

        if (dy == 0 || recyclerView.getScrollState() == RecyclerView.SCROLL_STATE_SETTLING) {
            return;
        }

        mYOffsetNonZeroSupplier.set(yOffset > 0);
    }
}
