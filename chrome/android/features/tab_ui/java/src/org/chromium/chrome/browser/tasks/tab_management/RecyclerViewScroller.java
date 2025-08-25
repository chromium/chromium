// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.graphics.Rect;
import android.view.View;

import androidx.recyclerview.widget.LinearSmoothScroller;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.LayoutManager;

import org.chromium.build.annotations.NullMarked;

/** Scrolls a {@link RecyclerView} to a given index. */
@NullMarked
public class RecyclerViewScroller {
    /**
     * Smooth scrolls the recycler view to a given index. Will not scroll if the target view at the
     * given index is fully visible.
     *
     * @param recyclerView The {@link RecyclerView} to scroll.
     * @param targetIndex The index to scroll to.
     * @param onScrollFinished A {@link Runnable} to execute when the scroll is complete. If the
     *     smooth scrolling is interrupted, this will be called immediately.
     */
    public static void smoothScrollToPosition(
            RecyclerView recyclerView, int targetIndex, Runnable onScrollFinished) {
        if (isTargetFullyVisible(recyclerView, targetIndex)) {
            onScrollFinished.run();
            return;
        }

        LinearSmoothScroller smoothScroller =
                new LinearSmoothScroller(recyclerView.getContext()) {
                    @Override
                    protected int getVerticalSnapPreference() {
                        return LinearSmoothScroller.SNAP_TO_START;
                    }

                    @Override
                    protected void onStop() {
                        super.onStop();
                        onScrollFinished.run();
                    }
                };
        smoothScroller.setTargetPosition(targetIndex);
        LayoutManager layoutManager = recyclerView.getLayoutManager();
        if (layoutManager == null) return;

        layoutManager.startSmoothScroll(smoothScroller);
    }

    private static boolean isTargetFullyVisible(RecyclerView recyclerView, int targetIndex) {
        RecyclerView.ViewHolder viewHolder =
                recyclerView.findViewHolderForAdapterPosition(targetIndex);
        if (viewHolder == null) return false;

        View view = viewHolder.itemView;
        if (!view.isShown()) return false;

        Rect actualPosition = new Rect();
        view.getGlobalVisibleRect(actualPosition);

        return view.getMeasuredHeight() == actualPosition.height();
    }
}
