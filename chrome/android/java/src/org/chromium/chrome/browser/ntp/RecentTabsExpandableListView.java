// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ExpandableListView;

import org.chromium.ui.base.DeviceFormFactor;

/**
 * Customized ExpandableListView for the recent tabs page. This class handles tablet-specific
 * layout implementation.
 */
public class RecentTabsExpandableListView extends ExpandableListView {
    private static final int MAX_LIST_VIEW_WIDTH_DP = 550;

    private int mMaxListViewWidth;
    private int mSavedListPosition;
    private int mSavedListTop;

    /** Constructor for inflating from XML. */
    public RecentTabsExpandableListView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        float density = getResources().getDisplayMetrics().density;
        mMaxListViewWidth = Math.round(MAX_LIST_VIEW_WIDTH_DP * density);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext())) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            return;
        }

        // Increase padding if needed to ensure children are no wider than mMaxListViewWidth.
        int childWidth = MeasureSpec.getSize(widthMeasureSpec);
        int excessWidth = childWidth - mMaxListViewWidth;
        int horizontalPadding = 0;
        if (excessWidth > 0) {
            horizontalPadding += excessWidth / 2;
        }

        setPadding(horizontalPadding, 0, horizontalPadding, 0);

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        setSelectionFromTop(mSavedListPosition, mSavedListTop);
    }

    @Override
    protected void onDetachedFromWindow() {
        mSavedListPosition = getFirstVisiblePosition();
        View v = getChildAt(0);
        mSavedListTop = (v == null) ? 0 : v.getTop();
        super.onDetachedFromWindow();
    }
}
