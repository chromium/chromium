// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import android.content.Context;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.widget.ScrollView;

import org.chromium.chrome.browser.readaloud.player.R;

/** An implementation of scroll view with enforced max height. */
public class MaxHeightScrollView extends ScrollView {
    private int mMaxHeight;

    public MaxHeightScrollView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init(attrs);
    }

    public MaxHeightScrollView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init(attrs);
    }

    public MaxHeightScrollView(
            Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
        init(attrs);
    }

    private void init(AttributeSet attrs) {
        TypedArray a =
                getContext().obtainStyledAttributes(attrs, R.styleable.MaxHeightScrollView, 0, 0);
        mMaxHeight = a.getDimensionPixelSize(R.styleable.MaxHeightScrollView_maxHeight, 0);
        a.recycle();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        heightMeasureSpec = MeasureSpec.makeMeasureSpec(mMaxHeight, MeasureSpec.AT_MOST);
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    /** Set new value of the maxHeight. The caller needs to call invalidate() afterwards. */
    public void setMaxHeight(int maxHeight) {
        mMaxHeight = maxHeight;
    }
}
