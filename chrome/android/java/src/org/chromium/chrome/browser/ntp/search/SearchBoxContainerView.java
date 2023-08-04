// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import android.content.Context;
import android.graphics.drawable.RippleDrawable;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.widget.LinearLayout;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Provides the additional capabilities needed for the SearchBox container layout.
 */
public class SearchBoxContainerView extends LinearLayout {
    private final boolean mIsSurfacePolishEnabled;
    private final int mEndPadding;

    /** Constructor for inflating from XML. */
    public SearchBoxContainerView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mIsSurfacePolishEnabled = ChromeFeatureList.sSurfacePolish.isEnabled();
        mEndPadding = getResources().getDimensionPixelSize(R.dimen.fake_search_box_end_padding);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        if (mIsSurfacePolishEnabled) {
            int startPadding = getPaddingStart();
            setPaddingRelative(startPadding, 0, mEndPadding, 0);
        }
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        if (ev.getActionMasked() == MotionEvent.ACTION_DOWN) {
            if (getBackground() instanceof RippleDrawable) {
                ((RippleDrawable) getBackground()).setHotspot(ev.getX(), ev.getY());
            }
        }
        return super.onInterceptTouchEvent(ev);
    }
}
