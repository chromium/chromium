// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.eventfilter;

import android.content.Context;
import android.os.Handler;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.ViewConfiguration;

import org.chromium.chrome.browser.layouts.EventFilter;

/**
 * Filters events that would trigger gestures like scroll and fling.
 */
public class GestureEventFilter extends EventFilter {
    private final int mLongPressTimeoutMs;
    private final GestureDetector mDetector;
    private final GestureHandler mHandler;
    private final boolean mUseDefaultLongPress;
    private final int mScaledTouchSlop;
    private boolean mSingleInput = true;
    private boolean mInLongPress;
    private boolean mSeenFirstScrollEvent;
    private int mButtons;
    private LongPressRunnable mLongPressRunnable = new LongPressRunnable();
    private Handler mLongPressHandler = new Handler();

    /**
     * A runnable to send a delayed long press.
     */
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

    /**
     * Creates a {@link GestureEventFilter} with offset touch events.
     */
    public GestureEventFilter(Context context, GestureHandler handler) {
        this(context, handler, true);
    }

    /**
     * Creates a {@link GestureEventFilter} with default long press behavior.
     */
    public GestureEventFilter(Context context, GestureHandler handler, boolean autoOffset) {
        this(context, handler, autoOffset, true);
    }

    /**
     * Creates a {@link GestureEventFilter}.
     * @param useDefaultLongPress If true, use Android's long press behavior which does not send
     *                            any more events after a long press. If false, use a custom
     *                            implementation that will send events after a long press.
     */
    public GestureEventFilter(Context context, GestureHandler handler, boolean autoOffset,
            boolean useDefaultLongPress) {
        super(context, autoOffset);
        assert handler != null;
        mScaledTouchSlop = ViewConfiguration.get(context).getScaledTouchSlop();
        mLongPressTimeoutMs = ViewConfiguration.getLongPressTimeout();
        mUseDefaultLongPress = useDefaultLongPress;
        mHandler = handler;
        context.getResources();

        mDetector = new GestureDetector(context, new GestureDetector.SimpleOnGestureListener() {

            private float mOnScrollBeginX;
            private float mOnScrollBeginY;

            @Override
            public boolean onScroll(MotionEvent e1, MotionEvent e2,
                    float distanceX, float distanceY) {
                if (!mSeenFirstScrollEvent) {
                    // Remove the touch slop region from the first scroll event to avoid a
                    // jump.
                    mSeenFirstScrollEvent = true;
                    float distance = (float) Math.sqrt(
                            distanceX * distanceX + distanceY * distanceY);
                    if (distance > 0.0f) {
                        float ratio = Math.max(0, distance - mScaledTouchSlop) / distance;
                        mOnScrollBeginX = e1.getX() + distanceX * (1.0f - ratio);
                        mOnScrollBeginY = e1.getY() + distanceY * (1.0f - ratio);
                        distanceX *= ratio;
                        distanceY *= ratio;
                    }
                }
                if (mSingleInput) {
                    // distanceX/Y only represent the distance since the last event, not the total
                    // distance for the full scroll.  Calculate the total distance here.
                    float totalX = e2.getX() - mOnScrollBeginX;
                    float totalY = e2.getY() - mOnScrollBeginY;
                    mHandler.drag(e2.getX() * mPxToDp, e2.getY() * mPxToDp,
                            -distanceX * mPxToDp, -distanceY * mPxToDp,
                            totalX * mPxToDp, totalY * mPxToDp);
                }
                return true;
            }

            @Override
            public boolean onSingleTapUp(MotionEvent e) {
                /* Android's GestureDector calls listener.onSingleTapUp on MotionEvent.ACTION_UP
                 * during long press, so we need to explicitly not call handler.click if a
                 * long press has been detected.
                 */
                if (mSingleInput && !mInLongPress) {
                    mHandler.click(e.getX() * mPxToDp, e.getY() * mPxToDp,
                                   e.getToolType(0) == MotionEvent.TOOL_TYPE_MOUSE,
                                   mButtons);
                }
                return true;
            }

            @Override
            public boolean onFling(MotionEvent e1, MotionEvent e2,
                    float velocityX, float velocityY) {
                if (mSingleInput) {
                    mHandler.fling(e1.getX() * mPxToDp, e1.getY() * mPxToDp,
                            velocityX * mPxToDp, velocityY * mPxToDp);
                }
                return true;
            }

            @Override
            public boolean onDown(MotionEvent e) {
                mButtons = e.getButtonState();
                mInLongPress = false;
                mSeenFirstScrollEvent = false;
                if (mSingleInput) {
                    mHandler.onDown(e.getX() * mPxToDp,
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
        });

        mDetector.setIsLongpressEnabled(mUseDefaultLongPress);
    }

    private void longPress(MotionEvent e) {
        if (mSingleInput) {
            mInLongPress = true;
            mHandler.onLongPress(e.getX() * mPxToDp, e.getY() * mPxToDp);
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
        mLongPressHandler.removeCallbacks(mLongPressRunnable);
        mLongPressRunnable.cancel();
    }

    @Override
    public boolean onTouchEventInternal(MotionEvent e) {
        final int action = e.getActionMasked();

        // This path mimics the Android long press detection while still allowing
        // other touch events to come through the gesture detector.
        if (!mUseDefaultLongPress) {
            if (e.getPointerCount() > 1) {
                // If there's more than one pointer ignore the long press.
                if (mLongPressRunnable.isPending()) {
                    cancelLongPress();
                }
            } else if (action == MotionEvent.ACTION_DOWN) {
                // If there was a pending event kill it off.
                if (mLongPressRunnable.isPending()) {
                    cancelLongPress();
                }
                mLongPressRunnable.init(e);
                mLongPressHandler.postDelayed(mLongPressRunnable, mLongPressTimeoutMs);
            } else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
                cancelLongPress();
            } else if (mLongPressRunnable.isPending()) {
                // Allow for a little bit of touch slop.
                MotionEvent initialEvent = mLongPressRunnable.getInitialEvent();
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
        if (e.getPointerCount() > 1) {
            mHandler.onPinch(e.getX(0) * mPxToDp, e.getY(0) * mPxToDp,
                    e.getX(1) * mPxToDp, e.getY(1) * mPxToDp,
                    action == MotionEvent.ACTION_POINTER_DOWN);
            mDetector.setIsLongpressEnabled(false);
            mSingleInput = false;
        } else {
            mDetector.setIsLongpressEnabled(mUseDefaultLongPress);
            mSingleInput = true;
        }
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

        if (action == MotionEvent.ACTION_HOVER_ENTER) {
            mHandler.onHoverEnter(e.getX() * mPxToDp, e.getY() * mPxToDp);
        } else if (action == MotionEvent.ACTION_HOVER_MOVE) {
            mHandler.onHoverMove(e.getX() * mPxToDp, e.getY() * mPxToDp);
        } else if (action == MotionEvent.ACTION_HOVER_EXIT) {
            mHandler.onHoverExit();
        }
        return true;
    }
}
