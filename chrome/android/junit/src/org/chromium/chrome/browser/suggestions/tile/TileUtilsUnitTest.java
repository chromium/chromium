// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.url.GURL;

/** Unit tests for {@link TileUtils} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TileUtilsUnitTest {
    @Test
    public void testFormatCustomTileName() {
        GURL testUrl = new GURL("https://example.com");
        assertEquals("Name", TileUtils.formatCustomTileName("Name", testUrl));
        assertEquals("https://example.com/", TileUtils.formatCustomTileName("", testUrl));

        // Relies on SuggestionsConfig.MAX_CUSTOM_TILES_NAME_LENGTH == 50.
        assertEquals(
                "A234567890B234567890C234567890D234567890E234567890",
                TileUtils.formatCustomTileName(
                        "A234567890B234567890C234567890D234567890E234567890F234567890", testUrl));
        assertEquals(
                "https://example.com/C234567890?234567890#234567890",
                TileUtils.formatCustomTileName(
                        "", new GURL("https://example.com/C234567890?234567890#234567890123456")));

        // Empty input -> empty output. The result will fail isValidCustomTileUrl(). We didn't
        // bother provide a default title for this, since if the URL is empty then a Tile would be
        // invalid anyway, and formatCustomTileName() would be unneeded.
        assertEquals("", TileUtils.formatCustomTileName("", new GURL("")));
    }

    @Test
    public void testIsValidCustomTileName() {
        assertTrue(TileUtils.isValidCustomTileName("Valid Name 123"));
        assertTrue(TileUtils.isValidCustomTileName("!@#$%^&*()"));
        assertTrue(TileUtils.isValidCustomTileName("          "));
        assertTrue(TileUtils.isValidCustomTileName("\u9ebb\u96c0"));
        assertFalse(TileUtils.isValidCustomTileName(""));

        // Rely on SuggestionsConfig.MAX_CUSTOM_TILES_NAME_LENGTH == 50.
        assertTrue(TileUtils.isValidCustomTileName("A".repeat(40)));
        assertFalse(TileUtils.isValidCustomTileName("A".repeat(60)));
    }

    @Test
    public void testIsValidCustomTileUrl() {
        assertTrue(TileUtils.isValidCustomTileUrl(new GURL("https://example.com/path?query#ref")));
        assertTrue(TileUtils.isValidCustomTileUrl(new GURL("https://m.example.com")));
        assertTrue(TileUtils.isValidCustomTileUrl(new GURL("https://example.com.")));
        assertTrue(TileUtils.isValidCustomTileUrl(new GURL("http://example.com:8000/")));
        assertTrue(TileUtils.isValidCustomTileUrl(new GURL("ftp://example.com/file.txt")));
        assertTrue(TileUtils.isValidCustomTileUrl(new GURL("file:///docs/info.pdf")));
        assertTrue(TileUtils.isValidCustomTileUrl(new GURL("chrome://bookmarks")));
        assertTrue(TileUtils.isValidCustomTileUrl(new GURL("chrome-native://recent-tabs")));
        assertFalse(TileUtils.isValidCustomTileUrl(new GURL("javascript://alert(3)")));
        assertFalse(TileUtils.isValidCustomTileUrl(new GURL("javascript:alert(3)")));
        assertFalse(TileUtils.isValidCustomTileUrl(new GURL("about:blank")));

        assertFalse(TileUtils.isValidCustomTileUrl(new GURL("")));
        assertFalse(TileUtils.isValidCustomTileUrl(new GURL("Not a valid URL")));
        assertFalse(TileUtils.isValidCustomTileUrl(new GURL("://example.com")));

        // Rely on SuggestionsConfig.MAX_CUSTOM_TILES_URL_LENGTH == 2083.
        assertTrue(
                TileUtils.isValidCustomTileUrl(
                        new GURL("https://example.com/" + "a".repeat(2000))));
        assertFalse(
                TileUtils.isValidCustomTileUrl(
                        new GURL("https://example.com/" + "a".repeat(3000))));
    }
}
