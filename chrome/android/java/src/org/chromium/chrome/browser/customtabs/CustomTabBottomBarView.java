// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.annotation.SuppressLint;
import android.content.Context;
import android.util.AttributeSet;
import android.view.MotionEvent;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.widget.BoundedLinearLayout;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;

/** A custom container view for the Custom Tab bottom bar that supports swipe gesture handling. */
public class CustomTabBottomBarView extends BoundedLinearLayout {
    private final Context mContext;
    private @Nullable BottomBarSwipeGestureListener mSwipeGestureListener;

    /** Constructor for inflating from xml. */
    public CustomTabBottomBarView(Context context, AttributeSet attrs) {
        super(context, attrs);

        mContext = context;
    }

    /**
     * @param swipeHandler The {@link SwipeHandler} that will be notified of the swipe gestures that
     *                     happen on this view.
     */
    public void setSwipeHandler(@Nullable SwipeHandler swipeHandler) {
        if (swipeHandler == null) {
            mSwipeGestureListener = null;
            return;
        }
        mSwipeGestureListener = new BottomBarSwipeGestureListener(mContext, swipeHandler);
    }

    @Override
    @SuppressLint("ClickableViewAccessibility")
    public boolean onTouchEvent(MotionEvent event) {
        if (mSwipeGestureListener == null) return false;
        if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
            return true;
        }
        return mSwipeGestureListener.onTouchEvent(event);
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        if (mSwipeGestureListener == null) return false;

        return mSwipeGestureListener.onTouchEvent(ev);
    }

    private static class BottomBarSwipeGestureListener extends SwipeGestureListener {
        public BottomBarSwipeGestureListener(Context context, SwipeHandler handler) {
            super(context, handler);
        }
    }
}
