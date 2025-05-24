// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.eventfilter;

import android.content.Context;
import android.graphics.RectF;
import android.view.MotionEvent;

import androidx.annotation.VisibleForTesting;

import org.chromium.ui.util.MotionEventUtils;

/**
 * A {@link AreaMotionEventFilter} intercepts all events that start in a specific Rect on the
 * screen.
 */
public class AreaMotionEventFilter extends MotionEventFilter {
    private final RectF mTriggerRect = new RectF();

    /** Whether a down event has occurred inside of the specified area. */
    private boolean mHasDownEventInArea;

    /** Whether a hover enter or move event has occurred inside of the specified area. */
    private boolean mHasHoverEnterOrMoveEventInArea;

    /** Whether a hover exit event has occurred from the specified area. */
    private boolean mHoverExitedArea;

    /** The handler for this instance that is used to notify owner of events/actions. */
    private final MotionEventHandler mHandler;

    /**
     * Creates a {@link AreaMotionEventFilter}.
     *
     * @param context The context to build the gesture handler under.
     * @param handler The handler to be notified of gesture events.
     * @param triggerRect The area that events should be stolen from in dp.
     * @param autoOffset Whether or not to offset touch events.
     * @param useDefaultLongPress Whether or not to use the default long press behavior.
     */
    public AreaMotionEventFilter(
            Context context,
            MotionEventHandler handler,
            RectF triggerRect,
            boolean autoOffset,
            boolean useDefaultLongPress) {
        super(context, handler, autoOffset, useDefaultLongPress);
        this.mHandler = handler;
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
            } else if (e.getActionMasked() == MotionEvent.ACTION_HOVER_EXIT) {
                // A hover exit event is recorded inside the filter area when another screen
                // interaction through a gesture event occurs. In this case, we will give a chance
                // to the event filter to handle such an exit event if a previous hover entry/move
                // event was intercepted.
                if (mHasHoverEnterOrMoveEventInArea) {
                    mHasHoverEnterOrMoveEventInArea = false;
                    return super.onInterceptHoverEventInternal(e);
                }
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

    @Override
    protected boolean onGenericMotionEventInternal(MotionEvent e) {
        // Do not consume events that do not happen within this area.
        if (!isMotionEventInArea(e)) {
            return false;
        }

        // This filter is currently only interested in acting on mouse and trackpad events.
        if (!MotionEventUtils.isMouseEvent(e) && !MotionEventUtils.isTrackpadEvent(e)) {
            return false;
        }

        // For scrolls, we will call the handler's scroll method to allow the MotionEventHandler
        // to handle the event as needed (e.g. scroll the tab strip). For other actions generating
        // motion events, we will intercept them to prevent them from leaking to underlying layers,
        // e.g. preventing clicks from leaking through the tab strip to the web contents behind it.
        int action = e.getActionMasked();
        if (action == MotionEvent.ACTION_SCROLL) {
            mHandler.onScroll(
                    e.getAxisValue(MotionEvent.AXIS_HSCROLL),
                    e.getAxisValue(MotionEvent.AXIS_VSCROLL));
            return true;
        } else if (action == MotionEvent.ACTION_BUTTON_PRESS
                || action == MotionEvent.ACTION_BUTTON_RELEASE) {
            return true;
        }
        return false;
    }

    /** Gets the motion event area rect for testing purposes. */
    public RectF getEventAreaForTesting() {
        return mTriggerRect;
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
