// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.content.Context;
import android.content.res.Resources;
import android.util.AttributeSet;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.TitleUtil;
import org.chromium.chrome.browser.widget.tile.TileWithTextView;

/**
 * The view for a site suggestion tile. Displays the title of the site beneath a large icon. If a
 * large icon isn't available, displays a rounded rectangle with a single letter in its place.
 */
public class SuggestionsTileView extends TileWithTextView {
    /** The data currently associated to this tile. */
    private SiteSuggestion mData;

    /**
     * Constructor for inflating from XML.
     */
    public SuggestionsTileView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Initializes the view using the data held by {@code tile}. This should be called immediately
     * after inflation.
     * @param tile The tile that holds the data to populate this view.
     * @param titleLines The number of text lines to use for the tile title.
     */
    public void initialize(Tile tile, int titleLines) {
        super.initialize(TitleUtil.getTitleForDisplay(tile.getTitle(), tile.getUrl()),
                tile.isOfflineAvailable(), tile.getIcon(), titleLines);
        mData = tile.getData();
        setIconViewLayoutParams(tile);
    }

    /** Retrieves data associated with this view.  */
    public SiteSuggestion getData() {
        return mData;
    }

    /** Retrieves url associated with this view. */
    public String getUrl() {
        return mData.url;
    }

    /** Renders icon based on tile data.  */
    public void renderIcon(Tile tile) {
        setIconDrawable(tile.getIcon());
        setIconViewLayoutParams(tile);
    }

    public void renderOfflineBadge(Tile tile) {
        setOfflineBadgeVisibility(tile.isOfflineAvailable());
    }

    private void setIconViewLayoutParams(Tile tile) {
        MarginLayoutParams params = (MarginLayoutParams) mIconView.getLayoutParams();
        Resources resources = getResources();
        if (tile.getType() == TileVisualType.ICON_COLOR
                || tile.getType() == TileVisualType.ICON_DEFAULT) {
            params.width = resources.getDimensionPixelSize(R.dimen.tile_view_monogram_size_modern);
            params.height = resources.getDimensionPixelSize(R.dimen.tile_view_monogram_size_modern);
            params.topMargin =
                    resources.getDimensionPixelSize(R.dimen.tile_view_monogram_margin_top_modern);
        } else {
            params.width = resources.getDimensionPixelSize(R.dimen.tile_view_icon_size_modern);
            params.height = resources.getDimensionPixelSize(R.dimen.tile_view_icon_size_modern);
            params.topMargin =
                    resources.getDimensionPixelSize(R.dimen.tile_view_icon_margin_top_modern);
        }
        mIconView.setLayoutParams(params);
    }
}
