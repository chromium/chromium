// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.text.TextUtils;
import android.view.KeyEvent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Utility class for {@link Tile}s related queries or operations. */
@NullMarked
public class TileUtils {
    private static final Set<String> CUSTOM_TILE_SCHEMES =
            new HashSet<>(
                    Arrays.asList(
                            UrlConstants.CHROME_SCHEME,
                            UrlConstants.CHROME_NATIVE_SCHEME,
                            UrlConstants.FTP_SCHEME,
                            UrlConstants.HTTP_SCHEME,
                            UrlConstants.HTTPS_SCHEME,
                            UrlConstants.FILE_SCHEME));

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

    /**
     * @return Custom tile name, truncated (if needed) from {@param name} if non-empty, or {@param
     *     url} otherwise. The result can still fail {@link isValidCustomTileName()}, e.g., when
     *     both {@param name} and {@param url} are empty then the result is also empty.
     */
    public static String formatCustomTileName(String name, GURL url) {
        String nameToUse = TextUtils.isEmpty(name) ? url.getSpec() : name;
        return nameToUse.length() <= SuggestionsConfig.MAX_CUSTOM_TILES_NAME_LENGTH
                ? nameToUse
                : nameToUse.substring(0, SuggestionsConfig.MAX_CUSTOM_TILES_NAME_LENGTH);
    }

    /**
     * @return Whether {@param url} is a valid name for Custom Tiles.
     */
    public static boolean isValidCustomTileName(String name) {
        return !name.isEmpty() && name.length() <= SuggestionsConfig.MAX_CUSTOM_TILES_NAME_LENGTH;
    }

    /**
     * @return Whether {@param url} is a valid URL for usage Custom Tiles.
     */
    public static boolean isValidCustomTileUrl(@Nullable GURL url) {
        return !GURL.isEmptyOrInvalid(url)
                && url.getSpec().length() <= SuggestionsConfig.MAX_CUSTOM_TILES_URL_LENGTH
                && CUSTOM_TILE_SCHEMES.contains(url.getScheme());
    }

    /**
     * @return Whether number of Custom Tiles in {@param tiles} is below maximum.
     */
    public static boolean customTileCountIsUnderLimit(List<Tile> tiles) {
        int numCustomTiles = 0;
        for (Tile tile : tiles) {
            if (tile.getData().source == TileSource.CUSTOM_LINKS) {
                ++numCustomTiles;
            }
        }
        return numCustomTiles < SuggestionsConfig.MAX_NUM_CUSTOM_LINKS;
    }

    /**
     * Returns whether {@param keyCode} and {@param event} from an onKey() event is the combo for
     * reorder a Custom Tile by swapping it with a neighbor. Implementation: Ctrl+Shift+{Page Up,
     * Page Down} swaps with the {previous, next} Custom Tile.
     */
    public static boolean isCustomTileSwapKeyCombo(int keyCode, KeyEvent event) {
        return (keyCode == KeyEvent.KEYCODE_PAGE_UP || keyCode == KeyEvent.KEYCODE_PAGE_DOWN)
                && event.isShiftPressed()
                && event.isCtrlPressed();
    }
}
