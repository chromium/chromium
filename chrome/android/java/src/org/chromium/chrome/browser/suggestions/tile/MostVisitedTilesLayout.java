// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.content.Context;
import android.content.res.Resources;
import android.util.AttributeSet;

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
    private float mIntervalMarginTabletDp;
    private int mEdgeMarginTabletPx;

    private @Nullable Integer mInitialContentWidthPx;

    /** Constructor for inflating from XML. */
    public MostVisitedTilesLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mIsTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);

        Resources resources = getResources();
        mTileViewWidthDp = resources.getDimension(R.dimen.tile_view_width);
        mIntervalMarginTabletDp = resources.getDimension(R.dimen.tile_view_padding_interval_tablet);
        mEdgeMarginTabletPx =
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

    /** Returns the total width of all content, including interval (but not edge) margins. */
    int computeContentWidthPx() {
        int tileCount = getTileCount();
        float contentWidthDp =
                mTileViewWidthDp * tileCount + mIntervalMarginTabletDp * (tileCount - 1);
        return ViewUtils.dpToPx(getContext(), contentWidthDp);
    }

    /**
     * Returns the edge margin. Cases are:
     *
     * <p>(1) Scrollable: If content width + 2 * {@code mEdgeMarginTabletPx} > total width, then
     * tiles can be scrolled. In this case, margin := {@code mEdgeMarginTabletPx}.
     *
     * <p>(2) Centered: Otherwise then tiles cannot be scrolled and may be centered. In this case,
     * margin := (total width - content width) / 2, to center content and act as strut.
     *
     * <p>Per design requirement (revisable), case (2) may still be treated as (1) by choosing
     * margin := {@code mEdgeMarginTabletPx}. This prevents centering, thus avoiding excessive tile
     * "jumps" during user edit. This special case is triggered if *initial* content width would
     * have led to (1) for the provided {@param totalWidthPx}.
     *
     * @param totalWidthPx The width of the Most Visited tiles container.
     */
    int computeEdgeMarginPx(int totalWidthPx) {
        if (mInitialContentWidthPx == null) {
            mInitialContentWidthPx = computeContentWidthPx();
        }
        // Detect and handle special case.
        if (mInitialContentWidthPx + 2 * mEdgeMarginTabletPx > totalWidthPx) {
            return mEdgeMarginTabletPx;
        }

        // Choose (1) or (2): The condition for (1) can be rearranged to
        //   LHS = {@code mEdgeMarginTabletPx} > (total width - content width) / 2 = RHS.
        // Noting that LHS is result when "true" and RHS for "false", we can simplify using max().
        int edgeMarginForCenteringPx = (totalWidthPx - computeContentWidthPx()) / 2;
        return Math.max(mEdgeMarginTabletPx, edgeMarginForCenteringPx);
    }

    /**
     * Adjusts the edge margin of tile elements on tablets.
     *
     * @param totalWidthPx The width of the Most Visited tiles container.
     */
    void updateEdgeMarginTablet(int totalWidthPx) {
        setEdgeMargins(computeEdgeMarginPx(totalWidthPx));
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (mIsTablet) {
            updateEdgeMarginTablet(widthMeasureSpec);
        }
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }
}
