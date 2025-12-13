// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.tab_groups.TabGroupColorId;

/** Unit tests for {@link TabWindowManagerUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabWindowManagerUtilsUnitTest {
    @Mock private TabWindowManager mTabWindowManager;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabGroupModelFilterProvider mTabGroupModelFilterProvider;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private Context mContext;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void testGetTabGroupTitleInAnyWindow() {
        Token tabGroupId = new Token(1, 1);
        when(mTabWindowManager.findWindowIdForTabGroup(tabGroupId)).thenReturn(1);
        when(mTabWindowManager.getTabModelSelectorById(1)).thenReturn(mTabModelSelector);
        when(mTabModelSelector.getTabGroupModelFilterProvider())
                .thenReturn(mTabGroupModelFilterProvider);
        when(mTabGroupModelFilterProvider.getTabGroupModelFilter(false))
                .thenReturn(mTabGroupModelFilter);
        when(mTabGroupModelFilter.tabGroupExists(tabGroupId)).thenReturn(true);
        when(mTabGroupModelFilter.getTabGroupTitle(tabGroupId)).thenReturn("Test Title");

        String title =
                TabWindowManagerUtils.getTabGroupTitleInAnyWindow(
                        mContext, mTabWindowManager, tabGroupId, false);
        assertEquals("Test Title", title);
    }

    @Test
    public void testGetTabGroupTitleInAnyWindow_invalidWindowId() {
        Token tabGroupId = new Token(1, 1);
        when(mTabWindowManager.findWindowIdForTabGroup(tabGroupId))
                .thenReturn(TabWindowManager.INVALID_WINDOW_ID);

        String title =
                TabWindowManagerUtils.getTabGroupTitleInAnyWindow(
                        mContext, mTabWindowManager, tabGroupId, false);
        assertNull(title);
    }

    @Test
    public void testGetTabGroupTitleInAnyWindow_nullTabModelSelector() {
        Token tabGroupId = new Token(1, 1);
        when(mTabWindowManager.findWindowIdForTabGroup(tabGroupId)).thenReturn(1);
        when(mTabWindowManager.getTabModelSelectorById(1)).thenReturn(null);

        String title =
                TabWindowManagerUtils.getTabGroupTitleInAnyWindow(
                        mContext, mTabWindowManager, tabGroupId, false);
        assertNull(title);
    }

    @Test
    public void testGetTabGroupColorInAnyWindow() {
        Token tabGroupId = new Token(1, 1);
        when(mTabWindowManager.findWindowIdForTabGroup(tabGroupId)).thenReturn(1);
        when(mTabWindowManager.getTabModelSelectorById(1)).thenReturn(mTabModelSelector);
        when(mTabModelSelector.getTabGroupModelFilterProvider())
                .thenReturn(mTabGroupModelFilterProvider);
        when(mTabGroupModelFilterProvider.getTabGroupModelFilter(false))
                .thenReturn(mTabGroupModelFilter);
        when(mTabGroupModelFilter.getTabGroupColorWithFallback(tabGroupId))
                .thenReturn(TabGroupColorId.BLUE);

        int color =
                TabWindowManagerUtils.getTabGroupColorInAnyWindow(
                        mTabWindowManager, tabGroupId, false);
        assertEquals(TabGroupColorId.BLUE, color);
    }

    @Test
    public void testGetTabGroupColorInAnyWindow_invalidWindowId() {
        Token tabGroupId = new Token(1, 1);
        when(mTabWindowManager.findWindowIdForTabGroup(tabGroupId))
                .thenReturn(TabWindowManager.INVALID_WINDOW_ID);

        int color =
                TabWindowManagerUtils.getTabGroupColorInAnyWindow(
                        mTabWindowManager, tabGroupId, false);
        assertEquals(TabGroupColorId.GREY, color);
    }

    @Test
    public void testGetTabGroupColorInAnyWindow_nullTabModelSelector() {
        Token tabGroupId = new Token(1, 1);
        when(mTabWindowManager.findWindowIdForTabGroup(tabGroupId)).thenReturn(1);
        when(mTabWindowManager.getTabModelSelectorById(1)).thenReturn(null);

        int color =
                TabWindowManagerUtils.getTabGroupColorInAnyWindow(
                        mTabWindowManager, tabGroupId, false);
        assertEquals(TabGroupColorId.GREY, color);
    }
}
