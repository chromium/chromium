// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.content.Context;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.widget.HorizontalScrollView;

import androidx.annotation.Nullable;
import androidx.annotation.Px;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.components.browser_ui.widget.tile.TileView;
import org.chromium.ui.base.DeviceFormFactor;

/** The most visited tiles layout. */
public class MostVisitedTilesLayout extends TilesLinearLayout {

    private final boolean mIsTablet;
    private final @Px int mTileViewWidthPx;
    private Integer mInitialTileCount;
    private Integer mInitialChildCount;
    private final int mIntervalPaddingsTablet;
    private final int mEdgePaddingsTablet;
    private @Nullable Integer mTileToMoveInViewIdx;

    /** Constructor for inflating from XML. */
    public MostVisitedTilesLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mIsTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);

        Resources resources = getResources();
        mTileViewWidthPx = resources.getDimensionPixelOffset(R.dimen.tile_view_width);
        mIntervalPaddingsTablet =
                resources.getDimensionPixelSize(R.dimen.tile_view_padding_interval_tablet);
        mEdgePaddingsTablet =
                resources.getDimensionPixelSize(R.dimen.tile_view_padding_edge_tablet);
    }

    void destroy() {
        for (int i = 0; i < getTileCount(); i++) {
            TileView tileView = getTileAt(i);
            tileView.setOnClickListener(null);
            tileView.setOnCreateContextMenuListener(null);
        }
        removeAllViews();
    }

    @Nullable
    public SuggestionsTileView findTileViewForTesting(SiteSuggestion suggestion) {
        int tileCount = getTileCount();
        for (int i = 0; i < tileCount; i++) {
            SuggestionsTileView tileView = (SuggestionsTileView) getTileAt(i);
            if (suggestion.equals(tileView.getData())) return tileView;
        }
        return null;
    }

    public SiteSuggestion getTileViewData(TileView tileView) {
        return ((SuggestionsTileView) tileView).getData();
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
                                - mTileViewWidthPx * mInitialTileCount
                                - getNonTileViewsTotalWidthPx()
                                - mIntervalPaddingsTablet * (mInitialChildCount - 1)
                                - 2 * mEdgePaddingsTablet
                        >= 0;
        if (!isFullFilled) {
            // When splitting the window, this function is invoked with a different totalWidth value
            // during the process. Therefore, we must update the edge padding with the appropriate
            // value once the correct totalWidth is provided at the end of the split.
            setEdgeMargins(mEdgePaddingsTablet);
            return;
        }

        int tileCount = getTileCount();
        int childCount = getChildCount();
        int edgeMargin =
                (totalWidth
                                - mTileViewWidthPx * tileCount
                                - getNonTileViewsTotalWidthPx()
                                - mIntervalPaddingsTablet * (childCount - 1))
                        / 2;
        setEdgeMargins(edgeMargin);
    }

    /**
     * Tags the {@link TileView} at {@param tileIdx} so that on next Layout, minimal scroll is
     * performed to ensure it's in-view.
     */
    void ensureTileIsInViewOnNextLayout(int tileIdx) {
        // If a value exists, simply overwrite it since the old value is likely new irrelevant.
        mTileToMoveInViewIdx = tileIdx;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (mInitialTileCount == null) {
            mInitialTileCount = getTileCount();
        }
        if (mInitialChildCount == null) {
            mInitialChildCount = getChildCount();
        }
        if (mIsTablet) {
            updateEdgeMarginTablet(widthMeasureSpec);
        }
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    @Override
    public void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        if (mTileToMoveInViewIdx != null) {
            Integer scrollXPx = getScrollXToMakeTileVisible(mTileToMoveInViewIdx);
            if (scrollXPx != null) {
                HorizontalScrollView parent = (HorizontalScrollView) getParent();
                parent.smoothScrollTo(scrollXPx.intValue(), 0);
            }
            mTileToMoveInViewIdx = null;
        }
    }

    private @Nullable Integer getScrollXToMakeTileVisible(int tileIdx) {
        if (tileIdx >= getTileCount()) {
            return null;
        }
        HorizontalScrollView parent = (HorizontalScrollView) getParent();
        @Px float tileXPx = getTileAt(tileIdx).getX();
        @Px int scrollXPx = parent.getScrollX();
        // If scroll position is too high so that the tile is out-of-view / truncated, scroll left
        // so that the tile appears on the left edge (RTL doesn't matter).
        @Px int scrollXHiPx = (int) tileXPx;
        if (scrollXPx > scrollXHiPx) {
            return scrollXHiPx;
        }
        // If scroll position is too low so that the tile is out-of-view / truncated, scroll right
        // so that the tile appears on the right edge (RTL doesn't matter).
        @Px int scrollXLoPx = (int) (tileXPx + mTileViewWidthPx - parent.getWidth());
        if (scrollXPx < scrollXLoPx) {
            return scrollXLoPx;
        }
        // Entire tile is in-view; no scroll is needed.
        return null;
    }
}
