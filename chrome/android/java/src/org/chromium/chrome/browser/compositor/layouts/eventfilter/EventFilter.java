// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.eventfilter;

import android.content.Context;
import android.view.MotionEvent;

import androidx.annotation.VisibleForTesting;

/**
 * {@link EventFilter} is an abstract minimal {@link EventFilter}. This class is designed to use or
 * propagate events from an {@link EventFilterHost} view.
 */
public abstract class EventFilter {
    protected final float mPxToDp;
    private boolean mSimulateIntercepting;

    private boolean mAutoOffset;
    protected float mCurrentTouchOffsetX;
    protected float mCurrentTouchOffsetY;

    /**
     * Creates a {@link EventFilter}.
     * @param context A {@link Context} instance.
     */
    public EventFilter(Context context) {
        this(context, true);
    }

    /**
     * Creates a {@link EventFilter}.
     * @param autoOffset Whether or not to automatically offset touch events.
     */
    public EventFilter(Context context, boolean autoOffset) {
        mPxToDp = 1.0f / context.getResources().getDisplayMetrics().density;
        mAutoOffset = autoOffset;
    }

    /**
     * @see android.view.ViewGroup#onInterceptTouchEvent(android.view.MotionEvent)
     * @param event             The {@link MotionEvent} that started the gesture to be evaluated.
     * @param isKeyboardShowing Whether the keyboard is currently showing.
     * @return                  Whether the filter is going to intercept events.
     */
    public final boolean onInterceptTouchEvent(MotionEvent event, boolean isKeyboardShowing) {
        MotionEvent sentEvent = event;
        if (mAutoOffset && (mCurrentTouchOffsetX != 0 || mCurrentTouchOffsetY != 0)) {
            sentEvent = MotionEvent.obtain(event);
            sentEvent.offsetLocation(mCurrentTouchOffsetX, mCurrentTouchOffsetY);
        }
        boolean consumed = onInterceptTouchEventInternal(sentEvent, isKeyboardShowing);
        if (sentEvent != event) sentEvent.recycle();
        return consumed;
    }

    /**
     * Sets the offset to apply to MotionEvents.
     *
     * @param offsetX How much to offset the X touch events in pixels.
     * @param offsetY How much to offset the Y touch events in pixels.
     */
    public void setCurrentMotionEventOffsets(float offsetX, float offsetY) {
        mCurrentTouchOffsetX = offsetX;
        mCurrentTouchOffsetY = offsetY;
    }

    /**
     * This function has a fairly uncommon behavior, if you change anything please checkout the
     * java-doc here:
     * @see android.view.ViewGroup#onInterceptTouchEvent(android.view.MotionEvent)
     * @param event             The {@link MotionEvent} that started the gesture to be evaluated.
     * @param isKeyboardShowing Whether the keyboard is currently showing.
     * @return                  Whether the filter is going to intercept events.
     */
    protected abstract boolean onInterceptTouchEventInternal(
            MotionEvent event, boolean isKeyboardShowing);

    /**
     * @see android.view.ViewGroup#onTouchEvent(android.view.MotionEvent)
     * @param event The {@link MotionEvent} that started the gesture to be evaluated.
     * @return      Whether the filter handled the event.
     */
    public final boolean onTouchEvent(MotionEvent event) {
        if (mAutoOffset) event.offsetLocation(mCurrentTouchOffsetX, mCurrentTouchOffsetY);
        return onTouchEventInternal(event);
    }

    /**
     * @see android.view.ViewGroup#onTouchEvent(android.view.MotionEvent)
     * @param event The {@link MotionEvent} that started the gesture to be evaluated.
     * @return      Whether the filter handled the event.
     */
    protected abstract boolean onTouchEventInternal(MotionEvent event);

    /**
     * Simulates an event for testing purpose. This will call onInterceptTouchEvent and
     * onTouchEvent appropriately.
     * @param event             The {@link MotionEvent} that started the gesture to be evaluated.
     * @param isKeyboardShowing Whether the keyboard is currently showing.
     * @return                  Whether the filter handled the event.
     */
    @VisibleForTesting
    public boolean simulateTouchEvent(MotionEvent event, boolean isKeyboardShowing) {
        if (event.getActionMasked() == MotionEvent.ACTION_DOWN || !mSimulateIntercepting) {
            mSimulateIntercepting = onInterceptTouchEvent(event, isKeyboardShowing);
        }
        return onTouchEvent(event);
    }

    /**
     * @return Whether or not touch events will be automatically offset.
     */
    protected boolean autoOffsetEvents() {
        return mAutoOffset;
    }
}
