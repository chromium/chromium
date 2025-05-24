// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.automotivetoolbar;

import android.content.Context;
import android.view.GestureDetector;
import android.view.MotionEvent;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.components.browser_ui.widget.TouchEventObserver;

/**
 * This observer to listen to motion events, and determine whenever a valid swipe occurs (i.e. from
 * the correct edge and the correct length). Triggers a callback whenever valid swipe occurs.
 */
@SuppressWarnings("UnusedVariable") // Remove once implementation is complete.
public class EdgeSwipeGestureDetector implements TouchEventObserver {
    // Width of back button toolbar in dp on the left edge used for navigation.
    // Swipe beginning from a point within these rects triggers the back button toolbar to visible.
    // When the back button toolbar is visible, swipes ending within hide the back button toolbar.
    @VisibleForTesting static final int EDGE_WIDTH_DP = 77;

    // Minimum horizontal width of a scroll to be considered a swipe.
    static final int SWIPE_THRESHOLD_DP = 48;

    // Weighted value to determine when to trigger an edge swipe. Initial scroll
    // vector should form 30 deg or below to initiate swipe action.
    private static final float WEIGHTED_TRIGGER_THRESHOLD = 1.73f;

    // |EDGE_WIDTH_DP| in physical pixel.
    private final float mEdgeWidthPx;
    private final float mSwipeThreshold;

    private final AutomotiveBackButtonToolbarCoordinator.OnSwipeCallback mOnSwipeCallback;
    private final GestureDetector mDetector;
    private boolean mIsReadyForNewScroll;

    private final GestureDetector.SimpleOnGestureListener mSwipeGestureListener =
            new GestureDetector.SimpleOnGestureListener() {
                @Override
                public boolean onScroll(
                        @Nullable MotionEvent startMotion,
                        @NonNull MotionEvent currentMotion,
                        float distanceX,
                        float distanceY) {
                    if (!mIsReadyForNewScroll || startMotion == null) {
                        return false;
                    }
                    if (isValidSwipe(startMotion, currentMotion, distanceX, distanceY)) {
                        mIsReadyForNewScroll = false;
                        mOnSwipeCallback.handleSwipe();
                        return true;
                    }
                    if (isValidBackSwipe(startMotion, currentMotion, distanceX, distanceY)) {
                        // TODO(https://crbug.com/384991179): Ensure back swipe works for horizontal
                        // toolbar and RTL
                        mIsReadyForNewScroll = false;
                        mOnSwipeCallback.handleBackSwipe();
                        return true;
                    }
                    return false;
                }
            };

    /**
     * Create an edge swipe gesture detector.
     *
     * @param context context from activity.
     * @param onSwipeCallback callback called whenever a valid swipe occurs.
     */
    public EdgeSwipeGestureDetector(
            Context context,
            AutomotiveBackButtonToolbarCoordinator.OnSwipeCallback onSwipeCallback) {
        mOnSwipeCallback = onSwipeCallback;
        mEdgeWidthPx = EDGE_WIDTH_DP * context.getResources().getDisplayMetrics().density;
        mSwipeThreshold = SWIPE_THRESHOLD_DP * context.getResources().getDisplayMetrics().density;
        mDetector = new GestureDetector(context, mSwipeGestureListener);
        mIsReadyForNewScroll = true;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent e) {
        return false;
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent e) {
        mDetector.onTouchEvent(e);
        return false;
    }

    /** Clear the state and mark the detector ready to detect a new swipe gesture. */
    public void setIsReadyForNewScroll(boolean isReadyForNewScroll) {
        mIsReadyForNewScroll = isReadyForNewScroll;
    }

    /**
     * Processes whether the scroll is a horizontal swipe from the left edge.
     *
     * @param startMotion event for the start of the swipe.
     * @param currentMotion event for where the swipe currently is.
     * @param distanceX X delta between previous and current motion event.
     * @param distanceY Y delta between previous and current motion event.
     * @return Whether the swipe is valid or not.
     */
    @VisibleForTesting
    public boolean isValidSwipe(
            MotionEvent startMotion, MotionEvent currentMotion, float distanceX, float distanceY) {
        return Math.abs(startMotion.getX() - currentMotion.getX()) > mSwipeThreshold
                && Math.abs(distanceX) > Math.abs(distanceY) * WEIGHTED_TRIGGER_THRESHOLD
                && startMotion.getX() < mEdgeWidthPx;
    }

    /**
     * Processes whether the scroll is a horizontal back swipe from right to left. A horizontal back
     * swipe occurs whenever a swipe ends within the back button toolbar width.
     *
     * @param startMotion event for the start of the swipe.
     * @param currentMotion event for where the swipe currently is.
     * @param distanceX X delta between previous and current motion event.
     * @param distanceY Y delta between previous and current motion event.
     * @return Whether the back swipe is valid or not.
     */
    @VisibleForTesting
    public boolean isValidBackSwipe(
            MotionEvent startMotion, MotionEvent currentMotion, float distanceX, float distanceY) {
        return startMotion.getX() - currentMotion.getX() > mSwipeThreshold
                && Math.abs(distanceX) > Math.abs(distanceY) * WEIGHTED_TRIGGER_THRESHOLD
                && currentMotion.getX() < mEdgeWidthPx;
    }

    GestureDetector.SimpleOnGestureListener getSwipeGestureListenerForTesting() {
        return mSwipeGestureListener;
    }
}
