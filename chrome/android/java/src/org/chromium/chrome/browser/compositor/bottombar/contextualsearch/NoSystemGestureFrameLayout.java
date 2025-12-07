// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.content.Context;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.widget.FrameLayout;

import org.chromium.build.annotations.NullMarked;

import java.util.Collections;

/** A FrameLayout that does not have System Gesture. */
@NullMarked
public class NoSystemGestureFrameLayout extends FrameLayout {
    public NoSystemGestureFrameLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    @SuppressWarnings("DrawAllocation")
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        setSystemGestureExclusionRects(
                Collections.singletonList(
                        new Rect(0, 0, Math.abs(right - left), Math.abs(top - bottom))));
    }
}
