// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import org.chromium.chrome.browser.suggestions.tile.TileSectionType;
import org.chromium.chrome.browser.suggestions.tile.TileSource;
import org.chromium.chrome.browser.suggestions.tile.TileTitleSource;
import org.chromium.url.GURL;

/** Data class that holds the site suggestion data provided by the tiles component. */
public class SiteSuggestion {
    public static final int INVALID_FAVICON_ID = -1;

    /** Title of the suggested site. */
    public final String title;

    /** URL of the suggested site. */
    public final GURL url;

    /** The generated tile's title originated from this {@code TileTitleSource}. */
    @TileTitleSource public final int titleSource;

    /** the {@code TileSource} that generated the tile. */
    @TileSource public final int source;

    /**
     * The {@link org.chromium.chrome.browser.suggestions.tile.TileSectionType} the tile is
     * contained in.
     */
    @TileSectionType public final int sectionType;

    public SiteSuggestion(String title, GURL url, int titleSource, int source, int sectionType) {
        this.title = title;
        this.url = url;
        this.source = source;
        this.titleSource = titleSource;
        this.sectionType = sectionType;
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
        return url.equals(that.url);
    }

    @Override
    public int hashCode() {
        int result = title.hashCode();
        result = 31 * result + url.hashCode();
        result = 31 * result + source;
        result = 31 * result + sectionType;
        result = 31 * result + titleSource;
        return result;
    }
}
