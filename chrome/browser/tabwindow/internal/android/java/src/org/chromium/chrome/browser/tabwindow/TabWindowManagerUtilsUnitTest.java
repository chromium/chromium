// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupId;

/** Unit tests for {@link TabWindowManagerUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabWindowManagerUtilsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TabWindowManager mTabWindowManager;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private Context mContext;

    @Test
    public void testGetTabGroupTitleInAnyWindow() {
        Token tabGroupId = new Token(1, 1);
        when(mTabWindowManager.findWindowIdForTabGroup(TabGroupId.fromRawToken(tabGroupId)))
                .thenReturn(1);
        when(mTabWindowManager.getTabModelSelectorById(1)).thenReturn(mTabModelSelector);
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModel.containsTabGroup(TabGroupId.fromOptionalToken(tabGroupId))).thenReturn(true);
        when(mTabModel.getTabGroupTitle(TabGroupId.fromRawToken(tabGroupId)))
                .thenReturn("Test Title");

        String title =
                TabWindowManagerUtils.getTabGroupTitleInAnyWindow(
                        mContext, mTabWindowManager, TabGroupId.fromRawToken(tabGroupId), false);
        assertEquals("Test Title", title);
    }

    @Test
    public void testGetTabGroupTitleInAnyWindow_invalidWindowId() {
        Token tabGroupId = new Token(1, 1);
        when(mTabWindowManager.findWindowIdForTabGroup(TabGroupId.fromRawToken(tabGroupId)))
                .thenReturn(TabWindowManager.INVALID_WINDOW_ID);

        String title =
                TabWindowManagerUtils.getTabGroupTitleInAnyWindow(
                        mContext, mTabWindowManager, TabGroupId.fromRawToken(tabGroupId), false);
        assertNull(title);
    }

    @Test
    public void testGetTabGroupTitleInAnyWindow_nullTabModelSelector() {
        Token tabGroupId = new Token(1, 1);
        when(mTabWindowManager.findWindowIdForTabGroup(TabGroupId.fromRawToken(tabGroupId)))
                .thenReturn(1);
        when(mTabWindowManager.getTabModelSelectorById(1)).thenReturn(null);

        String title =
                TabWindowManagerUtils.getTabGroupTitleInAnyWindow(
                        mContext, mTabWindowManager, TabGroupId.fromRawToken(tabGroupId), false);
        assertNull(title);
    }

    @Test
    public void testGetTabGroupColorInAnyWindow() {
        Token tabGroupId = new Token(1, 1);
        when(mTabWindowManager.findWindowIdForTabGroup(TabGroupId.fromRawToken(tabGroupId)))
                .thenReturn(1);
        when(mTabWindowManager.getTabModelSelectorById(1)).thenReturn(mTabModelSelector);
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModel.getTabGroupColorWithFallback(TabGroupId.fromRawToken(tabGroupId)))
                .thenReturn(TabGroupColorId.BLUE);

        int color =
                TabWindowManagerUtils.getTabGroupColorInAnyWindow(
                        mTabWindowManager, TabGroupId.fromRawToken(tabGroupId), false);
        assertEquals(TabGroupColorId.BLUE, color);
    }

    @Test
    public void testGetTabGroupColorInAnyWindow_invalidWindowId() {
        Token tabGroupId = new Token(1, 1);
        when(mTabWindowManager.findWindowIdForTabGroup(TabGroupId.fromRawToken(tabGroupId)))
                .thenReturn(TabWindowManager.INVALID_WINDOW_ID);

        int color =
                TabWindowManagerUtils.getTabGroupColorInAnyWindow(
                        mTabWindowManager, TabGroupId.fromRawToken(tabGroupId), false);
        assertEquals(TabGroupColorId.GREY, color);
    }

    @Test
    public void testGetTabGroupColorInAnyWindow_nullTabModelSelector() {
        Token tabGroupId = new Token(1, 1);
        when(mTabWindowManager.findWindowIdForTabGroup(TabGroupId.fromRawToken(tabGroupId)))
                .thenReturn(1);
        when(mTabWindowManager.getTabModelSelectorById(1)).thenReturn(null);

        int color =
                TabWindowManagerUtils.getTabGroupColorInAnyWindow(
                        mTabWindowManager, TabGroupId.fromRawToken(tabGroupId), false);
        assertEquals(TabGroupColorId.GREY, color);
    }
}
