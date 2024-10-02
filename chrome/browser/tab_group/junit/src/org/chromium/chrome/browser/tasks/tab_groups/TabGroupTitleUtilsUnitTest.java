// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_groups;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
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
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link TabGroupTitleUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupTitleUtilsUnitTest {
    private static final String TAB_GROUP_TITLES_FILE_NAME = "tab_group_titles";

    private static final int TAB_ID = 456;
    private static final String TAB_TITLE = "Tab";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock SharedPreferences mSharedPreferences;
    @Mock SharedPreferences.Editor mEditor;
    @Mock SharedPreferences.Editor mPutStringEditor;
    @Mock SharedPreferences.Editor mRemoveEditor;
    @Mock TabGroupModelFilter mTabGroupModelFilter;

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
        TabGroupTitleUtils.deleteTabGroupTitle(TAB_ID);

        verify(mEditor).remove(eq(String.valueOf(TAB_ID)));
        verify(mRemoveEditor).apply();
    }

    @Test
    public void testGetTabGroupTitle() {
        // Mock that we have a stored tab group title with reference to TAB_ID.
        when(mSharedPreferences.getString(String.valueOf(TAB_ID), null)).thenReturn(TAB_TITLE);

        assertThat(TabGroupTitleUtils.getTabGroupTitle(TAB_ID), equalTo(TAB_TITLE));
    }

    @Test
    public void testStoreTabGroupTitle() {
        TabGroupTitleUtils.storeTabGroupTitle(TAB_ID, TAB_TITLE);

        verify(mEditor).putString(eq(String.valueOf(TAB_ID)), eq(TAB_TITLE));
        verify(mPutStringEditor).apply();
    }

    @Test
    public void testStoreTabGroupTitle_Empty() {
        TabGroupTitleUtils.storeTabGroupTitle(TAB_ID, "");

        verify(mEditor).remove(eq(String.valueOf(TAB_ID)));
        verify(mRemoveEditor).apply();
    }

    @Test
    public void testStoreTabGroupTitle_Null() {
        TabGroupTitleUtils.storeTabGroupTitle(TAB_ID, null);

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
        when(mTabGroupModelFilter.getTabGroupTitle(anyInt())).thenReturn(title);
        assertEquals(
                title, TabGroupTitleUtils.getDisplayableTitle(mContext, mTabGroupModelFilter, 12));
    }

    @Test
    public void testGetDisplayableTitle_Fallback() {
        int tabCount = 4567;
        when(mTabGroupModelFilter.getTabGroupTitle(anyInt())).thenReturn("");
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(anyInt())).thenReturn(tabCount);
        String title = TabGroupTitleUtils.getDisplayableTitle(mContext, mTabGroupModelFilter, 12);
        assertTrue(title.contains(String.valueOf(tabCount)));
    }
}
