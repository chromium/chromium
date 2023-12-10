// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.eventfilter;

import android.content.Context;
import android.os.Handler;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.ViewConfiguration;

import org.chromium.base.MathUtils;
import org.chromium.chrome.browser.layouts.EventFilter;

/**
 * Filters events that would trigger gestures like scroll and fling and other motion events like
 * hover.
 */
public class MotionEventFilter extends EventFilter {
    private final int mLongPressTimeoutMs;
    private final GestureDetector mDetector;
    private final MotionEventHandler mHandler;
    private final boolean mUseDefaultLongPress;
    private final int mScaledTouchSlop;
    private boolean mSingleInput = true;
    private boolean mInLongPress;
    private boolean mSeenFirstScrollEvent;
    private int mButtons;
    private LongPressRunnable mLongPressRunnable = new LongPressRunnable();
    private Handler mLongPressHandler = new Handler();

    /** A runnable to send a delayed long press. */
    private class LongPressRunnable implements Runnable {
        private MotionEvent mInitialEvent;
        private boolean mIsPending;

        public void init(MotionEvent e) {
            if (mInitialEvent != null) {
                mInitialEvent.recycle();
            }
            mInitialEvent = MotionEvent.obtain(e);
            mIsPending = true;
        }

        @Override
        public void run() {
            longPress(mInitialEvent);
            mIsPending = false;
        }

        public void cancel() {
            mIsPending = false;
        }

        public boolean isPending() {
            return mIsPending;
        }

        public MotionEvent getInitialEvent() {
            return mInitialEvent;
        }
    }

    /** Creates a {@link MotionEventFilter} with offset touch events. */
    public MotionEventFilter(Context context, MotionEventHandler handler) {
        this(context, handler, true);
    }

    /** Creates a {@link MotionEventFilter} with default long press behavior. */
    public MotionEventFilter(Context context, MotionEventHandler handler, boolean autoOffset) {
        this(context, handler, autoOffset, true);
    }

    /**
     * Creates a {@link MotionEventFilter}.
     *
     * @param useDefaultLongPress If true, use Android's long press behavior which does not send any
     *     more events after a long press. If false, use a custom implementation that will send
     *     events after a long press.
     */
    public MotionEventFilter(
            Context context,
            MotionEventHandler handler,
            boolean autoOffset,
            boolean useDefaultLongPress) {
        super(context, autoOffset);
        assert handler != null;
        mScaledTouchSlop = ViewConfiguration.get(context).getScaledTouchSlop();
        mLongPressTimeoutMs = ViewConfiguration.getLongPressTimeout();
        mUseDefaultLongPress = useDefaultLongPress;
        mHandler = handler;
        context.getResources();

        var gestureListener =
                new GestureDetector.SimpleOnGestureListener() {
                    private float mOnScrollBeginX;
                    private float mOnScrollBeginY;

                    @Override
                    public boolean onScroll(
                            MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
                        if (!mSeenFirstScrollEvent) {
                            // Remove touch slop region from the first scroll event to avoid a jump.
                            mSeenFirstScrollEvent = true;
                            float distance = MathUtils.distance(distanceX, distanceY);
                            if (distance > 0.0f) {
                                float ratio = Math.max(0, distance - mScaledTouchSlop) / distance;
                                mOnScrollBeginX = e1.getX() + distanceX * (1.0f - ratio);
                                mOnScrollBeginY = e1.getY() + distanceY * (1.0f - ratio);
                                distanceX *= ratio;
                                distanceY *= ratio;
                            }
                        }
                        if (mSingleInput) {
                            // distanceX/Y only represent the distance since the last event, not the
                            // total distance for the full scroll.  Calculate the total distance
                            // here.
                            float e2X = e2.getX();
                            float e2Y = e2.getY();

                            float totalX = e2X - mOnScrollBeginX;
                            float totalY = e2Y - mOnScrollBeginY;
                            float pxToDp = mPxToDp;
                            mHandler.drag(
                                    e2X * pxToDp,
                                    e2Y * pxToDp,
                                    -distanceX * pxToDp,
                                    -distanceY * pxToDp,
                                    totalX * pxToDp,
                                    totalY * pxToDp);
                        }
                        return true;
                    }

                    @Override
                    public boolean onSingleTapUp(MotionEvent e) {
                        // Android's GestureDector calls listener.onSingleTapUp on
                        // MotionEvent.ACTION_UP during long press, so we need to explicitly not
                        // call handler.click if a long press has been detected.
                        if (mSingleInput && !mInLongPress) {
                            float pxToDp = mPxToDp;
                            mHandler.click(
                                    e.getX() * pxToDp,
                                    e.getY() * pxToDp,
                                    e.getToolType(0) == MotionEvent.TOOL_TYPE_MOUSE,
                                    mButtons);
                        }
                        return true;
                    }

                    @Override
                    public boolean onFling(
                            MotionEvent e1, MotionEvent e2, float velocityX, float velocityY) {
                        if (mSingleInput) {
                            float pxToDp = mPxToDp;
                            mHandler.fling(
                                    e1.getX() * pxToDp,
                                    e1.getY() * pxToDp,
                                    velocityX * pxToDp,
                                    velocityY * pxToDp);
                        }
                        return true;
                    }

                    @Override
                    public boolean onDown(MotionEvent e) {
                        mButtons = e.getButtonState();
                        mInLongPress = false;
                        mSeenFirstScrollEvent = false;
                        if (mSingleInput) {
                            mHandler.onDown(
                                    e.getX() * mPxToDp,
                                    e.getY() * mPxToDp,
                                    e.getToolType(0) == MotionEvent.TOOL_TYPE_MOUSE,
                                    mButtons);
                        }
                        return true;
                    }

                    @Override
                    public void onLongPress(MotionEvent e) {
                        longPress(e);
                    }
                };
        mDetector = new GestureDetector(context, gestureListener);
        mDetector.setIsLongpressEnabled(mUseDefaultLongPress);
    }

    private void longPress(MotionEvent e) {
        if (mSingleInput) {
            mInLongPress = true;
            float pxToDp = mPxToDp;
            mHandler.onLongPress(e.getX() * pxToDp, e.getY() * pxToDp);
        }
    }

    @Override
    public boolean onInterceptTouchEventInternal(MotionEvent e, boolean isKeyboardShowing) {
        return true;
    }

    @Override
    public boolean onInterceptHoverEventInternal(MotionEvent e) {
        return true;
    }

    private void cancelLongPress() {
        var longPressRunnable = mLongPressRunnable;
        mLongPressHandler.removeCallbacks(longPressRunnable);
        longPressRunnable.cancel();
    }

    @Override
    public boolean onTouchEventInternal(MotionEvent e) {
        final int action = e.getActionMasked();
        var longPressRunnable = mLongPressRunnable;
        boolean isMultiTouch = e.getPointerCount() > 1;

        // This path mimics the Android long press detection while still allowing
        // other touch events to come through the gesture detector.
        if (!mUseDefaultLongPress) {
            if (isMultiTouch) {
                // If there's more than one pointer ignore the long press.
                if (longPressRunnable.isPending()) {
                    cancelLongPress();
                }
            } else if (action == MotionEvent.ACTION_DOWN) {
                // If there was a pending event kill it off.
                if (longPressRunnable.isPending()) {
                    cancelLongPress();
                }
                longPressRunnable.init(e);
                mLongPressHandler.postDelayed(longPressRunnable, mLongPressTimeoutMs);
            } else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
                cancelLongPress();
            } else if (longPressRunnable.isPending()) {
                // Allow for a little bit of touch slop.
                MotionEvent initialEvent = longPressRunnable.getInitialEvent();
                float distanceX = initialEvent.getX() - e.getX();
                float distanceY = initialEvent.getY() - e.getY();
                float distance = distanceX * distanceX + distanceY * distanceY;

                // Save a square root here by comparing to squared touch slop
                if (distance > mScaledTouchSlop * mScaledTouchSlop) {
                    cancelLongPress();
                }
            }
        }

        // Sends the pinch event if two or more fingers touch the screen. According to test
        // Android handles the fingers order pretty consistently so always requesting
        // index 0 and 1 works here.
        // This might need some rework if 3 fingers event are supported.
        if (isMultiTouch) {
            float pxToDp = mPxToDp;
            mHandler.onPinch(
                    e.getX(0) * pxToDp,
                    e.getY(0) * pxToDp,
                    e.getX(1) * pxToDp,
                    e.getY(1) * pxToDp,
                    action == MotionEvent.ACTION_POINTER_DOWN);
        }
        mSingleInput = !isMultiTouch;
        mDetector.setIsLongpressEnabled(!isMultiTouch && mUseDefaultLongPress);
        mDetector.onTouchEvent(e);

        // Propagate the up event after any gesture events.
        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
            mHandler.onUpOrCancel();
        }
        return true;
    }

    @Override
    public boolean onHoverEventInternal(MotionEvent e) {
        final int action = e.getActionMasked();
        float pxToDp = mPxToDp;
        float x = e.getX() * pxToDp;
        float y = e.getY() * pxToDp;
        if (action == MotionEvent.ACTION_HOVER_ENTER) {
            mHandler.onHoverEnter(x, y);
        } else if (action == MotionEvent.ACTION_HOVER_MOVE) {
            mHandler.onHoverMove(x, y);
        } else if (action == MotionEvent.ACTION_HOVER_EXIT) {
            mHandler.onHoverExit();
        }
        return true;
    }
}
