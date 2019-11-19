// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.content.Context;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.util.Pair;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.util.MathUtils;

/**
 * A layout that arranges tiles in a grid.
 */
public class TileGridLayout extends FrameLayout {
    private final int mVerticalSpacing;
    private final int mMinHorizontalSpacing;
    private final int mMaxHorizontalSpacing;
    private final int mMaxWidth;

    private int mMaxRows;
    private int mMaxColumns;

    /**
     * Constructor for inflating from XML.
     *
     * @param context The view context in which this item will be shown.
     * @param attrs The attributes of the XML tag that is inflating the view.
     */
    public TileGridLayout(Context context, AttributeSet attrs) {
        super(context, attrs);

        Resources res = getResources();
        mVerticalSpacing = res.getDimensionPixelOffset(R.dimen.tile_grid_layout_vertical_spacing);
        TypedArray styledAttrs = context.obtainStyledAttributes(attrs, R.styleable.TileGridLayout);
        mMinHorizontalSpacing = styledAttrs.getDimensionPixelOffset(
                R.styleable.TileGridLayout_minHorizontalSpacing,
                res.getDimensionPixelOffset(R.dimen.tile_grid_layout_min_horizontal_spacing));
        styledAttrs.recycle();
        mMaxHorizontalSpacing = Integer.MAX_VALUE;
        mMaxWidth = Integer.MAX_VALUE;
    }

    /**
     * Sets the maximum number of rows to display. Any items that don't fit will be hidden.
     */
    public void setMaxRows(int rows) {
        mMaxRows = rows;
    }

    /**
     * Sets the maximum number of columns to display. Any items that don't fit will be hidden.
     */
    public void setMaxColumns(int columns) {
        mMaxColumns = columns;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int totalWidth = Math.min(MeasureSpec.getSize(widthMeasureSpec), mMaxWidth);
        int childCount = getChildCount();
        if (childCount == 0) {
            setMeasuredDimension(totalWidth, resolveSize(0, heightMeasureSpec));
            return;
        }

        // Measure the children. We don't use the ViewGroup.measureChildren() method here because
        // it only measures visible children. In a situation where a child is invisible before
        // this measurement and we decide to show it after the measurement, it will not have its
        // dimensions and will not be displayed.
        for (int i = 0; i < childCount; i++) {
            measureChild(getChildAt(i), MeasureSpec.UNSPECIFIED, MeasureSpec.UNSPECIFIED);
        }

        // Determine the number of columns that will fit.
        int childHeight = getChildAt(0).getMeasuredHeight();
        int childWidth = getChildAt(0).getMeasuredWidth();
        int numColumns = MathUtils.clamp(
                (totalWidth + mMinHorizontalSpacing) / (childWidth + mMinHorizontalSpacing), 1,
                mMaxColumns);

        // Determine how much padding to use between and around the tiles.
        int gridWidthMinusColumns = Math.max(0, totalWidth - numColumns * childWidth);
        Pair<Integer, Integer> gridProperties =
                computeHorizontalDimensions(true, gridWidthMinusColumns, numColumns);
        int gridStart = gridProperties.first;
        int horizontalSpacing = gridProperties.second;

        // Limit the number of rows to mMaxRows.
        int visibleChildCount = Math.min(childCount, mMaxRows * numColumns);

        // Arrange the visible children in a grid.
        int numRows = (visibleChildCount + numColumns - 1) / numColumns;
        int paddingTop = getPaddingTop();
        boolean isRtl = getLayoutDirection() == LAYOUT_DIRECTION_RTL;

        for (int i = 0; i < visibleChildCount; i++) {
            View child = getChildAt(i);
            child.setVisibility(View.VISIBLE);
            int row = i / numColumns;
            int column = i % numColumns;
            int childTop = row * (childHeight + mVerticalSpacing);
            int childStart = gridStart + (column * (childWidth + horizontalSpacing));
            MarginLayoutParams layoutParams = (MarginLayoutParams) child.getLayoutParams();
            layoutParams.setMargins(isRtl ? 0 : childStart, childTop, isRtl ? childStart : 0, 0);
            child.setLayoutParams(layoutParams);
        }

        // Hide any extra children in case there are more than needed for the maximum number of
        // rows.
        for (int i = visibleChildCount; i < childCount; i++) {
            getChildAt(i).setVisibility(View.GONE);
        }

        int totalHeight = paddingTop + getPaddingBottom() + numRows * childHeight
                + (numRows - 1) * mVerticalSpacing;

        setMeasuredDimension(totalWidth, resolveSize(totalHeight, heightMeasureSpec));
    }

    /**
     * @param spreadTiles Whether to spread the tiles with the same space between and around them.
     * @param availableWidth The space available to spread between and around the tiles.
     * @param numColumns The number of columns to be organised.
     * @return The [gridStart, horizontalSpacing] pair of dimensions.
     */
    @VisibleForTesting
    Pair<Integer, Integer> computeHorizontalDimensions(
            boolean spreadTiles, int availableWidth, int numColumns) {
        int gridStart;
        float horizontalSpacing;
        if (spreadTiles) {
            // Identically sized spacers are added both between and around the tiles.
            int spacerCount = numColumns + 1;
            horizontalSpacing = (float) availableWidth / spacerCount;
            gridStart = Math.round(horizontalSpacing);
            if (horizontalSpacing < mMinHorizontalSpacing) {
                return computeHorizontalDimensions(false, availableWidth, numColumns);
            }
        } else {
            // Ensure column spacing isn't greater than mMaxHorizontalSpacing.
            long gridSidePadding = availableWidth - (long) mMaxHorizontalSpacing * (numColumns - 1);
            if (gridSidePadding > 0) {
                horizontalSpacing = mMaxHorizontalSpacing;
                gridStart = (int) (gridSidePadding / 2);
            } else {
                horizontalSpacing = (float) availableWidth / Math.max(1, numColumns - 1);
                gridStart = 0;
            }
        }

        assert horizontalSpacing >= mMinHorizontalSpacing;
        assert horizontalSpacing <= mMaxHorizontalSpacing;

        return Pair.create(gridStart, Math.round(horizontalSpacing));
    }

    @Nullable
    public SuggestionsTileView getTileView(SiteSuggestion suggestion) {
        int childCount = getChildCount();
        for (int i = 0; i < childCount; i++) {
            SuggestionsTileView tileView = (SuggestionsTileView) getChildAt(i);
            if (suggestion.equals(tileView.getData())) return tileView;
        }
        return null;
    }
}
