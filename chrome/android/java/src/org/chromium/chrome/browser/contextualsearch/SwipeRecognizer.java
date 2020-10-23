// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.content.Context;
import android.graphics.PointF;
import android.view.GestureDetector;
import android.view.GestureDetector.SimpleOnGestureListener;
import android.view.MotionEvent;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EdgeSwipeHandler;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.ScrollDirection;

/**
 * Recognizes directional swipe gestures using supplied {@link MotionEvent}s.
 * The {@EdgeSwipeHandler} callbacks will notify users when a particular gesture
 * has occurred, if the handler supports the particular direction of the swipe.
 *
 * To use this class:
 * <ul>
 *  <li>Create an instance of the {@code SwipeRecognizer} for your View
 *  <li>In the View#onTouchEvent(MotionEvent) method ensure you call
 *          {@link #onTouchEvent(MotionEvent)}. The methods defined in your callback
 *          will be executed when the gestures occur.
 *  <li>Before trying to recognize the gesture, the class will call
 *          {@link #shouldRecognizeSwipe(MotionEvent, MotionEvent)}, which allow
 *          ignoring swipe recognition based on the MotionEvents.
 *  <li>Once a swipe gesture is detected, the class will check if the the direction
 *          is supported by calling {@link EdgeSwipeHandler#isSwipeEnabled(ScrollDirection)}.
 * </ul>

 * Internally, this class uses a {@link GestureDetector} to recognize swipe gestures.
 * For convenience, this class also extends {@link SimpleOnGestureListener} which
 * is passed to the {@GestureDetector}. This means that this class can also be
 * used to detect simple gestures defined in {@link GestureDetector}.
 */
public class SwipeRecognizer extends SimpleOnGestureListener {

    /**
     * The threshold for a vertical swipe gesture, in dps.
     */
    private static final float SWIPE_VERTICAL_DRAG_THRESHOLD_DP = 5.f;

    /**
     * The threshold for a horizontal swipe gesture, in dps.
     */
    private static final float SWIPE_HORIZONTAL_DRAG_THRESHOLD_DP = 10.f;

    /**
     * The {@link EdgeSwipeHandler} that will respond to recognized gestures.
     */
    private EdgeSwipeHandler mSwipeHandler;

    /**
     * The direction of the swipe gesture.
     * TODO(pedrosimonetti): Consider renaming ScrollDirection to SwipeDirection.
     * Also consider renaming EdgeSwipeHandler to SwipeHandler or DirectionalSwipeHandler.
     * Finally, consider moving the ScrollDirection/SwipeDirection enum to this class.
     */
    private @ScrollDirection int mSwipeDirection = ScrollDirection.UNKNOWN;

    /**
     * The point that originated the swipe gesture.
     */
    private final PointF mMotionStartPoint = new PointF();

    /**
     * The dps per pixel ratio.
     */
    private final float mPxToDp;

    /**
     * The internal {@GestureDetector} used to recognize swipe gestures.
     */
    private final GestureDetector mGestureDetector;

    /**
     * @param context The current Android {@link Context}.
     */
    public SwipeRecognizer(Context context) {
        mPxToDp = 1.f / context.getResources().getDisplayMetrics().density;
        mGestureDetector = new GestureDetector(context, this, ThreadUtils.getUiThreadHandler());
    }

    /**
     * Sets the {@link EdgeSwipeHandler} that will respond to recognized gestures.
     * @param handler The {@link EdgeSwipeHandler}.
     */
    public void setSwipeHandler(EdgeSwipeHandler handler) {
        mSwipeHandler = handler;
    }

    /**
     * Analyzes the given motion event by feeding it to a {@GestureDetector}. Depending on the
     * results from the onScroll() and onFling() methods, it triggers the appropriate callbacks
     * on the {@link EdgeSwipeHandler} supplied.
     *
     * @param event The {@link MotionEvent}.
     * @return Whether the event has been consumed.
     */
    public boolean onTouchEvent(MotionEvent event) {
        boolean consumed = mGestureDetector.onTouchEvent(event);

        if (mSwipeHandler != null) {
            final int action = event.getAction();
            if ((action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL)
                    && mSwipeDirection != ScrollDirection.UNKNOWN) {
                mSwipeHandler.swipeFinished();
                mSwipeDirection = ScrollDirection.UNKNOWN;
                consumed = true;
            }
        }

        return consumed;
    }

    /**
     * Checks whether the swipe gestures should be recognized. If this method returns false,
     * then the whole swipe recognition process will be ignored. By default this method returns
     * true. If a more complex logic is needed, this method should be overridden.
     *
     * @param e1 The first {@link MotionEvent}.
     * @param e2 The second {@link MotionEvent}.
     * @return Whether the swipe gestures should be recognized
     */
    public boolean shouldRecognizeSwipe(MotionEvent e1, MotionEvent e2) {
        return true;
    }

    // ============================================================================================
    // Swipe Recognition Helpers
    // ============================================================================================

    @Override
    public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
        if (mSwipeHandler == null || e1 == null || e2 == null) return false;

        final float x = e2.getRawX() * mPxToDp;
        final float y = e2.getRawY() * mPxToDp;

        if (mSwipeDirection == ScrollDirection.UNKNOWN && shouldRecognizeSwipe(e1, e2)) {
            float tx = (e2.getRawX() - e1.getRawX()) * mPxToDp;
            float ty = (e2.getRawY() - e1.getRawY()) * mPxToDp;

            @ScrollDirection
            int direction = ScrollDirection.UNKNOWN;

            if (Math.abs(tx) > SWIPE_HORIZONTAL_DRAG_THRESHOLD_DP) {
                direction = tx > 0.f ? ScrollDirection.RIGHT : ScrollDirection.LEFT;
            } else if (Math.abs(ty) > SWIPE_VERTICAL_DRAG_THRESHOLD_DP) {
                direction = ty > 0.f ? ScrollDirection.DOWN : ScrollDirection.UP;
            }

            if (direction != ScrollDirection.UNKNOWN && mSwipeHandler.isSwipeEnabled(direction)) {
                mSwipeDirection = direction;
                mSwipeHandler.swipeStarted(direction, x, y);
                mMotionStartPoint.set(e2.getRawX(), e2.getRawY());
            }
        }

        if (mSwipeDirection != ScrollDirection.UNKNOWN) {
            final float tx = (e2.getRawX() - mMotionStartPoint.x) * mPxToDp;
            final float ty = (e2.getRawY() - mMotionStartPoint.y) * mPxToDp;
            final float dx = -distanceX * mPxToDp;
            final float dy = -distanceY * mPxToDp;

            mSwipeHandler.swipeUpdated(x, y, dx, dy, tx, ty);
            return true;
        }

        return false;
    }

    @Override
    public boolean onFling(MotionEvent e1, MotionEvent e2, float velocityX, float velocityY) {
        if (mSwipeHandler == null) return false;

        if (mSwipeDirection != ScrollDirection.UNKNOWN) {
            final float x = e2.getRawX() * mPxToDp;
            final float y = e2.getRawY() * mPxToDp;
            final float tx = (e2.getRawX() - mMotionStartPoint.x) * mPxToDp;
            final float ty = (e2.getRawY() - mMotionStartPoint.y) * mPxToDp;
            final float vx = velocityX * mPxToDp;
            final float vy = velocityY * mPxToDp;

            mSwipeHandler.swipeFlingOccurred(x, y, tx, ty, vx, vy);
            return true;
        }

        return false;
    }
}
