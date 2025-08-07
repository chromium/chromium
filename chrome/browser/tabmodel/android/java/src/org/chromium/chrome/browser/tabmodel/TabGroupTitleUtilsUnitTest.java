// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.SharedPreferences;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;

import java.util.ArrayList;
import java.util.List;

/** Tests for {@link TabGroupTitleUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupTitleUtilsUnitTest {
    private static final String TAB_GROUP_TITLES_FILE_NAME = "tab_group_titles";

    private static final int TAB_ID = 456;
    private static final Token TAB_GROUP_ID = new Token(34789L, 3784L);
    private static final String TAB_TITLE = "Tab";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock SharedPreferences mSharedPreferences;
    @Mock SharedPreferences.Editor mEditor;
    @Mock SharedPreferences.Editor mPutStringEditor;
    @Mock SharedPreferences.Editor mRemoveEditor;
    @Mock TabGroupModelFilter mTabGroupModelFilter;
    @Mock Tab mTab1;
    @Mock Tab mTab2;
    @Mock Tab mTab3;

    Context mContext;

    @Before
    public void setUp() {
        mContext = spy(ApplicationProvider.getApplicationContext());
        doReturn(mSharedPreferences)
                .when(mContext)
                .getSharedPreferences(TAB_GROUP_TITLES_FILE_NAME, Context.MODE_PRIVATE);
        doReturn(mEditor).when(mSharedPreferences).edit();
        doReturn(mRemoveEditor).when(mEditor).remove(any(String.class));
        doReturn(mPutStringEditor).when(mEditor).putString(any(String.class), any(String.class));
        ContextUtils.initApplicationContextForTests(mContext);
    }

    @Test
    public void testDeleteTabGroupTitle() {
        TabGroupVisualDataStore.deleteTabGroupTitle(TAB_ID);

        verify(mEditor).remove(eq(String.valueOf(TAB_ID)));
        verify(mRemoveEditor).apply();
    }

    @Test
    public void testGetTabGroupTitle() {
        // Mock that we have a stored tab group title with reference to TAB_ID.
        when(mSharedPreferences.getString(String.valueOf(TAB_ID), null)).thenReturn(TAB_TITLE);

        assertThat(TabGroupVisualDataStore.getTabGroupTitle(TAB_ID), equalTo(TAB_TITLE));
    }

    @Test
    public void testStoreTabGroupTitle() {
        TabGroupVisualDataStore.storeTabGroupTitle(TAB_ID, TAB_TITLE);

        verify(mEditor).putString(eq(String.valueOf(TAB_ID)), eq(TAB_TITLE));
        verify(mPutStringEditor).apply();
    }

    @Test
    public void testStoreTabGroupTitle_Empty() {
        TabGroupVisualDataStore.storeTabGroupTitle(TAB_ID, "");

        verify(mEditor).remove(eq(String.valueOf(TAB_ID)));
        verify(mRemoveEditor).apply();
    }

    @Test
    public void testStoreTabGroupTitle_Null() {
        TabGroupVisualDataStore.storeTabGroupTitle(TAB_ID, null);

        verify(mEditor).remove(eq(String.valueOf(TAB_ID)));
        verify(mRemoveEditor).apply();
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
        when(mTabGroupModelFilter.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTabGroupModelFilter.getTabGroupTitle(TAB_GROUP_ID)).thenReturn(title);
        assertEquals(
                title,
                TabGroupTitleUtils.getDisplayableTitle(
                        mContext, mTabGroupModelFilter, TAB_GROUP_ID));
    }

    @Test
    public void testGetDisplayableTitle_Fallback() {
        int tabCount = 4567;
        when(mTabGroupModelFilter.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTabGroupModelFilter.getTabGroupTitle(TAB_GROUP_ID)).thenReturn("");

        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < tabCount; i++) {
            Tab tab = mock(Tab.class);
            when(tab.isClosing()).thenReturn(false);
            tabs.add(tab);
        }
        when(mTabGroupModelFilter.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);

        String title =
                TabGroupTitleUtils.getDisplayableTitle(
                        mContext, mTabGroupModelFilter, TAB_GROUP_ID);
        assertTrue(title.contains(String.valueOf(tabCount)));
    }

    @Test
    public void testGetDisplayableTitle_FallbackNoClosingTabs() {
        when(mTabGroupModelFilter.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTabGroupModelFilter.getTabGroupTitle(TAB_GROUP_ID)).thenReturn(null);
        List<Tab> tabs = new ArrayList<>();
        tabs.add(mTab1);
        tabs.add(mTab2);
        when(mTab1.isClosing()).thenReturn(false);
        when(mTab2.isClosing()).thenReturn(false);
        when(mTabGroupModelFilter.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);

        String title =
                TabGroupTitleUtils.getDisplayableTitle(
                        mContext, mTabGroupModelFilter, TAB_GROUP_ID);

        assertTrue(title.contains("2"));
    }

    @Test
    public void testGetDisplayableTitle_FallbackSomeClosingTabs() {
        when(mTabGroupModelFilter.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTabGroupModelFilter.getTabGroupTitle(TAB_GROUP_ID)).thenReturn(null);
        List<Tab> tabs = new ArrayList<>();
        tabs.add(mTab1);
        tabs.add(mTab2);
        tabs.add(mTab3);
        when(mTab1.isClosing()).thenReturn(false);
        when(mTab2.isClosing()).thenReturn(true);
        when(mTab3.isClosing()).thenReturn(false);
        when(mTabGroupModelFilter.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);

        String title =
                TabGroupTitleUtils.getDisplayableTitle(
                        mContext, mTabGroupModelFilter, TAB_GROUP_ID);

        assertTrue(title.contains("2"));
        assertFalse(title.contains("3"));
    }

    @Test
    public void testGetDisplayableTitle_FallbackAllClosingTabs() {
        when(mTabGroupModelFilter.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTabGroupModelFilter.getTabGroupTitle(TAB_GROUP_ID)).thenReturn(null);
        List<Tab> tabs = new ArrayList<>();
        tabs.add(mTab1);
        tabs.add(mTab2);
        when(mTab1.isClosing()).thenReturn(true);
        when(mTab2.isClosing()).thenReturn(true);
        when(mTabGroupModelFilter.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);

        String title =
                TabGroupTitleUtils.getDisplayableTitle(
                        mContext, mTabGroupModelFilter, TAB_GROUP_ID);

        assertTrue(title.contains("0"));
        assertFalse(title.contains("2"));
    }

    @Test
    public void testGetDisplayableTitle_FallbackNoTabs() {
        when(mTabGroupModelFilter.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTabGroupModelFilter.getTabGroupTitle(TAB_GROUP_ID)).thenReturn(null);
        List<Tab> tabs = new ArrayList<>();
        when(mTabGroupModelFilter.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);

        String title =
                TabGroupTitleUtils.getDisplayableTitle(
                        mContext, mTabGroupModelFilter, TAB_GROUP_ID);

        assertTrue(title.contains("0"));
    }
}
