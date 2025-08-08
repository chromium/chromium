// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link TabGroupVisualDataStore}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupVisualDataStoreUnitTest {
    private static final int TAB_ID = 456;
    private static final String TAB_TITLE = "Tab";
    private static final int TAB_COLOR = 1;

    @Test
    public void testStoreAndGetTabGroupTitle() {
        TabGroupVisualDataStore.storeTabGroupTitle(TAB_ID, TAB_TITLE);
        assertThat(TabGroupVisualDataStore.getTabGroupTitle(TAB_ID), equalTo(TAB_TITLE));
    }

    @Test
    public void testStoreAndGetTabGroupTitle_Empty() {
        TabGroupVisualDataStore.storeTabGroupTitle(TAB_ID, "");
        assertNull(TabGroupVisualDataStore.getTabGroupTitle(TAB_ID));
    }

    @Test
    public void testStoreAndGetTabGroupTitle_Null() {
        TabGroupVisualDataStore.storeTabGroupTitle(TAB_ID, null);
        assertNull(TabGroupVisualDataStore.getTabGroupTitle(TAB_ID));
    }

    @Test
    public void testDeleteTabGroupTitle() {
        TabGroupVisualDataStore.storeTabGroupTitle(TAB_ID, TAB_TITLE);
        assertThat(TabGroupVisualDataStore.getTabGroupTitle(TAB_ID), equalTo(TAB_TITLE));

        TabGroupVisualDataStore.deleteTabGroupTitle(TAB_ID);
        assertNull(TabGroupVisualDataStore.getTabGroupTitle(TAB_ID));
    }

    @Test
    public void testStoreAndGetTabGroupColor() {
        TabGroupVisualDataStore.storeTabGroupColor(TAB_ID, TAB_COLOR);
        assertThat(TabGroupVisualDataStore.getTabGroupColor(TAB_ID), equalTo(TAB_COLOR));
    }

    @Test
    public void testDeleteTabGroupColor() {
        TabGroupVisualDataStore.storeTabGroupColor(TAB_ID, TAB_COLOR);
        assertThat(TabGroupVisualDataStore.getTabGroupColor(TAB_ID), equalTo(TAB_COLOR));

        TabGroupVisualDataStore.deleteTabGroupColor(TAB_ID);
        assertThat(
                TabGroupVisualDataStore.getTabGroupColor(TAB_ID),
                equalTo(TabGroupColorUtils.INVALID_COLOR_ID));
    }

    @Test
    public void testStoreAndGetTabGroupCollapsed() {
        TabGroupVisualDataStore.storeTabGroupCollapsed(TAB_ID, true);
        assertTrue(TabGroupVisualDataStore.getTabGroupCollapsed(TAB_ID));

        TabGroupVisualDataStore.storeTabGroupCollapsed(TAB_ID, false);
        assertFalse(TabGroupVisualDataStore.getTabGroupCollapsed(TAB_ID));
    }

    @Test
    public void testDeleteTabGroupCollapsed() {
        TabGroupVisualDataStore.storeTabGroupCollapsed(TAB_ID, true);
        assertTrue(TabGroupVisualDataStore.getTabGroupCollapsed(TAB_ID));

        TabGroupVisualDataStore.deleteTabGroupCollapsed(TAB_ID);
        assertFalse(TabGroupVisualDataStore.getTabGroupCollapsed(TAB_ID));
    }

    @Test
    public void testColorMigration() {
        assertFalse(TabGroupVisualDataStore.isColorInitialMigrationDone());
        TabGroupVisualDataStore.setColorInitialMigrationDone();
        assertTrue(TabGroupVisualDataStore.isColorInitialMigrationDone());
    }
}
