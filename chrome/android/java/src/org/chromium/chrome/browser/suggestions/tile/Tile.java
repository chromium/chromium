// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.graphics.drawable.Drawable;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.favicon.IconType;
import org.chromium.chrome.browser.suggestions.OfflinableSuggestion;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;

/**
 * Holds the details to populate a site suggestion tile.
 */
public class Tile implements OfflinableSuggestion {
    private final SiteSuggestion mSiteData;

    private final int mIndex;

    @TileVisualType
    private int mType = TileVisualType.NONE;

    @IconType
    private int mIconType = IconType.INVALID;

    @Nullable
    private Drawable mIcon;

    @Nullable
    private Long mOfflinePageOfflineId;

    /**
     * @param suggestion The site data we want to populate the tile with.
     * @param index The index of this tile in the list of tiles.
     */
    public Tile(SiteSuggestion suggestion, int index) {
        mSiteData = suggestion;
        mIndex = index;
    }

    public SiteSuggestion getData() {
        return mSiteData;
    }

    @Override
    public String getUrl() {
        return mSiteData.url;
    }

    @Override
    public void setOfflinePageOfflineId(@Nullable Long offlineId) {
        mOfflinePageOfflineId = offlineId;
    }

    @Nullable
    @Override
    public Long getOfflinePageOfflineId() {
        return mOfflinePageOfflineId;
    }

    @Override
    public boolean requiresExactOfflinePage() {
        return false;
    }

    /**
     * @return The title of this tile.
     */
    public String getTitle() {
        return mSiteData.title;
    }

    /**
     * @return Whether this tile is available offline.
     */
    public boolean isOfflineAvailable() {
        return getOfflinePageOfflineId() != null;
    }

    /**
     * @return The index of this tile in the list of tiles.
     */
    public int getIndex() {
        return mIndex;
    }

    /**
     * @return The source of this tile's title. Used for metrics tracking. Valid values are listed
     * in {@code TileTitleSource}.
     */
    @TileTitleSource
    public int getTitleSource() {
        return mSiteData.titleSource;
    }

    /**
     * @return The source of this tile. Used for metrics tracking. Valid values are listed in
     * {@code TileSource}.
     */
    @TileSource
    public int getSource() {
        return mSiteData.source;
    }

    /**
     * @return The visual type of this tile. Valid values are listed in {@link TileVisualType}.
     */
    @TileVisualType
    public int getType() {
        return mType;
    }

    /**
     * Sets the visual type of this tile. Valid values are listed in
     * {@link TileVisualType}.
     */
    public void setType(@TileVisualType int type) {
        mType = type;
    }

    /**
     * @return The icon type of this tile. Valid values are listed in {@link IconType}.
     */
    @IconType
    public int getIconType() {
        return mIconType;
    }

    /**
     * Sets the icon type of this tile. Valid values are listed in {@link IconType}.
     */
    public void setIconType(@IconType int iconType) {
        mIconType = iconType;
    }

    /**
     * @return The icon, may be null.
     */
    @Nullable
    public Drawable getIcon() {
        return mIcon;
    }

    /**
     * Updates the icon drawable.
     */
    public void setIcon(@Nullable Drawable icon) {
        mIcon = icon;
    }

    @TileSectionType
    public int getSectionType() {
        return mSiteData.sectionType;
    }
}
