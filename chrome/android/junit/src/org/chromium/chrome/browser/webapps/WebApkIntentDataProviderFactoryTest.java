// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.os.Bundle;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.webapk.lib.common.WebApkMetaDataKeys;

import java.util.Map;

/** Tests {@link WebApkIntentDataProviderFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebApkIntentDataProviderFactoryTest {
    private static final String ICON_URL1 = "https://example.com/1.png";
    private static final String ICON_MURMUR2_HASH1 = "11";
    private static final String ICON_URL2 = "https://example.com/2.png";
    private static final String ICON_MURMUR2_HASH2 = "22";

    @Test
    public void testGetIconUrlAndIconMurmur2HashMap() {
        Bundle bundle = new Bundle();
        bundle.putString(
                WebApkMetaDataKeys.ICON_URLS_AND_ICON_MURMUR2_HASHES,
                ICON_URL1 + " " + ICON_MURMUR2_HASH1 + " " + ICON_URL2 + " " + ICON_MURMUR2_HASH2);

        Map<String, String> iconUrlToMurmur2HashMap =
                WebApkIntentDataProviderFactory.getIconUrlAndIconMurmur2HashMap(bundle);

        assertEquals(2, iconUrlToMurmur2HashMap.size());
        assertTrue(iconUrlToMurmur2HashMap.containsKey(ICON_URL1));
        assertEquals(ICON_MURMUR2_HASH1, iconUrlToMurmur2HashMap.get(ICON_URL1));
        assertTrue(iconUrlToMurmur2HashMap.containsKey(ICON_URL2));
        assertEquals(ICON_MURMUR2_HASH2, iconUrlToMurmur2HashMap.get(ICON_URL2));
    }

    /**
     * Test that getIconUrlAndIconMurmur2HashMap return hashmap with 1 item when has duplicate
     * key entries.
     */
    @Test
    public void testGetIconUrlAndIconMurmur2HashMap_duplicateUrl() {
        Bundle bundle = new Bundle();
        bundle.putString(
                WebApkMetaDataKeys.ICON_URLS_AND_ICON_MURMUR2_HASHES,
                ICON_URL1 + " " + ICON_MURMUR2_HASH1 + " " + ICON_URL1 + " " + ICON_MURMUR2_HASH2);

        Map<String, String> iconUrlToMurmur2HashMap =
                WebApkIntentDataProviderFactory.getIconUrlAndIconMurmur2HashMap(bundle);

        assertEquals(1, iconUrlToMurmur2HashMap.size());
        assertTrue(iconUrlToMurmur2HashMap.containsKey(ICON_URL1));
        assertEquals(ICON_MURMUR2_HASH2, iconUrlToMurmur2HashMap.get(ICON_URL1));
    }

    /** Test when contains empty urls, getIconUrlAndIconMurmur2HashMap still returns correct result. */
    @Test
    public void testGetIconUrlAndIconMurmur2HashMap_emptyUrl() {
        Bundle bundle = new Bundle();
        bundle.putString(
                WebApkMetaDataKeys.ICON_URLS_AND_ICON_MURMUR2_HASHES,
                " 0  0 " + ICON_URL1 + " " + ICON_MURMUR2_HASH1);

        Map<String, String> iconUrlToMurmur2HashMap =
                WebApkIntentDataProviderFactory.getIconUrlAndIconMurmur2HashMap(bundle);

        assertEquals(1, iconUrlToMurmur2HashMap.size());
        assertTrue(iconUrlToMurmur2HashMap.containsKey(ICON_URL1));
        assertEquals(ICON_MURMUR2_HASH1, iconUrlToMurmur2HashMap.get(ICON_URL1));
    }

    /**
     * Test getIconUrlAndIconMurmur2HashMap returns empty map when provided hashes string not in
     * pairs.
     */
    @Test
    public void testGetIconUrlAndIconMurmur2HashMap_notPaired() {
        Bundle bundle = new Bundle();
        bundle.putString(
                WebApkMetaDataKeys.ICON_URLS_AND_ICON_MURMUR2_HASHES,
                ICON_URL1 + " " + ICON_MURMUR2_HASH1 + " 0");

        Map<String, String> iconUrlToMurmur2HashMap =
                WebApkIntentDataProviderFactory.getIconUrlAndIconMurmur2HashMap(bundle);

        assertTrue(iconUrlToMurmur2HashMap.isEmpty());
    }
}
