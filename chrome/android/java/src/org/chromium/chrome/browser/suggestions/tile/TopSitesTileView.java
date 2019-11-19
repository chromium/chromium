// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.content.Context;
import android.content.res.Resources;
import android.util.AttributeSet;

import org.chromium.chrome.browser.explore_sites.ExploreSitesBridge;
import org.chromium.chrome.browser.explore_sites.MostLikelyVariation;

/**
 * The view for a top sites tile. Displays the title of the site beneath a large icon.
 */
public class TopSitesTileView extends SuggestionsTileView {
    /**
     * Constructor for inflating from XML.
     */
    public TopSitesTileView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void setIconViewLayoutParams(Tile tile) {
        MarginLayoutParams params = (MarginLayoutParams) mIconView.getLayoutParams();
        Resources resources = getResources();
        if (tile.getType() == TileVisualType.ICON_REAL
                && ExploreSitesBridge.getIconVariation() == MostLikelyVariation.ICON_GROUPED) {
            // Grouped icons have extra large size.
            params.width = resources.getDimensionPixelOffset(
                    org.chromium.chrome.R.dimen.tile_view_icon_size);
            params.height = resources.getDimensionPixelSize(
                    org.chromium.chrome.R.dimen.tile_view_icon_size);
            params.topMargin = resources.getDimensionPixelSize(
                    org.chromium.chrome.R.dimen.tile_view_icon_background_margin_top_modern);
        } else {
            params.width = resources.getDimensionPixelSize(
                    org.chromium.chrome.R.dimen.tile_view_icon_size_modern);
            params.height = resources.getDimensionPixelSize(
                    org.chromium.chrome.R.dimen.tile_view_icon_size_modern);
            params.topMargin = resources.getDimensionPixelSize(
                    org.chromium.chrome.R.dimen.tile_view_icon_margin_top_modern);
        }
        mIconView.setLayoutParams(params);
    }
}
