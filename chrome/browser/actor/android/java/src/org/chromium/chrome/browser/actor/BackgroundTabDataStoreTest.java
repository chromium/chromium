// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;

/** Tests for {@link BackgroundTabDataStore}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BackgroundTabDataStoreTest {

    @Test
    public void testSaveAndRetrieve() {
        int tabId = 123;
        int placeholderId = 456;
        int originalIndex = 1;
        int tabWindowId = 2;
        boolean shouldRead = true;

        BackgroundTabDataStore.storePlaceholderTabId(tabId, placeholderId);
        BackgroundTabDataStore.storeOriginalTabIndex(tabId, originalIndex);
        BackgroundTabDataStore.storeTabWindowId(tabId, tabWindowId);
        BackgroundTabDataStore.storeShouldRead(tabId, shouldRead);

        assertEquals(placeholderId, BackgroundTabDataStore.getPlaceholderTabId(tabId));
        assertEquals(originalIndex, BackgroundTabDataStore.getOriginalTabIndex(tabId));
        assertEquals(tabWindowId, BackgroundTabDataStore.getTabWindowId(tabId));
        assertTrue(BackgroundTabDataStore.getShouldRead(tabId, false));
    }

    @Test
    public void testDelete() {
        int tabId = 456;
        BackgroundTabDataStore.storePlaceholderTabId(tabId, 999);
        BackgroundTabDataStore.storeOriginalTabIndex(tabId, 1);
        BackgroundTabDataStore.storeTabWindowId(tabId, 2);

        BackgroundTabDataStore.deletePlaceholderTabId(tabId);
        BackgroundTabDataStore.deleteOriginalTabIndex(tabId);
        BackgroundTabDataStore.deleteTabWindowId(tabId);

        assertEquals(Tab.INVALID_TAB_ID, BackgroundTabDataStore.getPlaceholderTabId(tabId));
        assertEquals(TabModel.INVALID_TAB_INDEX, BackgroundTabDataStore.getOriginalTabIndex(tabId));
        assertEquals(-1, BackgroundTabDataStore.getTabWindowId(tabId));
    }

    @Test
    public void testRemoveAll() {
        int tabId = 999;
        BackgroundTabDataStore.storePlaceholderTabId(tabId, 111);
        BackgroundTabDataStore.storeOriginalTabIndex(tabId, 2);
        BackgroundTabDataStore.storeTabWindowId(tabId, 3);
        BackgroundTabDataStore.storeShouldRead(tabId, false);

        BackgroundTabDataStore.removeBackgroundTabData(tabId);

        assertEquals(Tab.INVALID_TAB_ID, BackgroundTabDataStore.getPlaceholderTabId(tabId));
        assertEquals(TabModel.INVALID_TAB_INDEX, BackgroundTabDataStore.getOriginalTabIndex(tabId));
        assertEquals(-1, BackgroundTabDataStore.getTabWindowId(tabId));
        assertTrue(BackgroundTabDataStore.getShouldRead(tabId, true));
    }

    @Test
    public void testClearAll() {
        int tabId = 444;
        BackgroundTabDataStore.storePlaceholderTabId(tabId, 555);

        BackgroundTabDataStore.clearAllBackgroundTabData();

        assertEquals(Tab.INVALID_TAB_ID, BackgroundTabDataStore.getPlaceholderTabId(tabId));
    }
}
