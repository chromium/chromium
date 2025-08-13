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

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link TabGroupVisualDataStore}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupVisualDataStoreUnitTest {
    private static final int TAB_ID = 456;
    private static final Token TOKEN_ID = new Token(123L, 456L);
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

    @Test
    public void testStoreAndGetTabGroupTitle_Token() {
        TabGroupVisualDataStore.storeTabGroupTitle(TOKEN_ID, TAB_TITLE);
        assertThat(TabGroupVisualDataStore.getTabGroupTitle(TOKEN_ID), equalTo(TAB_TITLE));
    }

    @Test
    public void testStoreAndGetTabGroupTitle_Token_Empty() {
        TabGroupVisualDataStore.storeTabGroupTitle(TOKEN_ID, "");
        assertNull(TabGroupVisualDataStore.getTabGroupTitle(TOKEN_ID));
    }

    @Test
    public void testStoreAndGetTabGroupTitle_Token_Null() {
        TabGroupVisualDataStore.storeTabGroupTitle(TOKEN_ID, null);
        assertNull(TabGroupVisualDataStore.getTabGroupTitle(TOKEN_ID));
    }

    @Test
    public void testDeleteTabGroupTitle_Token() {
        TabGroupVisualDataStore.storeTabGroupTitle(TOKEN_ID, TAB_TITLE);
        assertThat(TabGroupVisualDataStore.getTabGroupTitle(TOKEN_ID), equalTo(TAB_TITLE));

        TabGroupVisualDataStore.deleteTabGroupTitle(TOKEN_ID);
        assertNull(TabGroupVisualDataStore.getTabGroupTitle(TOKEN_ID));
    }

    @Test
    public void testStoreAndGetTabGroupColor_Token() {
        TabGroupVisualDataStore.storeTabGroupColor(TOKEN_ID, TAB_COLOR);
        assertThat(TabGroupVisualDataStore.getTabGroupColor(TOKEN_ID), equalTo(TAB_COLOR));
    }

    @Test
    public void testDeleteTabGroupColor_Token() {
        TabGroupVisualDataStore.storeTabGroupColor(TOKEN_ID, TAB_COLOR);
        assertThat(TabGroupVisualDataStore.getTabGroupColor(TOKEN_ID), equalTo(TAB_COLOR));

        TabGroupVisualDataStore.deleteTabGroupColor(TOKEN_ID);
        assertThat(
                TabGroupVisualDataStore.getTabGroupColor(TOKEN_ID),
                equalTo(TabGroupColorUtils.INVALID_COLOR_ID));
    }

    @Test
    public void testStoreAndGetTabGroupCollapsed_Token() {
        TabGroupVisualDataStore.storeTabGroupCollapsed(TOKEN_ID, true);
        assertTrue(TabGroupVisualDataStore.getTabGroupCollapsed(TOKEN_ID));

        TabGroupVisualDataStore.storeTabGroupCollapsed(TOKEN_ID, false);
        assertFalse(TabGroupVisualDataStore.getTabGroupCollapsed(TOKEN_ID));
    }

    @Test
    public void testDeleteTabGroupCollapsed_Token() {
        TabGroupVisualDataStore.storeTabGroupCollapsed(TOKEN_ID, true);
        assertTrue(TabGroupVisualDataStore.getTabGroupCollapsed(TOKEN_ID));

        TabGroupVisualDataStore.deleteTabGroupCollapsed(TOKEN_ID);
        assertFalse(TabGroupVisualDataStore.getTabGroupCollapsed(TOKEN_ID));
    }

    @Test
    public void testDeleteAllVisualDataForGroup() {
        TabGroupVisualDataStore.storeTabGroupTitle(TOKEN_ID, TAB_TITLE);
        TabGroupVisualDataStore.storeTabGroupColor(TOKEN_ID, TAB_COLOR);
        TabGroupVisualDataStore.storeTabGroupCollapsed(TOKEN_ID, true);

        assertThat(TabGroupVisualDataStore.getTabGroupTitle(TOKEN_ID), equalTo(TAB_TITLE));
        assertThat(TabGroupVisualDataStore.getTabGroupColor(TOKEN_ID), equalTo(TAB_COLOR));
        assertTrue(TabGroupVisualDataStore.getTabGroupCollapsed(TOKEN_ID));

        TabGroupVisualDataStore.deleteAllVisualDataForGroup(TOKEN_ID);

        assertNull(TabGroupVisualDataStore.getTabGroupTitle(TOKEN_ID));
        assertThat(
                TabGroupVisualDataStore.getTabGroupColor(TOKEN_ID),
                equalTo(TabGroupColorUtils.INVALID_COLOR_ID));
        assertFalse(TabGroupVisualDataStore.getTabGroupCollapsed(TOKEN_ID));
    }

    @Test
    public void testMigrateToTokenKeyedStorage() {
        TabGroupVisualDataStore.storeTabGroupTitle(TAB_ID, TAB_TITLE);
        TabGroupVisualDataStore.storeTabGroupColor(TAB_ID, TAB_COLOR);
        TabGroupVisualDataStore.storeTabGroupCollapsed(TAB_ID, true);

        TabGroupVisualDataStore.migrateToTokenKeyedStorage(TAB_ID, TOKEN_ID);

        assertNull(TabGroupVisualDataStore.getTabGroupTitle(TAB_ID));
        assertThat(
                TabGroupVisualDataStore.getTabGroupColor(TAB_ID),
                equalTo(TabGroupColorUtils.INVALID_COLOR_ID));
        assertFalse(TabGroupVisualDataStore.getTabGroupCollapsed(TAB_ID));

        assertThat(TabGroupVisualDataStore.getTabGroupTitle(TOKEN_ID), equalTo(TAB_TITLE));
        assertThat(TabGroupVisualDataStore.getTabGroupColor(TOKEN_ID), equalTo(TAB_COLOR));
        assertTrue(TabGroupVisualDataStore.getTabGroupCollapsed(TOKEN_ID));
    }

    @Test
    public void testMigrateFromTokenKeyedStorage() {
        TabGroupVisualDataStore.storeTabGroupTitle(TOKEN_ID, TAB_TITLE);
        TabGroupVisualDataStore.storeTabGroupColor(TOKEN_ID, TAB_COLOR);
        TabGroupVisualDataStore.storeTabGroupCollapsed(TOKEN_ID, true);

        TabGroupVisualDataStore.migrateFromTokenKeyedStorage(TOKEN_ID, TAB_ID);

        assertNull(TabGroupVisualDataStore.getTabGroupTitle(TOKEN_ID));
        assertThat(
                TabGroupVisualDataStore.getTabGroupColor(TOKEN_ID),
                equalTo(TabGroupColorUtils.INVALID_COLOR_ID));
        assertFalse(TabGroupVisualDataStore.getTabGroupCollapsed(TOKEN_ID));

        assertThat(TabGroupVisualDataStore.getTabGroupTitle(TAB_ID), equalTo(TAB_TITLE));
        assertThat(TabGroupVisualDataStore.getTabGroupColor(TAB_ID), equalTo(TAB_COLOR));
        assertTrue(TabGroupVisualDataStore.getTabGroupCollapsed(TAB_ID));
    }
}
