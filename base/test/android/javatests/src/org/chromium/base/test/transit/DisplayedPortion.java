// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.content.Context;
import android.graphics.Rect;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.View;
import android.view.WindowManager;

/**
 * Calculates the part of a View that is displayed on the screen, not occluded and not cropped.
 *
 * <p>Calculation identical to androidx.test.espresso.matcher.ViewMatchers for compatibility of
 * ViewInteractions. Can return > 100%.
 */
class DisplayedPortion {
    View mView;
    int mViewWidth;
    int mViewHeight;
    int mPercentage;
    Rect mVisibleRect = new Rect();
    Rect mScreenRect = new Rect();
    float mScaleX;
    float mScaleY;
    float mMaxAreaWidth;
    float mMaxAreaHeight;
    float mVisibleAreaWidth;
    float mVisibleAreaHeight;

    /** Calculate the current {@link DisplayedPortion} of a View. */
    static DisplayedPortion ofView(View view) {
        DisplayedPortion result = new DisplayedPortion();
        Rect visibleParts = new Rect();
        result.mView = view;

        boolean visibleAtAll = view.getGlobalVisibleRect(visibleParts);
        if (!visibleAtAll) {
            result.mPercentage = 0;
            return result;
        }

        Rect screen = getScreenWithoutStatusBarActionBar(view);

        result.mViewWidth = view.getWidth();
        result.mViewHeight = view.getHeight();
        result.mScaleX = view.getScaleX();
        result.mScaleY = view.getScaleY();
        // factor in the View's scaleX and scaleY properties.
        float viewHeight = Math.min(view.getHeight() * Math.abs(view.getScaleY()), screen.height());
        float viewWidth = Math.min(view.getWidth() * Math.abs(view.getScaleX()), screen.width());

        result.mMaxAreaWidth = viewWidth;
        result.mMaxAreaHeight = viewHeight;
        double maxArea = viewHeight * viewWidth;
        double visibleArea = visibleParts.height() * visibleParts.width();
        result.mVisibleAreaWidth = visibleParts.width();
        result.mVisibleAreaHeight = visibleParts.height();
        result.mPercentage = (int) ((visibleArea / maxArea) * 100);
        result.mVisibleRect = visibleParts;
        result.mScreenRect = screen;

        return result;
    }

    private static Rect getScreenWithoutStatusBarActionBar(View view) {
        DisplayMetrics m = new DisplayMetrics();
        ((WindowManager) view.getContext().getSystemService(Context.WINDOW_SERVICE))
                .getDefaultDisplay()
                .getMetrics(m);

        // Get status bar height
        int resourceId =
                view.getContext()
                        .getResources()
                        .getIdentifier("status_bar_height", "dimen", "android");
        int statusBarHeight =
                (resourceId > 0)
                        ? view.getContext().getResources().getDimensionPixelSize(resourceId)
                        : 0;

        // Get action bar height
        TypedValue tv = new TypedValue();
        int actionBarHeight =
                view.getContext()
                                .getTheme()
                                .resolveAttribute(android.R.attr.actionBarSize, tv, true)
                        ? TypedValue.complexToDimensionPixelSize(
                                tv.data, view.getContext().getResources().getDisplayMetrics())
                        : 0;

        return new Rect(0, statusBarHeight, m.widthPixels, m.heightPixels - actionBarHeight);
    }

    @Override
    public String toString() {
        int[] location = new int[2];
        mView.getLocationOnScreen(location);
        return String.format(
                "pct=%d, visibleR=%s, screenR=%s, visibleAreaWH=%dx%d, viewXY=%.2f-%.2f,"
                        + " viewLT=%d-%d, locationXY=%d-%d, viewWH=%dx%d maxAreaWH=%dx%d,"
                        + " scaleXY=%.2fx%.2f",
                mPercentage,
                mVisibleRect,
                mScreenRect,
                (int) mVisibleAreaWidth,
                (int) mVisibleAreaHeight,
                mView.getX(),
                mView.getY(),
                mView.getLeft(),
                mView.getTop(),
                location[0],
                location[1],
                mViewWidth,
                mViewHeight,
                (int) mMaxAreaWidth,
                (int) mMaxAreaHeight,
                mScaleX,
                mScaleY);
    }
}
