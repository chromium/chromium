// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic.ui;

import android.content.Context;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.widget.FrameLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Root FRE overlay that passes touch, hover and key events through once marked non-clickable. */
@NullMarked
public class GlicFreRootView extends FrameLayout {
    /** Constructor for XML inflation. */
    public GlicFreRootView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
        return isClickable() && super.dispatchTouchEvent(ev);
    }

    @Override
    protected boolean dispatchHoverEvent(MotionEvent ev) {
        return isClickable() && super.dispatchHoverEvent(ev);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent ev) {
        return isClickable() && super.dispatchKeyEvent(ev);
    }
}
