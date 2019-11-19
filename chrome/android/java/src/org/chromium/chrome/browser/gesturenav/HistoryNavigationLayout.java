// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.content.Context;
import android.util.AttributeSet;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.widget.FrameLayout;

/**
 * FrameLayout that supports side-wise slide gesture for history navigation. Inheriting
 * class may need to override {@link #wasLastSideSwipeGestureConsumed()} if
 * {@link #onTouchEvent} cannot be relied upon to know whether the side-swipe related
 * event was handled. Namely {@link android.support.v7.widget.RecyclerView}) always
 * claims to handle touch events.
 * TODO(jinsukkim): Write a test verifying UI logic.
 */
public class HistoryNavigationLayout extends FrameLayout {
    private GestureDetector mDetector;
    private NavigationHandler mNavigationHandler;
    private HistoryNavigationDelegate mDelegate = HistoryNavigationDelegateFactory.DEFAULT;

    public HistoryNavigationLayout(Context context) {
        this(context, null);
    }

    public HistoryNavigationLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Initializes navigation logic and internal objects if navigation is enabled.
     * @param delegate {@link HistoryNavigationDelegate} providing info and a factory method.
     */
    public void setNavigationDelegate(HistoryNavigationDelegate delegate) {
        mDelegate = delegate;

        // Navigation is potentially enabled only when the delegate is set.
        delegate.setWindowInsetsChangeObserver(this, () -> updateNavigationHandler());
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        updateNavigationHandler();
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        // TODO(jinsukkim): There are callsites enabling HistoryNavigationLayout but
        //         failing to call |setNavigationDelegate| (or |setTab| before renaming).
        //         Find when it can happen.
        if (mNavigationHandler != null) mNavigationHandler.reset();
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent e) {
        if (mNavigationHandler != null) {
            mDetector.onTouchEvent(e);
            mNavigationHandler.onTouchEvent(e.getAction());
        }
        return super.dispatchTouchEvent(e);
    }

    private void updateNavigationHandler() {
        if (mDelegate.isNavigationEnabled(this)) {
            if (mNavigationHandler == null) {
                mDetector = new GestureDetector(getContext(), new SideNavGestureListener());
                mNavigationHandler = new NavigationHandler(
                        this, getContext(), mDelegate, NavigationGlowFactory.forJavaLayer(this));
            }
        } else {
            mDetector = null;
            if (mNavigationHandler != null) {
                mNavigationHandler.destroy();
                mNavigationHandler = null;
            }
        }
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent e) {
        // Do not propagate touch events down to children if navigation UI was triggered.
        if (mDetector != null && mNavigationHandler.isActive()) return true;
        return super.onInterceptTouchEvent(e);
    }

    private class SideNavGestureListener extends GestureDetector.SimpleOnGestureListener {
        @Override
        public boolean onDown(MotionEvent event) {
            return mNavigationHandler.onDown();
        }

        @Override
        public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
            // |onScroll| needs handling only after the state moves away from |NONE|. This helps
            // invoke |wasLastSideSwipeGestureConsumed| which may be expensive less often.
            if (mNavigationHandler.isStopped()) return true;

            return mNavigationHandler.onScroll(
                    e1.getX(), distanceX, distanceY, e2.getX(), e2.getY());
        }
    }

    /**
     * Cancel navigation UI with animation effect.
     */
    public void release() {
        if (mNavigationHandler != null) mNavigationHandler.release(false);
    }
}
