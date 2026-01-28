// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import static org.chromium.build.NullUtil.assumeNonNull;

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
    private int mMaxItemCount;
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
        assumeNonNull(mGridLayoutManager);
        int availableWidth = MeasureSpec.getSize(widthSpec);

        int newWidthSpec = widthSpec;
        if (availableWidth > 0 && availableWidth != mLastRecyclerViewWidth) {
            mLastRecyclerViewWidth = availableWidth;
            int totalItemSpace = mItemWidth + mSpacing;
            int maxSpanCount =
                    Math.min(mMaxItemCount, Math.max(1, availableWidth / totalItemSpace));

            if (mSpanCount != maxSpanCount) {
                mSpanCount = maxSpanCount;
                mGridLayoutManager.setSpanCount(mSpanCount);
            }

            int contentWidth = mSpanCount * totalItemSpace;
            newWidthSpec = MeasureSpec.makeMeasureSpec(contentWidth, MeasureSpec.EXACTLY);
        }
        super.onMeasure(newWidthSpec, heightSpec);
    }

    /** Sets the item width for span calculation. */
    void setItemWidth(int itemWidth) {
        mItemWidth = itemWidth;
    }

    /** Sets the spacing for span calculation. */
    void setSpacing(int spacing) {
        mSpacing = spacing;
    }

    /**
     * Sets the maximum number of items allowed per row. This value acts as an upper limit (cap)
     * when calculating the span count based on the available width.
     */
    void setMaxItemCount(int maxItem) {
        mMaxItemCount = maxItem;
    }
}
