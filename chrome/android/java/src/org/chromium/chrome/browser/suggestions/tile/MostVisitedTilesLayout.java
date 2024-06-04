// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.components.browser_ui.widget.tile.TileView;
import org.chromium.ui.base.DeviceFormFactor;

/** The most visited tiles layout. */
public class MostVisitedTilesLayout extends LinearLayout {

    private int mTileViewWidth;
    private Integer mInitialTileNum;
    private boolean mIsTablet;
    private Integer mIntervalPaddingsTablet;
    private Integer mEdgePaddingsTablet;

    /** Constructor for inflating from XML. */
    public MostVisitedTilesLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mIsTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);

        mTileViewWidth =
                getResources().getDimensionPixelOffset(org.chromium.chrome.R.dimen.tile_view_width);
        mIntervalPaddingsTablet =
                getResources()
                        .getDimensionPixelSize(
                                org.chromium.chrome.R.dimen.tile_view_padding_interval_tablet);
        mEdgePaddingsTablet =
                getResources()
                        .getDimensionPixelSize(
                                org.chromium.chrome.R.dimen.tile_view_padding_edge_tablet);
    }

    void setIntervalPaddings(int padding) {
        int childCount = getChildCount();
        if (childCount == 0) return;

        for (int i = 1; i < childCount; i++) {
            TileView tileView = (TileView) getChildAt(i);
            updateSingleTileViewStartMargin(tileView, padding);
        }
    }

    void setEdgePaddings(int edgePadding) {
        int childCount = getChildCount();
        if (childCount == 0) return;
        updateSingleTileViewStartMargin((TileView) getChildAt(0), edgePadding);
        updateSingleTileViewEndMargin((TileView) getChildAt(childCount - 1), edgePadding);
    }

    void destroy() {
        for (int i = 0; i < getChildCount(); i++) {
            View tileView = getChildAt(i);
            tileView.setOnClickListener(null);
            tileView.setOnCreateContextMenuListener(null);
        }
        removeAllViews();
    }

    private void updateSingleTileViewStartMargin(TileView tileView, int newStartMargin) {
        MarginLayoutParams layoutParams = (MarginLayoutParams) tileView.getLayoutParams();
        if (newStartMargin != layoutParams.getMarginStart()) {
            layoutParams.setMarginStart(newStartMargin);
            tileView.setLayoutParams(layoutParams);
        }
    }

    private void updateSingleTileViewEndMargin(TileView tileView, int newEndMargin) {
        MarginLayoutParams layoutParams = (MarginLayoutParams) tileView.getLayoutParams();
        if (newEndMargin != layoutParams.getMarginEnd()) {
            layoutParams.setMarginEnd(newEndMargin);
            tileView.setLayoutParams(layoutParams);
        }
    }

    @Nullable
    public SuggestionsTileView findTileViewForTesting(SiteSuggestion suggestion) {
        int childCount = getChildCount();
        for (int i = 0; i < childCount; i++) {
            SuggestionsTileView tileView = (SuggestionsTileView) getChildAt(i);
            if (suggestion.equals(tileView.getData())) return tileView;
        }
        return null;
    }

    /**
     * Adjusts the edge margin of the tile elements when they are displayed in the center of the NTP
     * on the tablet.
     *
     * @param totalWidth The width of the mv tiles container.
     */
    void updateEdgeMarginTablet(int totalWidth) {
        boolean isFullFilled =
                totalWidth
                                - mTileViewWidth * mInitialTileNum
                                - mIntervalPaddingsTablet * (mInitialTileNum - 1)
                                - 2 * mEdgePaddingsTablet
                        >= 0;
        if (!isFullFilled) {
            // When splitting the window, this function is invoked with a different totalWidth value
            // during the process. Therefore, we must update the edge padding with the appropriate
            // value once the correct totalWidth is provided at the end of the split.
            setEdgePaddings(mEdgePaddingsTablet);
            return;
        }

        int currentNum = getChildCount();
        int edgeMargin =
                (totalWidth
                                - mTileViewWidth * currentNum
                                - mIntervalPaddingsTablet * (currentNum - 1))
                        / 2;
        setEdgePaddings(edgeMargin);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (mInitialTileNum == null) {
            mInitialTileNum = getChildCount();
        }
        if (mIsTablet) {
            updateEdgeMarginTablet(widthMeasureSpec);
        }
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }
}
