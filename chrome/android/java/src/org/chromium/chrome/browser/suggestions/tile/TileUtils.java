// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.search_engines.TemplateUrlService;

/** Utility class for {@link Tile}s related queries or operations. */
public class TileUtils {
    /**
     * Returns whether or not {@param tile} represents a Search query. This includes suggestions
     * from Repeatable Queries.
     */
    public static boolean isSearchTile(Profile profile, Tile tile) {
        assert profile != null;
        TemplateUrlService searchService = TemplateUrlServiceFactory.getForProfile(profile);
        return searchService != null
                && searchService.isSearchResultsPageFromDefaultSearchProvider(tile.getUrl());
    }
}
