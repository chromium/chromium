// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import android.content.Context;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.VelocityTracker;

import androidx.core.view.MotionEventCompat;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabBottomSheetStrategy.HeightStatus;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;

import java.util.function.BooleanSupplier;

/** Handling touch events for resizing the Window. */
class PartialCustomTabHandleStrategy extends GestureDetector.SimpleOnGestureListener
        implements CustomTabToolbar.HandleStrategy {
    /**
     * The base duration of the settling animation of the sheet. 218 ms is a spec for material
     * design (this is the minimum time a user is guaranteed to pay attention to something).
     */
    private static final long BASE_ANIMATION_DURATION_MS = 218;

    private static final int FLING_THRESHOLD_PX = 100;

    static final int FLING_VELOCITY_PIXELS_PER_MS = 1000;

    private final GestureDetector mGestureDetector;
    private float mLastPosY;
    private float mDeltaY;
    private boolean mSeenFirstMoveOrDown;
    private VelocityTracker mVelocityTracker;
    private Runnable mCloseHandler;

    private BooleanSupplier mIsFullHeight;
    private Supplier<Integer> mStatus;
    private DragEventCallback mDragEventCallback;

    /** Callback for drag events. */
    interface DragEventCallback {
        /**
         * Drag action gets started.
         * @param y Y position when the drag action starts.
         */
        void onDragStart(int y);

        /**
         * Drag action is in progress. Called for each move.
         * @param y Y position when the drag move happens.
         */
        void onDragMove(int y);

        /**
         * Drag action is finished.
         *
         * @param flingDistance fling distance when the drag action ends up in fling action. Zero if
         *     not.
         */
        boolean onDragEnd(int flingDistance);
    }

    public PartialCustomTabHandleStrategy(
            Context context,
            BooleanSupplier isFullHeight,
            Supplier<Integer> status,
            DragEventCallback dragEventCallback) {
        mIsFullHeight = isFullHeight;
        mStatus = status;
        mDragEventCallback = dragEventCallback;
        mGestureDetector = new GestureDetector(context, this, ThreadUtils.getUiThreadHandler());
        mVelocityTracker = VelocityTracker.obtain();
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent event) {
        return mIsFullHeight.getAsBoolean() ? false : mGestureDetector.onTouchEvent(event);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (mStatus.get() == HeightStatus.TRANSITION) {
            return true;
        }
        // We will get events directly even when onInterceptTouchEvent() didn't return true,
        // because the sub View tree might not want this event, so check orientation and
        // multi-window flags here again.
        if (mIsFullHeight.getAsBoolean()) {
            return true;
        }

        float y = event.getRawY();
        switch (MotionEventCompat.getActionMasked(event)) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_MOVE:
                if (!mSeenFirstMoveOrDown) {
                    mSeenFirstMoveOrDown = true;
                    mVelocityTracker.clear();
                    mLastPosY = y;
                    mDragEventCallback.onDragStart((int) y);
                } else {
                    mVelocityTracker.addMovement(event);
                    mDragEventCallback.onDragMove((int) y);
                }
                mDeltaY = y - mLastPosY;
                mLastPosY = y;
                return true;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                if (mSeenFirstMoveOrDown) {
                    mVelocityTracker.computeCurrentVelocity(FLING_VELOCITY_PIXELS_PER_MS);
                    float v = Math.abs(mVelocityTracker.getYVelocity());
                    int flingDist = Math.abs(v) < FLING_THRESHOLD_PX ? 0 : getFlingDistance(v);
                    int direction = (int) Math.signum(mDeltaY);
                    if (!mDragEventCallback.onDragEnd((int) (flingDist * direction))) {
                        mCloseHandler.run();
                    }
                    mSeenFirstMoveOrDown = false;
                }
                return true;
            default:
                return true;
        }
    }

    @Override
    public void setCloseClickHandler(Runnable handler) {
        mCloseHandler = handler;
    }

    @Override
    public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
        // Always intercept scroll events.
        return true;
    }

    /**
     * Gets the distance of a fling based on the velocity and the base animation time. This
     * formula assumes the deceleration curve is quadratic (t^2), hence the displacement formula
     * should be: displacement = initialVelocity * duration / 2.
     * @param velocity The velocity of the fling.
     * @return The distance the fling would cover.
     */
    private int getFlingDistance(float velocity) {
        // This includes conversion from seconds to ms.
        return (int) (velocity * BASE_ANIMATION_DURATION_MS / 2000f);
    }
}
