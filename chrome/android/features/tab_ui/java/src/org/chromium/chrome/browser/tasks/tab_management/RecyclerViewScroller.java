// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.graphics.Rect;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearSmoothScroller;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.LayoutManager;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;

import org.chromium.build.annotations.NullMarked;

/** Scrolls a {@link RecyclerView} to a given index. */
@NullMarked
public class RecyclerViewScroller {
    /**
     * Smooth scrolls the recycler view to a given index.
     *
     * <p>This method will not scroll and will immediately invoke `onScrollFinished` if the target
     * view at the given index is already fully visible.
     *
     * @param recyclerView The {@link RecyclerView} to scroll.
     * @param targetIndex The index to scroll to.
     * @param onScrollFinished A {@link Runnable} to execute when the scroll is complete. If the
     *     smooth scrolling is interrupted by the user, this be called immediately.
     */
    public static void smoothScrollToPosition(
            RecyclerView recyclerView, int targetIndex, Runnable onScrollFinished) {
        LayoutManager layoutManager = recyclerView.getLayoutManager();
        if (isTargetFullyVisible(recyclerView, targetIndex) || layoutManager == null) {
            onScrollFinished.run();
            return;
        }

        boolean scrollingUp = isScrollingUp(targetIndex, layoutManager);
        OnScrollListener scrollListener = new InterruptionDetectingScrollListener(onScrollFinished);

        LinearSmoothScroller smoothScroller =
                new LinearSmoothScroller(recyclerView.getContext()) {
                    @Override
                    protected int getVerticalSnapPreference() {
                        // Ensures full visibility of target view.
                        return scrollingUp ? SNAP_TO_START : SNAP_TO_END;
                    }
                };

        recyclerView.addOnScrollListener(scrollListener);
        smoothScroller.setTargetPosition(targetIndex);
        layoutManager.startSmoothScroll(smoothScroller);
    }

    @VisibleForTesting
    static boolean isScrollingUp(int targetIndex, LayoutManager layoutManager) {
        if (layoutManager.getChildCount() == 0) {
            // Cannot determine direction if no views are laid out. Default to scrolling "down".
            return false;
        } else {
            int minVisiblePosition = Integer.MAX_VALUE;
            for (int i = 0; i < layoutManager.getChildCount(); i++) {
                View child = layoutManager.getChildAt(i);
                if (child != null) {
                    minVisiblePosition =
                            Math.min(minVisiblePosition, layoutManager.getPosition(child));
                }
            }
            return targetIndex < minVisiblePosition;
        }
    }

    @VisibleForTesting
    static boolean isTargetFullyVisible(RecyclerView recyclerView, int targetIndex) {
        RecyclerView.ViewHolder viewHolder =
                recyclerView.findViewHolderForAdapterPosition(targetIndex);
        if (viewHolder == null) return false;

        View view = viewHolder.itemView;
        if (!view.isShown()) return false;

        Rect actualPosition = new Rect();
        view.getGlobalVisibleRect(actualPosition);

        return view.getMeasuredHeight() == actualPosition.height();
    }

    /**
     * An {@link OnScrollListener} that detects when a smooth scroll is finished or interrupted by
     * the user. This is important because there is no direct way to know if a smooth scroll was
     * interrupted.
     */
    private static class InterruptionDetectingScrollListener extends OnScrollListener {
        private final Runnable mOnScrollFinishedWrapped;

        /**
         * @param onScrollFinishedWrapped The {@link Runnable} to be called when the scroll is
         *     finished or interrupted.
         */
        public InterruptionDetectingScrollListener(Runnable onScrollFinishedWrapped) {
            mOnScrollFinishedWrapped = onScrollFinishedWrapped;
        }

        @Override
        public void onScrollStateChanged(RecyclerView recyclerView, int newState) {
            super.onScrollStateChanged(recyclerView, newState);

            // The RecyclerView should be in SCROLL_STATE_SETTLING when automatically scrolling.
            if (newState == RecyclerView.SCROLL_STATE_DRAGGING
                    || newState == RecyclerView.SCROLL_STATE_IDLE) {
                mOnScrollFinishedWrapped.run();
                recyclerView.removeOnScrollListener(this);
            }
        }
    }
}
