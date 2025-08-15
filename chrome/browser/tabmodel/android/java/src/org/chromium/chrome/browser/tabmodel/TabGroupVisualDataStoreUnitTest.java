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

import java.util.HashSet;
import java.util.Set;

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

    @Test
    public void testDeleteTabGroupDataExcluding() {
        // 1. Store some data for three tab groups.
        Token tokenId1 = new Token(1L, 1L);
        Token tokenId2 = new Token(2L, 2L);
        Token tokenId3 = new Token(3L, 3L);
        String title1 = "Group 1";
        String title2 = "Group 2";
        String title3 = "Group 3";
        int color1 = 1;
        int color2 = 2;
        int color3 = 3;

        TabGroupVisualDataStore.storeTabGroupTitle(tokenId1, title1);
        TabGroupVisualDataStore.storeTabGroupColor(tokenId1, color1);
        TabGroupVisualDataStore.storeTabGroupCollapsed(tokenId1, true);

        TabGroupVisualDataStore.storeTabGroupTitle(tokenId2, title2);
        TabGroupVisualDataStore.storeTabGroupColor(tokenId2, color2);
        // Do not store collapsed state for token2 to test default values.

        TabGroupVisualDataStore.storeTabGroupTitle(tokenId3, title3);
        TabGroupVisualDataStore.storeTabGroupColor(tokenId3, color3);
        TabGroupVisualDataStore.storeTabGroupCollapsed(tokenId3, true);

        // 2. Call deleteTabGroupDataExcluding with a subset of token IDs.
        Set<String> tokensToKeep = new HashSet<>();
        tokensToKeep.add(tokenId1.toString());
        TabGroupVisualDataStore.deleteTabGroupDataExcluding(tokensToKeep);

        // 3. Verify that the data for the excluded token IDs is deleted.
        assertNull(TabGroupVisualDataStore.getTabGroupTitle(tokenId2));
        assertThat(
                TabGroupVisualDataStore.getTabGroupColor(tokenId2),
                equalTo(TabGroupColorUtils.INVALID_COLOR_ID));
        assertFalse(TabGroupVisualDataStore.getTabGroupCollapsed(tokenId2));
        assertNull(TabGroupVisualDataStore.getTabGroupTitle(tokenId3));
        assertThat(
                TabGroupVisualDataStore.getTabGroupColor(tokenId3),
                equalTo(TabGroupColorUtils.INVALID_COLOR_ID));
        assertFalse(TabGroupVisualDataStore.getTabGroupCollapsed(tokenId3));

        // 4. Verify that the data for the token IDs passed to the method is not deleted.
        assertThat(TabGroupVisualDataStore.getTabGroupTitle(tokenId1), equalTo(title1));
        assertThat(TabGroupVisualDataStore.getTabGroupColor(tokenId1), equalTo(color1));
        assertTrue(TabGroupVisualDataStore.getTabGroupCollapsed(tokenId1));
    }

    @Test
    public void testDeleteTabGroupDataExcluding_EmptySet() {
        // 1. Store some data for a tab group.
        Token tokenId1 = new Token(1L, 1L);
        String title1 = "Group 1";
        int color1 = 1;

        TabGroupVisualDataStore.storeTabGroupTitle(tokenId1, title1);
        TabGroupVisualDataStore.storeTabGroupColor(tokenId1, color1);
        TabGroupVisualDataStore.storeTabGroupCollapsed(tokenId1, true);

        // 2. Call deleteTabGroupDataExcluding with an empty set.
        TabGroupVisualDataStore.deleteTabGroupDataExcluding(new HashSet<>());

        // 3. Verify that all data is deleted.
        assertNull(TabGroupVisualDataStore.getTabGroupTitle(tokenId1));
        assertThat(
                TabGroupVisualDataStore.getTabGroupColor(tokenId1),
                equalTo(TabGroupColorUtils.INVALID_COLOR_ID));
        assertFalse(TabGroupVisualDataStore.getTabGroupCollapsed(tokenId1));
    }

    @Test
    public void testDeleteTabGroupDataExcluding_KeepAll() {
        // 1. Store some data for two tab groups.
        Token tokenId1 = new Token(1L, 1L);
        Token tokenId2 = new Token(2L, 2L);
        String title1 = "Group 1";
        String title2 = "Group 2";
        int color1 = 1;
        int color2 = 2;

        TabGroupVisualDataStore.storeTabGroupTitle(tokenId1, title1);
        TabGroupVisualDataStore.storeTabGroupColor(tokenId1, color1);
        TabGroupVisualDataStore.storeTabGroupCollapsed(tokenId1, true);

        TabGroupVisualDataStore.storeTabGroupTitle(tokenId2, title2);
        TabGroupVisualDataStore.storeTabGroupColor(tokenId2, color2);
        TabGroupVisualDataStore.storeTabGroupCollapsed(tokenId2, true);

        // 2. Call deleteTabGroupDataExcluding with all token IDs.
        Set<String> tokensToKeep = new HashSet<>();
        tokensToKeep.add(tokenId1.toString());
        tokensToKeep.add(tokenId2.toString());
        TabGroupVisualDataStore.deleteTabGroupDataExcluding(tokensToKeep);

        // 3. Verify that no data is deleted.
        assertThat(TabGroupVisualDataStore.getTabGroupTitle(tokenId1), equalTo(title1));
        assertThat(TabGroupVisualDataStore.getTabGroupColor(tokenId1), equalTo(color1));
        assertTrue(TabGroupVisualDataStore.getTabGroupCollapsed(tokenId1));
        assertThat(TabGroupVisualDataStore.getTabGroupTitle(tokenId2), equalTo(title2));
        assertThat(TabGroupVisualDataStore.getTabGroupColor(tokenId2), equalTo(color2));
        assertTrue(TabGroupVisualDataStore.getTabGroupCollapsed(tokenId2));
    }
}
