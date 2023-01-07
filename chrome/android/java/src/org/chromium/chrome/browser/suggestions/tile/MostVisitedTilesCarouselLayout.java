// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.content.Context;
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
public class MostVisitedTilesCarouselLayout extends LinearLayout {
    /**
     * Constructor for inflating from XML.
     */
    public MostVisitedTilesCarouselLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
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
}
