// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub.swipe;

import android.content.Context;
import android.view.MotionEvent;
import android.view.VelocityTracker;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewParent;

import org.chromium.base.MathUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.R;

/**
 * Handles touch event interception and processing for horizontal swipe gestures to switch panes in
 * the Hub.
 */
@NullMarked
public class HubPaneSwipeGestureHandler {
    /**
     * Delegate to receive gesture events and provide container dimensions and interactive checks.
     */
    public interface SwipeGestureDelegate {
        /** Returns the width of the pane container in pixels. */
        int getContainerWidth();

        /**
         * Returns whether the touch at coordinates relative to host view is on an interactive child
         * element.
         */
        boolean isTouchOnInteractiveElement(float hostX, float hostY);

        /**
         * Called when a drag gesture begins past the touch slop. Prepares and returns the adjacent
         * pane root view, or null if no adjacent pane exists.
         */
        @Nullable View onSwipeStarted(boolean isSwipeLeft);

        /** Called on each drag movement with clamped delta X, progress [0..1], and direction. */
        void onSwipeProgress(float dx, float progress, boolean isSwipeLeft);

        /**
         * Called when the swipe drag is released, indicating whether it should switch or settle.
         */
        void onSwipeSettled(boolean shouldSwitch, boolean isSwipeLeft);

        /** Called when the swipe drag is cancelled or aborted without animation. */
        void onSwipeCancelled();
    }

    private static final float SWIPE_SWITCH_DISTANCE_FRACTION = 1.0f / 3.0f;

    private final int mSwipeEdgeGutterWidth;
    private final int mSwipeTouchSlop;
    private final int mMinSwipeFlingVelocity;
    private final SwipeGestureDelegate mDelegate;

    private boolean mIsSwipeBeingDragged;
    private boolean mCanInterceptSwipe;
    private float mSwipeInitialDownX;
    private float mSwipeInitialDownY;
    private boolean mSwipeDirectionIsLeft;
    private boolean mHasPreparedAdjacentView;
    private @Nullable VelocityTracker mVelocityTracker;

    /**
     * Creates a new gesture handler.
     *
     * @param context The Android context.
     * @param delegate The delegate handling swipe updates.
     */
    public HubPaneSwipeGestureHandler(Context context, SwipeGestureDelegate delegate) {
        mDelegate = delegate;
        ViewConfiguration vc = ViewConfiguration.get(context);
        mSwipeEdgeGutterWidth =
                context.getResources().getDimensionPixelSize(R.dimen.hub_edge_swipe_gutter_width);
        mSwipeTouchSlop = vc.getScaledTouchSlop();
        mMinSwipeFlingVelocity = vc.getScaledMinimumFlingVelocity();
    }

    /**
     * Intercepts touch events for the host view to detect swipe gestures.
     *
     * @param motionEvent The motion event received by the host view.
     * @param parent The parent view for intercept disallow requests.
     * @return Whether the gesture handler intercepts the touch event.
     */
    public boolean onInterceptTouchEvent(MotionEvent motionEvent, @Nullable ViewParent parent) {
        if (!ChromeFeatureList.sEnableSwipeToSwitchPane.isEnabled()) {
            return false;
        }

        final int action = motionEvent.getActionMasked();
        if (action == MotionEvent.ACTION_CANCEL || action == MotionEvent.ACTION_UP) {
            cleanupDrag();
            return false;
        }

        if (action != MotionEvent.ACTION_DOWN && mIsSwipeBeingDragged) {
            return true;
        }

        switch (action) {
            case MotionEvent.ACTION_DOWN:
                mSwipeInitialDownX = motionEvent.getX();
                mSwipeInitialDownY = motionEvent.getY();
                mIsSwipeBeingDragged = false;
                mHasPreparedAdjacentView = false;

                mCanInterceptSwipe = checkCanInterceptSwipe(mSwipeInitialDownX, mSwipeInitialDownY);
                if (mCanInterceptSwipe) {
                    if (mVelocityTracker == null) {
                        mVelocityTracker = VelocityTracker.obtain();
                    } else {
                        mVelocityTracker.clear();
                    }
                    mVelocityTracker.addMovement(motionEvent);
                }
                break;

            case MotionEvent.ACTION_MOVE:
                if (!mCanInterceptSwipe) {
                    return false;
                }

                if (mVelocityTracker != null) {
                    mVelocityTracker.addMovement(motionEvent);
                }

                final float dx = motionEvent.getX() - mSwipeInitialDownX;
                final float dy = motionEvent.getY() - mSwipeInitialDownY;

                if (Math.abs(dx) > mSwipeTouchSlop && Math.abs(dx) > Math.abs(dy)) {
                    mIsSwipeBeingDragged = true;
                    if (parent != null) {
                        parent.requestDisallowInterceptTouchEvent(true);
                    }
                }
                break;
        }

        return mIsSwipeBeingDragged;
    }

    /**
     * Handles touch events for active swipe gestures.
     *
     * @param event The motion event.
     * @return Whether the event was handled.
     */
    public boolean onTouchEvent(MotionEvent event) {
        if (!ChromeFeatureList.sEnableSwipeToSwitchPane.isEnabled()) {
            return false;
        }

        final int action = event.getActionMasked();
        final float x = event.getX();
        final int containerWidth = mDelegate.getContainerWidth();

        if (action == MotionEvent.ACTION_DOWN) {
            mSwipeInitialDownX = event.getX();
            mSwipeInitialDownY = event.getY();
            mIsSwipeBeingDragged = false;
            mHasPreparedAdjacentView = false;
            mCanInterceptSwipe = checkCanInterceptSwipe(mSwipeInitialDownX, mSwipeInitialDownY);
            if (!mCanInterceptSwipe) {
                return false;
            }
        }

        if (!mCanInterceptSwipe) {
            return false;
        }

        if (mVelocityTracker == null) {
            mVelocityTracker = VelocityTracker.obtain();
        }
        mVelocityTracker.addMovement(event);

        switch (action) {
            case MotionEvent.ACTION_DOWN:
                break;

            case MotionEvent.ACTION_MOVE:
                if (!mIsSwipeBeingDragged) {
                    final float dx = x - mSwipeInitialDownX;
                    final float dy = event.getY() - mSwipeInitialDownY;
                    if (Math.abs(dx) > mSwipeTouchSlop && Math.abs(dx) > Math.abs(dy)) {
                        mIsSwipeBeingDragged = true;
                    }
                }

                if (mIsSwipeBeingDragged && containerWidth > 0) {
                    float dx = x - mSwipeInitialDownX;
                    if (!mHasPreparedAdjacentView) {
                        boolean isSwipeLeft = dx < 0;
                        View adjacentView = mDelegate.onSwipeStarted(isSwipeLeft);
                        if (adjacentView != null) {
                            mSwipeDirectionIsLeft = isSwipeLeft;
                            mHasPreparedAdjacentView = true;
                        } else {
                            cleanupDrag();
                            return false;
                        }
                    }

                    dx =
                            mSwipeDirectionIsLeft
                                    ? MathUtils.clamp(dx, -containerWidth, 0)
                                    : MathUtils.clamp(dx, 0, containerWidth);
                    float progress = Math.abs(dx) / (float) containerWidth;
                    mDelegate.onSwipeProgress(dx, progress, mSwipeDirectionIsLeft);
                }
                break;

            case MotionEvent.ACTION_UP:
                if (mIsSwipeBeingDragged && mHasPreparedAdjacentView && containerWidth > 0) {
                    mVelocityTracker.computeCurrentVelocity(1000);
                    float velocityX = mVelocityTracker.getXVelocity();
                    float velocityY = mVelocityTracker.getYVelocity();
                    float dx = x - mSwipeInitialDownX;

                    boolean isFling =
                            Math.abs(velocityX) > mMinSwipeFlingVelocity
                                    && Math.abs(velocityX) > Math.abs(velocityY);
                    boolean isFlingInCorrectDirection =
                            isFling
                                    && ((mSwipeDirectionIsLeft && velocityX < 0)
                                            || (!mSwipeDirectionIsLeft && velocityX > 0));
                    boolean isOppositeFling = isFling && !isFlingInCorrectDirection;

                    boolean isDisplacementEnough =
                            Math.abs(dx) > (containerWidth * SWIPE_SWITCH_DISTANCE_FRACTION);
                    boolean shouldSwitch =
                            (isDisplacementEnough && !isOppositeFling) || isFlingInCorrectDirection;

                    mDelegate.onSwipeSettled(shouldSwitch, mSwipeDirectionIsLeft);
                } else {
                    mDelegate.onSwipeCancelled();
                }
                cleanupDrag();
                break;

            case MotionEvent.ACTION_CANCEL:
                mDelegate.onSwipeCancelled();
                cleanupDrag();
                break;
        }

        return true;
    }

    /** Returns whether a swipe gesture can be intercepted from the given touch coordinate. */
    private boolean checkCanInterceptSwipe(float downX, float downY) {
        int containerWidth = mDelegate.getContainerWidth();
        boolean isEdgeTouch =
                downX <= mSwipeEdgeGutterWidth
                        || (containerWidth > 0 && downX >= containerWidth - mSwipeEdgeGutterWidth);
        boolean isInteractiveTouch = mDelegate.isTouchOnInteractiveElement(downX, downY);
        return !isEdgeTouch && !isInteractiveTouch;
    }

    /** Cleans up the ongoing drag state and recycles the velocity tracker. */
    public void cleanupDrag() {
        mIsSwipeBeingDragged = false;
        mCanInterceptSwipe = false;
        mHasPreparedAdjacentView = false;
        if (mVelocityTracker != null) {
            mVelocityTracker.recycle();
            mVelocityTracker = null;
        }
    }

    /** Returns whether a swipe gesture is actively being dragged. */
    public boolean isSwipeBeingDragged() {
        return mIsSwipeBeingDragged;
    }

    /** Returns the edge gutter width in pixels. */
    public int getSwipeEdgeGutterWidth() {
        return mSwipeEdgeGutterWidth;
    }

    public void setVelocityTrackerForTesting(VelocityTracker tracker) {
        mVelocityTracker = tracker;
    }
}
