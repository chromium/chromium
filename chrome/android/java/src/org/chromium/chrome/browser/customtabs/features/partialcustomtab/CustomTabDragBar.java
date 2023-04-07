// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import android.annotation.SuppressLint;
import android.content.Context;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.widget.FrameLayout;

import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar.HandleStrategy;

/**
 * Top strip view of the custom tab toolbar. Pass forward touch events to
 * {@link CustomTabHandleStrategy} for dragging CCT up and down.
 */
public class CustomTabDragBar extends FrameLayout {
    private HandleStrategy mHandleStrategy;

    public CustomTabDragBar(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public void setHandleStrategy(HandleStrategy handleStrategy) {
        mHandleStrategy = handleStrategy;
    }

    @SuppressLint("ClickableViewAccessibility")
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return mHandleStrategy != null ? mHandleStrategy.onTouchEvent(event) : false;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent event) {
        return mHandleStrategy != null ? mHandleStrategy.onInterceptTouchEvent(event) : false;
    }
}
