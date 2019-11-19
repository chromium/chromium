// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.view.View;

import androidx.annotation.CallSuper;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;

/**
 * Describes a portion of UI responsible for rendering a group of sites. It abstracts general tasks
 * related to initialising and updating this UI.
 */
public abstract class SiteSectionViewHolder extends NewTabPageViewHolder {
    protected TileGroup mTileGroup;
    protected TileRenderer mTileRenderer;

    /**
     * Constructs a {@link SiteSectionViewHolder} used to display tiles in both NTP and Chrome Home.
     *
     * @param itemView The {@link View} for this item
     */
    public SiteSectionViewHolder(View itemView) {
        super(itemView);
    }

    /** Initialise the view, letting it know the data it will have to display. */
    @CallSuper
    public void bindDataSource(TileGroup tileGroup, TileRenderer tileRenderer) {
        mTileGroup = tileGroup;
        mTileRenderer = tileRenderer;
    }

    /**
     * Sets a new icon on the child view with a matching site.
     * @param tile The tile that holds the data to populate the tile view.
     */
    public void updateIconView(Tile tile) {
        SuggestionsTileView tileView = findTileView(tile.getData());
        if (tileView != null) tileView.renderIcon(tile);
    }

    /**
     * Updates the visibility of the offline badge on the child view with a matching site.
     * @param tile The tile that holds the data to populate the tile view.
     */
    public void updateOfflineBadge(Tile tile) {
        SuggestionsTileView tileView = findTileView(tile.getData());
        if (tileView != null) tileView.renderOfflineBadge(tile);
    }

    /** Clears the current data and displays the current state of the model. */
    public abstract void refreshData();

    @Nullable
    public abstract SuggestionsTileView findTileView(SiteSuggestion data);
}
