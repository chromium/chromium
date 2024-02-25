// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.content.Context;
import android.graphics.Rect;
import android.os.Build;
import android.util.AttributeSet;
import android.widget.FrameLayout;

import java.util.Collections;

/** A FrameLayout that does not have System Gesture. */
public class NoSystemGestureFrameLayout extends FrameLayout {
    public NoSystemGestureFrameLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    @SuppressWarnings("DrawAllocation")
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            setSystemGestureExclusionRects(
                    Collections.singletonList(
                            new Rect(0, 0, Math.abs(right - left), Math.abs(top - bottom))));
        }
    }
}
