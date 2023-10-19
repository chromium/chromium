// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link org.chromium.chrome.browser.ShortcutHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ShortcutHelperTest {
    /** Test method for {@link ShortcutHelper#getScopeFromUrl.} */
    @Test
    public void testGetScopeFromUrl() {
        String url1 = "https://www.google.com";
        String url2 = "https://www.google.com/";
        String url3 = "https://www.google.com/maps.htm";
        String url4 = "https://www.google.com/maps/";
        String url5 = "https://www.google.com/index.html";
        String url6 = "https://www.google.com/index.html?q=maps";
        String url7 = "https://www.google.com/index.html#maps/";
        String url8 = "https://www.google.com/maps/au/index.html";
        String url9 = "https://www.google.com/maps/au/north";
        String url10 = "https://www.google.com/maps/au/north/";
        String url11 = "https://www.google.com/maps/au/index.html?q=maps#fragment/";
        String url12 = "http://www.google.com:8000/maps/au/index.html?q=maps#fragment/";
        String url13 = "https://www.google.com/maps/au/north/?q=maps#fragment";
        String url14 = "https://www.google.com/maps/au/north?q=maps#fragment";
        String url15 = "https://www.google.com/a=b/c";

        String url2_scope = "https://www.google.com/";
        String url4_scope = "https://www.google.com/maps/";
        String url8_scope = "https://www.google.com/maps/au/";
        String url10_scope = "https://www.google.com/maps/au/north/";
        String url12_scope = "http://www.google.com:8000/maps/au/";
        String url15_scope = "https://www.google.com/a=b/";

        assertEquals(url2_scope, ShortcutHelper.getScopeFromUrl(url1));
        assertEquals(url2_scope, ShortcutHelper.getScopeFromUrl(url2));
        assertEquals(url2_scope, ShortcutHelper.getScopeFromUrl(url3));
        assertEquals(url4_scope, ShortcutHelper.getScopeFromUrl(url4));
        assertEquals(url2_scope, ShortcutHelper.getScopeFromUrl(url5));
        assertEquals(url2_scope, ShortcutHelper.getScopeFromUrl(url6));
        assertEquals(url2_scope, ShortcutHelper.getScopeFromUrl(url7));
        assertEquals(url8_scope, ShortcutHelper.getScopeFromUrl(url8));
        assertEquals(url8_scope, ShortcutHelper.getScopeFromUrl(url9));
        assertEquals(url10_scope, ShortcutHelper.getScopeFromUrl(url10));
        assertEquals(url8_scope, ShortcutHelper.getScopeFromUrl(url11));
        assertEquals(url12_scope, ShortcutHelper.getScopeFromUrl(url12));
        assertEquals(url10_scope, ShortcutHelper.getScopeFromUrl(url13));
        assertEquals(url8_scope, ShortcutHelper.getScopeFromUrl(url14));
        assertEquals(url15_scope, ShortcutHelper.getScopeFromUrl(url15));
    }
}
