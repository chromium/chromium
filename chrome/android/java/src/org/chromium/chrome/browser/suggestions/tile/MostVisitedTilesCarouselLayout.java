// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.content.Context;
import android.content.res.Configuration;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.components.browser_ui.widget.tile.TileView;

/**
 * The most visited tiles carousel layout.
 */
public class MostVisitedTilesCarouselLayout extends LinearLayout implements MostVisitedTilesLayout {
    // There's a minimum limit of 4.
    private static final int MIN_RESULTS = 4;

    private int mTileViewWidth;
    private int mTileViewMinIntervalPaddingTablet;
    private int mTileViewMaxIntervalPaddingTablet;
    private Integer mInitialTileNum;
    private Integer mIntervalPaddingsLandscapeTablet;
    private Integer mIntervalPaddingsPortraitTablet;
    private boolean mIsMultiColumnFeedOnTabletEnabled;

    /**
     * Constructor for inflating from XML.
     */
    public MostVisitedTilesCarouselLayout(Context context, AttributeSet attrs) {
        super(context, attrs);

        mTileViewWidth =
                getResources().getDimensionPixelOffset(org.chromium.chrome.R.dimen.tile_view_width);
        mTileViewMinIntervalPaddingTablet = getResources().getDimensionPixelOffset(
                org.chromium.chrome.R.dimen.tile_view_padding);
        mTileViewMaxIntervalPaddingTablet = getResources().getDimensionPixelOffset(
                org.chromium.chrome.R.dimen.tile_carousel_layout_max_interval_margin_tablet);
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
    @VisibleForTesting
    public SuggestionsTileView findTileViewForTesting(SiteSuggestion suggestion) {
        int childCount = getChildCount();
        for (int i = 0; i < childCount; i++) {
            SuggestionsTileView tileView = (SuggestionsTileView) getChildAt(i);
            if (suggestion.equals(tileView.getData())) return tileView;
        }
        return null;
    }

    /**
     * Adjust the padding intervals of the tile elements when they are displayed on the NTP
     * on the tablet.
     * @param isOrientationLandscape {@code true} if the orientation of the tablet is landscape.
     */
    void updateIntervalPaddingsTablet(boolean isOrientationLandscape) {
        if (isOrientationLandscape && mIntervalPaddingsLandscapeTablet != null) {
            setIntervalPaddings(mIntervalPaddingsLandscapeTablet);
        } else if (!isOrientationLandscape && mIntervalPaddingsPortraitTablet != null) {
            setIntervalPaddings(mIntervalPaddingsPortraitTablet);
        }
    }

    /**
     * Computes the distance between each MV tiles element based on certain parameters.
     * @param totalWidth The total width of the most recent tile,
     *                   excluding the half MV tiles element at the end.
     * @return The median value of the appropriate distances calculated as the distance between
     *         each MV tiles element.
     */
    @VisibleForTesting
    public int calculateTabletIntervalPadding(int totalWidth) {
        int tileWidthWithMinInternalPadding = mTileViewWidth - mTileViewMinIntervalPaddingTablet;
        int tileWidthWithMaxInternalPadding = mTileViewWidth + mTileViewMaxIntervalPaddingTablet;
        int maxElements = totalWidth / tileWidthWithMinInternalPadding;
        int minElements = (int) Math.ceil((double) totalWidth / tileWidthWithMaxInternalPadding);
        int preferElements = (minElements + Math.min(maxElements, mInitialTileNum - 1)) / 2;
        return (totalWidth - preferElements * mTileViewWidth) / preferElements;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (mIsMultiColumnFeedOnTabletEnabled) {
            if (mInitialTileNum == null) {
                mInitialTileNum = Math.min(Math.max(getChildCount(), MIN_RESULTS),
                        MostVisitedTilesMediator.MAX_RESULTS);
            }

            int currentOrientation = getResources().getConfiguration().orientation;
            if ((currentOrientation == Configuration.ORIENTATION_LANDSCAPE
                        && mIntervalPaddingsLandscapeTablet == null)
                    || (currentOrientation == Configuration.ORIENTATION_PORTRAIT
                            && mIntervalPaddingsPortraitTablet == null)) {
                int totalWidth = Math.min(MeasureSpec.getSize(widthMeasureSpec), Integer.MAX_VALUE)
                        - mTileViewWidth / 2;
                int tileViewIntervalPadding = calculateTabletIntervalPadding(totalWidth);
                if (currentOrientation == Configuration.ORIENTATION_LANDSCAPE) {
                    mIntervalPaddingsLandscapeTablet = tileViewIntervalPadding;
                } else {
                    mIntervalPaddingsPortraitTablet = tileViewIntervalPadding;
                }
                setIntervalPaddings(tileViewIntervalPadding);
            }
        }
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    @Override
    public void setIsMultiColumnFeedOnTabletEnabled(boolean isMultiColumnFeedOnTabletEnabled) {
        mIsMultiColumnFeedOnTabletEnabled = isMultiColumnFeedOnTabletEnabled;
    }

    @VisibleForTesting
    boolean getIsMultiColumnFeedOnTabletEnabledForTesting() {
        return mIsMultiColumnFeedOnTabletEnabled;
    }

    @VisibleForTesting
    public void setInitialTileNumForTesting(int initialTileNum) {
        mInitialTileNum = initialTileNum;
    }

    @VisibleForTesting
    public void setTileViewWidthForTesting(int tileViewWidth) {
        mTileViewWidth = tileViewWidth;
    }

    @VisibleForTesting
    public void setTileViewMinIntervalPaddingTabletForTesting(
            int tileViewMinIntervalPaddingTablet) {
        mTileViewMinIntervalPaddingTablet = tileViewMinIntervalPaddingTablet;
    }

    @VisibleForTesting
    public void setTileViewMaxIntervalPaddingTabletForTesting(
            int tileViewMaxIntervalPaddingTablet) {
        mTileViewMaxIntervalPaddingTablet = tileViewMaxIntervalPaddingTablet;
    }
}
