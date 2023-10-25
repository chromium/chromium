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

    private int mTileViewWidth;
    private int mTileViewMinIntervalPaddingTablet;
    private int mTileViewMaxIntervalPaddingTablet;
    private Integer mInitialTileNum;
    private Integer mIntervalPaddingsLandscapeTablet;
    private Integer mIntervalPaddingsPortraitTablet;
    private boolean mIsNtpAsHomeSurfaceOnTablet;
    private boolean mIsSurfacePolishEnabled;
    private Integer mIntervalPaddingsTabletForPolish;
    private Integer mEdgePaddingsTabletForPolish;

    /**
     * Constructor for inflating from XML.
     */
    public MostVisitedTilesCarouselLayout(Context context, AttributeSet attrs) {
        super(context, attrs);

        mTileViewWidth =
                getResources().getDimensionPixelOffset(org.chromium.chrome.R.dimen.tile_view_width);
        mTileViewMinIntervalPaddingTablet = getResources().getDimensionPixelOffset(
                org.chromium.chrome.R.dimen.tile_carousel_layout_min_interval_margin_tablet);
        mTileViewMaxIntervalPaddingTablet = getResources().getDimensionPixelOffset(
                org.chromium.chrome.R.dimen.tile_carousel_layout_max_interval_margin_tablet);
        mIntervalPaddingsTabletForPolish = getResources().getDimensionPixelSize(
                org.chromium.chrome.R.dimen.tile_view_padding_interval_tablet_polish);
        mEdgePaddingsTabletForPolish = getResources().getDimensionPixelSize(
                org.chromium.chrome.R.dimen.tile_view_padding_edge_tablet_polish);
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
     * Adjusts the edge margin of the tile elements when they are displayed in the center of the NTP
     * on the tablet.
     * @param totalWidth The width of the mv tiles container.
     */
    void updateEdgeMarginTablet(int totalWidth) {
        boolean isFullFilled = totalWidth - mTileViewWidth * mInitialTileNum
                        - mIntervalPaddingsTabletForPolish * (mInitialTileNum - 1)
                        - 2 * mEdgePaddingsTabletForPolish
                >= 0;
        if (!isFullFilled) {
            // When splitting the window, this function is invoked with a different totalWidth value
            // during the process. Therefore, we must update the edge padding with the appropriate
            // value once the correct totalWidth is provided at the end of the split.
            setEdgePaddings(mEdgePaddingsTabletForPolish);
            return;
        }

        int currentNum = getChildCount();
        int edgeMargin = (totalWidth - mTileViewWidth * currentNum
                                 - mIntervalPaddingsTabletForPolish * (currentNum - 1))
                / 2;
        setEdgePaddings(edgeMargin);
    }

    /**
     * Computes the distance between each MV tiles element based on certain parameters.
     * @param totalWidth The total width of the MV tiles.
     * @param isHalfMvt Whether there should be half MV tiles element at the end.
     * @return The median value of the appropriate distances calculated as the distance between
     *         each MV tiles element.
     */
    @VisibleForTesting
    public int calculateTabletIntervalPadding(int totalWidth, boolean isHalfMvt) {
        if (isHalfMvt) {
            int preferElements = (totalWidth - mTileViewWidth / 2)
                    / (mTileViewWidth + mTileViewMinIntervalPaddingTablet);
            return (totalWidth - mTileViewWidth / 2 - preferElements * mTileViewWidth)
                    / Math.max(1, preferElements);
        }
        return Math.min((totalWidth - mInitialTileNum * mTileViewWidth)
                        / Math.max(1, (mInitialTileNum - 1)),
                mTileViewMaxIntervalPaddingTablet);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (mInitialTileNum == null) {
            mInitialTileNum = getChildCount();
        }
        if (mIsNtpAsHomeSurfaceOnTablet && !mIsSurfacePolishEnabled) {
            int currentOrientation = getResources().getConfiguration().orientation;
            if ((currentOrientation == Configuration.ORIENTATION_LANDSCAPE
                        && mIntervalPaddingsLandscapeTablet == null)
                    || (currentOrientation == Configuration.ORIENTATION_PORTRAIT
                            && mIntervalPaddingsPortraitTablet == null)) {
                int totalWidth = Math.min(MeasureSpec.getSize(widthMeasureSpec), Integer.MAX_VALUE);
                boolean isHalfMvt = totalWidth < mInitialTileNum * mTileViewWidth
                                + (mInitialTileNum - 1) * mTileViewMinIntervalPaddingTablet;
                int tileViewIntervalPadding = calculateTabletIntervalPadding(totalWidth, isHalfMvt);
                if (currentOrientation == Configuration.ORIENTATION_LANDSCAPE) {
                    mIntervalPaddingsLandscapeTablet = tileViewIntervalPadding;
                } else {
                    mIntervalPaddingsPortraitTablet = tileViewIntervalPadding;
                }
                setIntervalPaddings(tileViewIntervalPadding);
            }
        }

        if (mIsNtpAsHomeSurfaceOnTablet && mIsSurfacePolishEnabled) {
            updateEdgeMarginTablet(widthMeasureSpec);
        }
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    @Override
    public void setIsNtpAsHomeSurfaceOnTablet(boolean isNtpAsHomeSurfaceOnTablet) {
        mIsNtpAsHomeSurfaceOnTablet = isNtpAsHomeSurfaceOnTablet;
    }

    public void setIsSurfacePolishEnabled(boolean isSurfacePolishEnabled) {
        mIsSurfacePolishEnabled = isSurfacePolishEnabled;
    }

    boolean getIsNtpAsHomeSurfaceOnTabletForTesting() {
        return mIsNtpAsHomeSurfaceOnTablet;
    }

    public void setInitialTileNumForTesting(int initialTileNum) {
        mInitialTileNum = initialTileNum;
    }

    public void setTileViewWidthForTesting(int tileViewWidth) {
        mTileViewWidth = tileViewWidth;
    }

    public void setTileViewMinIntervalPaddingTabletForTesting(
            int tileViewMinIntervalPaddingTablet) {
        mTileViewMinIntervalPaddingTablet = tileViewMinIntervalPaddingTablet;
    }

    public void setTileViewMaxIntervalPaddingTabletForTesting(
            int tileViewMaxIntervalPaddingTablet) {
        mTileViewMaxIntervalPaddingTablet = tileViewMaxIntervalPaddingTablet;
    }
}
