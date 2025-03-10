// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.content.Context;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.components.browser_ui.widget.tile.TileView;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewUtils;

/** The most visited tiles layout. */
@NullMarked
public class MostVisitedTilesLayout extends TilesLinearLayout {
    private boolean mIsTablet;
    private float mTileViewWidthDp;

    // "Padding" here is informal; they're actually implemented by assigning margins.
    private float mIntervalPaddingsTabletDp;
    private int mEdgePaddingsTabletPx;

    private @Nullable Integer mInitialContentWidthPx;

    /** Constructor for inflating from XML. */
    public MostVisitedTilesLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mIsTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);

        Resources res = getResources();
        mTileViewWidthDp = res.getDimension(R.dimen.tile_view_width);
        mIntervalPaddingsTabletDp = res.getDimension(R.dimen.tile_view_padding_interval_tablet);
        mEdgePaddingsTabletPx = res.getDimensionPixelSize(R.dimen.tile_view_padding_edge_tablet);
    }

    void setIntervalPaddings(int paddingPx) {
        int tileCount = getTileCount();
        // Skip the first tile.
        for (int i = 1; i < tileCount; i++) {
            TileView tileView = getTileAt(i);
            updateSingleViewStartMargin(tileView, paddingPx);
        }
    }

    void setEdgePaddings(int edgePaddingPx) {
        int tileCount = getTileCount();
        if (tileCount == 0) return;
        updateSingleViewStartMargin(getTileAt(0), edgePaddingPx);
        updateSingleViewEndMargin(getTileAt(tileCount - 1), edgePaddingPx);
    }

    void destroy() {
        for (int i = 0; i < getTileCount(); i++) {
            TileView tileView = getTileAt(i);
            tileView.setOnClickListener(null);
            tileView.setOnCreateContextMenuListener(null);
        }
        removeAllViews();
    }

    private void updateSingleViewStartMargin(View view, int newStartMarginPx) {
        MarginLayoutParams layoutParams = (MarginLayoutParams) view.getLayoutParams();
        if (newStartMarginPx != layoutParams.getMarginStart()) {
            layoutParams.setMarginStart(newStartMarginPx);
            view.setLayoutParams(layoutParams);
        }
    }

    private void updateSingleViewEndMargin(View view, int newEndMarginPx) {
        MarginLayoutParams layoutParams = (MarginLayoutParams) view.getLayoutParams();
        if (newEndMarginPx != layoutParams.getMarginEnd()) {
            layoutParams.setMarginEnd(newEndMarginPx);
            view.setLayoutParams(layoutParams);
        }
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
     * @return Total width of all tiles, including interval (but not edge) paddings.
     */
    int computeContentWidthPx() {
        int tileCount = getTileCount();
        float contentWidthDp =
                mTileViewWidthDp * tileCount + mIntervalPaddingsTabletDp * (tileCount - 1);
        return ViewUtils.dpToPx(getContext(), contentWidthDp);
    }

    /**
     * Returns the edge padding. Cases are:
     *
     * <p>(1) Scrollable: If content width + 2 * {@code mEdgePaddingsTabletPx} > total width, then
     * tiles can be scrolled. In this case, padding := {@code mEdgePaddingsTabletPx}.
     *
     * <p>(2) Centered: Otherwise then tiles cannot be scrolled and may be centered. In this case,
     * padding := (total width - content width) / 2, to center content and act as strut.
     *
     * <p>Per design requirement (revisable), case (2) may still be treated as (1) by choosing
     * padding := {@code mEdgePaddingsTabletPx}. This prevents centering, thus avoiding excessive
     * tile "jumps" during user edit. This special case is triggered if *initial* content width
     * would have led to (1) for the provided {@param totalWidthPx}.
     *
     * @param totalWidthPx The width of the Most Visited tiles container.
     */
    int computeEdgePaddingPx(int totalWidthPx) {
        if (mInitialContentWidthPx == null) {
            mInitialContentWidthPx = computeContentWidthPx();
        }
        // Detect and handle special case.
        if (mInitialContentWidthPx + 2 * mEdgePaddingsTabletPx > totalWidthPx) {
            return mEdgePaddingsTabletPx;
        }

        // Choose (1) or (2): The condition for (1) can be rearranged to
        //   LHS = {@code mEdgePaddingsTabletPx} > (total width - content width) / 2 = RHS.
        // Noting that LHS is result when "true" and RHS for "false", we can simplify using max().
        int edgePaddingForCenteringPx = (totalWidthPx - computeContentWidthPx()) / 2;
        return Math.max(mEdgePaddingsTabletPx, edgePaddingForCenteringPx);
    }

    /**
     * Adjusts the edge padding of tile elements on tablets.
     *
     * @param totalWidthPx The width of the Most Visited tiles container.
     */
    void updateEdgeMarginTablet(int totalWidthPx) {
        setEdgePaddings(computeEdgePaddingPx(totalWidthPx));
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (mIsTablet) {
            updateEdgeMarginTablet(View.MeasureSpec.getSize(widthMeasureSpec));
        }
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }
}
