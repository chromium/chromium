// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.content.Context;
import android.content.res.Resources;
import android.util.AttributeSet;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.components.browser_ui.widget.tile.TileView;
import org.chromium.ui.base.DeviceFormFactor;

/** The most visited tiles layout. */
public class MostVisitedTilesLayout extends TilesLinearLayout {

    private final int mTileViewWidth;
    private Integer mInitialTileNum;
    private final boolean mIsTablet;
    private final int mIntervalPaddingsTablet;
    private final int mEdgePaddingsTablet;

    /** Constructor for inflating from XML. */
    public MostVisitedTilesLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mIsTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);

        Resources resources = getResources();
        mTileViewWidth = resources.getDimensionPixelOffset(R.dimen.tile_view_width);
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
            setEdgeMargins(mEdgePaddingsTablet);
            return;
        }

        int currentNum = getTileCount();
        int edgeMargin =
                (totalWidth
                                - mTileViewWidth * currentNum
                                - mIntervalPaddingsTablet * (currentNum - 1))
                        / 2;
        setEdgeMargins(edgeMargin);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (mInitialTileNum == null) {
            mInitialTileNum = getTileCount();
        }
        if (mIsTablet) {
            updateEdgeMarginTablet(widthMeasureSpec);
        }
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }
}
