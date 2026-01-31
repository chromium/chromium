// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_component;

import android.annotation.SuppressLint;
import android.content.Context;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.viewpager.widget.ViewPager;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.R;

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

    /**
     * To support max height and WRAP_CONTENT is not automatically supported so this class needs to
     * calculate the height manually.
     */
    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (MeasureSpec.EXACTLY == MeasureSpec.getMode(heightMeasureSpec)
                || !ChromeFeatureList.isEnabled(
                        ChromeFeatureList
                                .AUTOFILL_ANDROID_KEYBOARD_ACCESSORY_DYNAMIC_POSITIONING)) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            return;
        }

        int height = 0;
        if (getAdapter() instanceof AccessoryPagerAdapter) {
            AccessoryPagerAdapter adapter = (AccessoryPagerAdapter) getAdapter();
            View child = adapter.getView(getCurrentItem());
            if (child != null) {
                child.measure(
                        widthMeasureSpec, MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
                height = child.getMeasuredHeight();
            }
        }

        int maxHeight =
                getContext()
                        .getResources()
                        .getDimensionPixelSize(
                                R.dimen.keyboard_accessory_sheet_dynamic_positioning_max_height);
        height = Math.min(height, maxHeight);

        if (MeasureSpec.getMode(heightMeasureSpec) == MeasureSpec.AT_MOST) {
            int size = MeasureSpec.getSize(heightMeasureSpec);
            height = Math.min(height, size);
        }

        if (height != 0) {
            heightMeasureSpec = MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY);
        }
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }
}
