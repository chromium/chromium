// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils.UNSET_TAB_GROUP_TITLE;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;

import java.util.ArrayList;
import java.util.List;

/** Tests for {@link TabGroupTitleUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupTitleUtilsUnitTest {
    private static final Token TAB_GROUP_ID = new Token(34789L, 3784L);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModel mTabModel;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Tab mTab3;

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
    }

    @Test
    public void testDefaultTitle() {
        int relatedTabCount = 5;

        String expectedTitle =
                mContext.getResources()
                        .getQuantityString(
                                R.plurals.bottom_tab_grid_title_placeholder,
                                relatedTabCount,
                                relatedTabCount);
        assertEquals(expectedTitle, TabGroupTitleUtils.getDefaultTitle(mContext, relatedTabCount));
    }

    @Test
    public void testIsDefaultTitle() {
        int fourTabsCount = 4;
        String fourTabsTitle = TabGroupTitleUtils.getDefaultTitle(mContext, fourTabsCount);
        assertTrue(TabGroupTitleUtils.isDefaultTitle(mContext, fourTabsTitle, fourTabsCount));
        assertFalse(TabGroupTitleUtils.isDefaultTitle(mContext, fourTabsTitle, 3));
        assertFalse(TabGroupTitleUtils.isDefaultTitle(mContext, "Foo", fourTabsCount));
    }

    @Test
    public void testGetDisplayableTitle_Explicit() {
        String title = "t1";
        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTabModel.getTabGroupTitle(TAB_GROUP_ID)).thenReturn(title);
        assertEquals(
                title, TabGroupTitleUtils.getDisplayableTitle(mContext, mTabModel, TAB_GROUP_ID));
    }

    @Test
    public void testGetDisplayableTitle_Fallback() {
        int tabCount = 4567;
        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTabModel.getTabGroupTitle(TAB_GROUP_ID)).thenReturn("");

        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < tabCount; i++) {
            Tab tab = mock(Tab.class);
            when(tab.isClosing()).thenReturn(false);
            tabs.add(tab);
        }
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);

        String title = TabGroupTitleUtils.getDisplayableTitle(mContext, mTabModel, TAB_GROUP_ID);
        assertTrue(title.contains(String.valueOf(tabCount)));
    }

    @Test
    public void testGetDisplayableTitle_FallbackNoClosingTabs() {
        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTabModel.getTabGroupTitle(TAB_GROUP_ID)).thenReturn(UNSET_TAB_GROUP_TITLE);
        List<Tab> tabs = new ArrayList<>();
        tabs.add(mTab1);
        tabs.add(mTab2);
        when(mTab1.isClosing()).thenReturn(false);
        when(mTab2.isClosing()).thenReturn(false);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);

        String title = TabGroupTitleUtils.getDisplayableTitle(mContext, mTabModel, TAB_GROUP_ID);

        assertTrue(title.contains("2"));
    }

    @Test
    public void testGetDisplayableTitle_FallbackSomeClosingTabs() {
        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTabModel.getTabGroupTitle(TAB_GROUP_ID)).thenReturn(UNSET_TAB_GROUP_TITLE);
        List<Tab> tabs = new ArrayList<>();
        tabs.add(mTab1);
        tabs.add(mTab2);
        tabs.add(mTab3);
        when(mTab1.isClosing()).thenReturn(false);
        when(mTab2.isClosing()).thenReturn(true);
        when(mTab3.isClosing()).thenReturn(false);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);

        String title = TabGroupTitleUtils.getDisplayableTitle(mContext, mTabModel, TAB_GROUP_ID);

        assertTrue(title.contains("2"));
        assertFalse(title.contains("3"));
    }

    @Test
    public void testGetDisplayableTitle_FallbackAllClosingTabs() {
        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTabModel.getTabGroupTitle(TAB_GROUP_ID)).thenReturn(UNSET_TAB_GROUP_TITLE);
        List<Tab> tabs = new ArrayList<>();
        tabs.add(mTab1);
        tabs.add(mTab2);
        when(mTab1.isClosing()).thenReturn(true);
        when(mTab2.isClosing()).thenReturn(true);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);

        String title = TabGroupTitleUtils.getDisplayableTitle(mContext, mTabModel, TAB_GROUP_ID);

        assertTrue(title.contains("0"));
        assertFalse(title.contains("2"));
    }

    @Test
    public void testGetDisplayableTitle_FallbackNoTabs() {
        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTabModel.getTabGroupTitle(TAB_GROUP_ID)).thenReturn(UNSET_TAB_GROUP_TITLE);
        List<Tab> tabs = new ArrayList<>();
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);

        String title = TabGroupTitleUtils.getDisplayableTitle(mContext, mTabModel, TAB_GROUP_ID);

        assertTrue(title.contains("0"));
    }
}
