// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import org.chromium.chrome.browser.suggestions.tile.TileSectionType;
import org.chromium.chrome.browser.suggestions.tile.TileSource;
import org.chromium.chrome.browser.suggestions.tile.TileTitleSource;

import java.util.Date;

/**
 * Data class that holds the site suggestion data provided by the tiles component.
 */
public class SiteSuggestion {
    /** Title of the suggested site. */
    public final String title;

    /** URL of the suggested site. */
    public final String url;

    /** The path to the icon image file for whitelisted tile, empty string otherwise. */
    public final String whitelistIconPath;

    /** The generated tile's title originated from this {@code TileTitleSource}. */
    @TileTitleSource
    public final int titleSource;

    /** the {@code TileSource} that generated the tile. */
    @TileSource
    public final int source;

    /**
     * The {@link org.chromium.chrome.browser.suggestions.tile.TileSectionType} the tile is
     * contained in.
     */
    @TileSectionType
    public final int sectionType;

    /** The instant in time representing when the tile was originally generated
        (produced by a ranking algorithm). */
    public final Date dataGenerationTime;

    public SiteSuggestion(String title, String url, String whitelistIconPath, int titleSource,
            int source, int sectionType, Date dataGenerationTime) {
        this.title = title;
        this.url = url;
        this.whitelistIconPath = whitelistIconPath;
        this.source = source;
        this.titleSource = titleSource;
        this.sectionType = sectionType;
        this.dataGenerationTime = (Date) dataGenerationTime.clone();
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;

        SiteSuggestion that = (SiteSuggestion) o;

        if (source != that.source) return false;
        if (titleSource != that.titleSource) return false;
        if (sectionType != that.sectionType) return false;
        if (!title.equals(that.title)) return false;
        if (!url.equals(that.url)) return false;
        return whitelistIconPath.equals(that.whitelistIconPath);
    }

    @Override
    public int hashCode() {
        int result = title.hashCode();
        result = 31 * result + url.hashCode();
        result = 31 * result + whitelistIconPath.hashCode();
        result = 31 * result + source;
        result = 31 * result + sectionType;
        result = 31 * result + titleSource;
        return result;
    }
}
