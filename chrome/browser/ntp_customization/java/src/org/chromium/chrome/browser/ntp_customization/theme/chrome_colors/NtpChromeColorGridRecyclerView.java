// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import android.content.Context;
import android.util.AttributeSet;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A RecyclerView that automatically adjusts its GridLayoutManager's span count based on its
 * measured width. This is more efficient than calculating in onLayoutChildren as it's done during
 * the measurement pass, preventing re-layout cycles.
 */
@NullMarked
public class NtpChromeColorGridRecyclerView extends RecyclerView {
    private @Nullable GridLayoutManager mGridLayoutManager;
    private int mSpanCount;
    private int mItemWidth;
    private int mSpacing;
    private int mLastRecyclerViewWidth;

    public NtpChromeColorGridRecyclerView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);

        setItemAnimator(null);
    }

    @Override
    public void setLayoutManager(@Nullable LayoutManager layout) {
        super.setLayoutManager(layout);
        if (layout instanceof GridLayoutManager) {
            mGridLayoutManager = (GridLayoutManager) layout;
        }
    }

    @Override
    protected void onMeasure(int widthSpec, int heightSpec) {
        assert mGridLayoutManager != null;

        super.onMeasure(widthSpec, heightSpec);
        int measuredWidth = getMeasuredWidth();
        if (measuredWidth > 0 && mLastRecyclerViewWidth != measuredWidth) {
            mLastRecyclerViewWidth = measuredWidth;
            int totalItemSpace = mItemWidth + mSpacing;
            assert totalItemSpace > 0;

            int maxSpanCount = Math.max(1, measuredWidth / totalItemSpace);
            if (mSpanCount != maxSpanCount) {
                mSpanCount = maxSpanCount;
                mGridLayoutManager.setSpanCount(mSpanCount);
            }
        }
    }

    /** Sets the item width for span calculation. */
    void setItemWidth(int itemWidth) {
        mItemWidth = itemWidth;
    }

    /** Sets the spacing for span calculation. */
    void setSpacing(int spacing) {
        mSpacing = spacing;
    }
}
