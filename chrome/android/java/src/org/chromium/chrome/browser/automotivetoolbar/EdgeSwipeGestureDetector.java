// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.automotivetoolbar;

import android.content.Context;
import android.view.GestureDetector;
import android.view.MotionEvent;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.widget.TouchEventObserver;

/**
 * This observer to listen to motion events, and determine whenever a valid swipe occurs (i.e. from
 * the correct edge and the correct length). Triggers a callback whenever valid swipe occurs.
 */
public class EdgeSwipeGestureDetector implements TouchEventObserver {
    private AutomotiveBackButtonToolbarCoordinator.OnSwipeCallback mOnSwipeCallback;
    private GestureDetector mDetector;

    private final GestureDetector.SimpleOnGestureListener mSwipeGestureListener =
            new GestureDetector.SimpleOnGestureListener() {
                @Override
                public boolean onScroll(
                        @Nullable MotionEvent e1,
                        @NonNull MotionEvent e2,
                        float distanceX,
                        float distanceY) {
                    if (shouldHandleSwipe()) {
                        mOnSwipeCallback.handleSwipe();
                    }
                    return true;
                }
            };

    /**
     * Create an edge swipe gesture detector.
     *
     * @param context context from activity.
     * @param onSwipeCallback callback called whenever a valid swipe occurs.
     */
    public EdgeSwipeGestureDetector(
            Context context,
            AutomotiveBackButtonToolbarCoordinator.OnSwipeCallback onSwipeCallback) {
        mOnSwipeCallback = onSwipeCallback;
        mDetector = new GestureDetector(context, mSwipeGestureListener);
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent e) {
        return false;
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent e) {
        return false;
    }

    private boolean shouldHandleSwipe() {
        // TODO(jtanaristy): implement shouldHandleSwipe.
        return false;
    }
}
