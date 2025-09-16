// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_component;

import android.annotation.SuppressLint;
import android.content.Context;
import android.util.AttributeSet;
import android.view.MotionEvent;

import androidx.annotation.NonNull;
import androidx.viewpager.widget.ViewPager;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** This ViewPager disables all animations - swipes and smooth transitions. */
@NullMarked
class NoSwipeViewPager extends ViewPager {
    /** Constructor for inflating from XML which is why it must be public. */
    public NoSwipeViewPager(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @SuppressLint("ClickableViewAccessibility")
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return false;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent event) {
        return false;
    }

    @Override
    public void setCurrentItem(int item) {
        // By default, setCurrentItem would enable smoothScroll. Disable it instead:
        super.setCurrentItem(item, false);
    }
}
