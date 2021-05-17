// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.SuggestionsTileView;
import org.chromium.chrome.browser.suggestions.tile.Tile;
import org.chromium.components.browser_ui.widget.tile.TileView;

/**
 * The layout for the container of MV tiles on the Start surface.
 */
public class MvTilesLayout extends LinearLayout {
    private final Context mContext;
    private final int mTileViewPaddingPortrait;
    private final int mTileViewPaddingLandscape;
    private final int mTileViewPaddingEdgePortrait;

    public MvTilesLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;

        Resources resources = mContext.getResources();
        mTileViewPaddingEdgePortrait = resources.getDimensionPixelSize(
                org.chromium.chrome.R.dimen.tile_view_padding_edge_portrait);
        mTileViewPaddingPortrait = resources.getDimensionPixelSize(
                org.chromium.chrome.R.dimen.tile_view_padding_portrait);
        mTileViewPaddingLandscape = resources.getDimensionPixelSize(
                org.chromium.chrome.R.dimen.tile_view_padding_landscape);
    }

    @Override
    protected void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        updateTilesViewLayout(newConfig.orientation);
    }

    /**
     * Updates the paddings of all of the MV tiles according to the current screen orientation.
     * @param orientation The new orientation that paddings should be adjusted to.
     */
    void updateTilesViewLayout(int orientation) {
        int childCount = getChildCount();
        if (childCount == 0) return;

        // The margins of the left side of the first tile view and the right side of the last tile
        // view are larger than the tiles in the middle.
        int newEdgeMargin = orientation == Configuration.ORIENTATION_PORTRAIT
                ? mTileViewPaddingEdgePortrait
                : mTileViewPaddingLandscape;
        updateSingleTileViewLayout(
                (TileView) getChildAt(0), newEdgeMargin, /* isSetStartMargin = */ true);
        updateSingleTileViewLayout((TileView) getChildAt(childCount - 1), newEdgeMargin,
                /* isSetStartMargin = */ false);

        int newMargin = orientation == Configuration.ORIENTATION_PORTRAIT
                ? mTileViewPaddingPortrait
                : mTileViewPaddingLandscape;
        for (int i = 1; i < childCount; i++) {
            TileView tileView = (TileView) getChildAt(i);
            updateSingleTileViewLayout(tileView, newMargin, /* isSetStartMargin = */ true);
        }
    }

    SuggestionsTileView findTileView(Tile tile) {
        for (int i = 0; i < getChildCount(); i++) {
            View tileView = getChildAt(i);

            assert tileView instanceof SuggestionsTileView : "Tiles must be SuggestionsTileView";

            SuggestionsTileView suggestionsTileView = (SuggestionsTileView) tileView;

            if (tile.getUrl().equals(suggestionsTileView.getUrl())) {
                return (SuggestionsTileView) tileView;
            }
        }
        return null;
    }

    private void updateSingleTileViewLayout(
            TileView tileView, int newMargin, boolean isSetStartMargin) {
        MarginLayoutParams layoutParams = (MarginLayoutParams) tileView.getLayoutParams();

        if (isSetStartMargin && newMargin != layoutParams.getMarginStart()) {
            layoutParams.setMarginStart(newMargin);
            tileView.setLayoutParams(layoutParams);
        } else if (!isSetStartMargin && newMargin != layoutParams.getMarginEnd()) {
            layoutParams.setMarginEnd(newMargin);
            tileView.setLayoutParams(layoutParams);
        }
    }

    @VisibleForTesting
    public SuggestionsTileView getTileViewForTesting(SiteSuggestion suggestion) {
        int childCount = getChildCount();
        for (int i = 0; i < childCount; i++) {
            SuggestionsTileView tileView = (SuggestionsTileView) getChildAt(i);
            if (suggestion.equals(tileView.getData())) return tileView;
        }
        return null;
    }
}
