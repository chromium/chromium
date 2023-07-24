// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.eventfilter;

import android.content.Context;
import android.graphics.RectF;
import android.view.MotionEvent;

import androidx.annotation.VisibleForTesting;

/**
 * A {@link AreaGestureEventFilter} intercepts all events that start in a specific Rect on the
 * screen.
 * TODO (crbug.com/1451925): Rename this class to reflect handling of gesture as well as other
 * generic motion events.
 */
public class AreaGestureEventFilter extends GestureEventFilter {
    private final RectF mTriggerRect = new RectF();

    /** Whether a down event has occurred inside of the specified area. */
    private boolean mHasDownEventInArea;

    /** Whether a hover enter or move event has occurred inside of the specified area. */
    private boolean mHasHoverEnterOrMoveEventInArea;
    /** Whether a hover exit event has occurred from the specified area. */
    private boolean mHoverExitedArea;

    /**
     * Creates a {@link AreaGestureEventFilter}.
     * @param context       The context to build the gesture handler under.
     * @param handler       The handler to be notified of gesture events.
     * @param triggerRect   The area that events should be stolen from in dp.
     */
    public AreaGestureEventFilter(Context context, GestureHandler handler, RectF triggerRect) {
        this(context, handler, triggerRect, true);
    }

    /**
     * Creates a {@link AreaGestureEventFilter}.
     * @param context       The context to build the gesture handler under.
     * @param handler       The handler to be notified of gesture events.
     * @param triggerRect   The area that events should be stolen from in dp.
     * @param autoOffset    Whether or not to offset touch events.
     */
    public AreaGestureEventFilter(
            Context context, GestureHandler handler, RectF triggerRect, boolean autoOffset) {
        super(context, handler, autoOffset);
        setEventArea(triggerRect);
    }

    /**
     * Creates a {@link AreaGestureEventFilter}.
     * @param context               The context to build the gesture handler under.
     * @param handler               The handler to be notified of gesture events.
     * @param triggerRect           The area that events should be stolen from in dp.
     * @param autoOffset            Whether or not to offset touch events.
     * @param useDefaultLongPress   Whether or not to use the default long press behavior.
     */
    public AreaGestureEventFilter(Context context, GestureHandler handler, RectF triggerRect,
            boolean autoOffset, boolean useDefaultLongPress) {
        super(context, handler, autoOffset, useDefaultLongPress);
        setEventArea(triggerRect);
    }

    /**
     * @param rect The area that events should be stolen from in dp.
     */
    public void setEventArea(RectF rect) {
        if (rect == null) {
            mTriggerRect.setEmpty();
        } else {
            mTriggerRect.set(rect);
        }
    }

    @Override
    public boolean onTouchEventInternal(MotionEvent e) {
        // If the action is up or down, consider it to be a new gesture.
        if (e.getActionMasked() == MotionEvent.ACTION_UP
                || e.getActionMasked() == MotionEvent.ACTION_DOWN) {
            mHasDownEventInArea = false;
        }
        return super.onTouchEventInternal(e);
    }

    @Override
    public boolean onInterceptTouchEventInternal(MotionEvent e, boolean isKeyboardShowing) {
        // If the action is up or down, consider it to be a new gesture.
        if (e.getActionMasked() == MotionEvent.ACTION_UP
                || e.getActionMasked() == MotionEvent.ACTION_DOWN) {
            mHasDownEventInArea = false;
        }
        if (isMotionEventInArea(e)) {
            if (e.getActionMasked() == MotionEvent.ACTION_DOWN) {
                mHasDownEventInArea = true;
            } else if (!mHasDownEventInArea) {
                return false;
            }
            return super.onInterceptTouchEventInternal(e, isKeyboardShowing);
        }
        return false;
    }

    @Override
    public boolean onHoverEventInternal(MotionEvent e) {
        // |mHoverExitedArea| determines whether a hover event within the parent view in which
        // |mTriggerRect| is present causes a hover out of this rect; in this case, we will process
        // this action as an ACTION_HOVER_EXIT from the rect area.
        if (mHoverExitedArea) {
            mHoverExitedArea = false;
            MotionEvent exitEvent = MotionEvent.obtain(e);
            exitEvent.setAction(MotionEvent.ACTION_HOVER_EXIT);
            exitEvent.recycle();
            return super.onHoverEventInternal(exitEvent);
        }
        return super.onHoverEventInternal(e);
    }

    @Override
    public boolean onInterceptHoverEventInternal(MotionEvent e) {
        if (isMotionEventInArea(e)) {
            if (e.getActionMasked() == MotionEvent.ACTION_HOVER_ENTER
                    || e.getActionMasked() == MotionEvent.ACTION_HOVER_MOVE) {
                mHasHoverEnterOrMoveEventInArea = true;
                return super.onInterceptHoverEventInternal(e);
            }
        } else {
            // If there was a previous hover into/within the rect, potentially handle hovering out
            // of this rect.
            if (mHasHoverEnterOrMoveEventInArea) {
                mHasHoverEnterOrMoveEventInArea = false;
                mHoverExitedArea = true;
                return super.onInterceptHoverEventInternal(e);
            }
        }
        return false;
    }

    @VisibleForTesting
    boolean isMotionEventInArea(MotionEvent e) {
        return mTriggerRect.contains(e.getX() * mPxToDp, e.getY() * mPxToDp);
    }

    boolean getHasHoverEnterOrMoveEventInAreaForTesting() {
        return mHasHoverEnterOrMoveEventInArea;
    }

    boolean getHoverExitedAreaForTesting() {
        return mHoverExitedArea;
    }
}
