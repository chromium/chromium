// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.SharedPreferences;

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

/** Tests for {@link TabGroupVisualDataStore}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupVisualDataStoreUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String TAB_GROUP_TITLES_FILE_NAME = "tab_group_titles";
    private static final String TAB_GROUP_COLORS_FILE_NAME = "tab_group_colors";
    private static final String TAB_GROUP_COLLAPSED_FILE_NAME = "tab_group_collapsed";

    private static final int TAB_ID = 456;
    private static final String TAB_TITLE = "Tab";
    private static final int TAB_COLOR = 1;

    @Mock Context mContext;
    @Mock SharedPreferences mTitleSharedPreferences;
    @Mock SharedPreferences mColorSharedPreferences;
    @Mock SharedPreferences mCollapsedSharedPreferences;
    @Mock SharedPreferences.Editor mTitleEditor;
    @Mock SharedPreferences.Editor mColorEditor;
    @Mock SharedPreferences.Editor mCollapsedEditor;
    @Mock SharedPreferences.Editor mPutStringEditor;
    @Mock SharedPreferences.Editor mPutIntEditor;
    @Mock SharedPreferences.Editor mPutBooleanEditor;
    @Mock SharedPreferences.Editor mRemoveTitleEditor;
    @Mock SharedPreferences.Editor mRemoveColorEditor;
    @Mock SharedPreferences.Editor mRemoveCollapsedEditor;

    @Before
    public void setUp() {
        doReturn(mTitleSharedPreferences)
                .when(mContext)
                .getSharedPreferences(TAB_GROUP_TITLES_FILE_NAME, Context.MODE_PRIVATE);
        doReturn(mColorSharedPreferences)
                .when(mContext)
                .getSharedPreferences(TAB_GROUP_COLORS_FILE_NAME, Context.MODE_PRIVATE);
        doReturn(mCollapsedSharedPreferences)
                .when(mContext)
                .getSharedPreferences(TAB_GROUP_COLLAPSED_FILE_NAME, Context.MODE_PRIVATE);

        doReturn(mTitleEditor).when(mTitleSharedPreferences).edit();
        doReturn(mColorEditor).when(mColorSharedPreferences).edit();
        doReturn(mCollapsedEditor).when(mCollapsedSharedPreferences).edit();

        doReturn(mRemoveTitleEditor).when(mTitleEditor).remove(any(String.class));
        doReturn(mRemoveColorEditor).when(mColorEditor).remove(any(String.class));
        doReturn(mRemoveCollapsedEditor).when(mCollapsedEditor).remove(any(String.class));

        doReturn(mPutStringEditor)
                .when(mTitleEditor)
                .putString(any(String.class), any(String.class));
        doReturn(mPutIntEditor).when(mColorEditor).putInt(any(String.class), anyInt());
        doReturn(mPutBooleanEditor)
                .when(mCollapsedEditor)
                .putBoolean(any(String.class), anyBoolean());

        ContextUtils.initApplicationContextForTests(mContext);
    }

    @Test
    public void testDeleteTabGroupTitle() {
        TabGroupVisualDataStore.deleteTabGroupTitle(TAB_ID);
        verify(mTitleEditor).remove(eq(String.valueOf(TAB_ID)));
        verify(mRemoveTitleEditor).apply();
    }

    @Test
    public void testGetTabGroupTitle() {
        when(mTitleSharedPreferences.getString(String.valueOf(TAB_ID), null)).thenReturn(TAB_TITLE);
        assertThat(TabGroupVisualDataStore.getTabGroupTitle(TAB_ID), equalTo(TAB_TITLE));
    }

    @Test
    public void testStoreTabGroupTitle() {
        TabGroupVisualDataStore.storeTabGroupTitle(TAB_ID, TAB_TITLE);
        verify(mTitleEditor).putString(eq(String.valueOf(TAB_ID)), eq(TAB_TITLE));
        verify(mPutStringEditor).apply();
    }

    @Test
    public void testStoreTabGroupTitle_Empty() {
        TabGroupVisualDataStore.storeTabGroupTitle(TAB_ID, "");
        verify(mTitleEditor).remove(eq(String.valueOf(TAB_ID)));
        verify(mRemoveTitleEditor).apply();
    }

    @Test
    public void testStoreTabGroupTitle_Null() {
        TabGroupVisualDataStore.storeTabGroupTitle(TAB_ID, null);
        verify(mTitleEditor).remove(eq(String.valueOf(TAB_ID)));
        verify(mRemoveTitleEditor).apply();
    }

    @Test
    public void testDeleteTabGroupColor() {
        TabGroupVisualDataStore.deleteTabGroupColor(TAB_ID);
        verify(mColorEditor).remove(eq(String.valueOf(TAB_ID)));
        verify(mRemoveColorEditor).apply();
    }

    @Test
    public void testGetTabGroupColor() {
        when(mColorSharedPreferences.getInt(
                        String.valueOf(TAB_ID), TabGroupColorUtils.INVALID_COLOR_ID))
                .thenReturn(TAB_COLOR);
        assertThat(TabGroupVisualDataStore.getTabGroupColor(TAB_ID), equalTo(TAB_COLOR));
    }

    @Test
    public void testStoreTabGroupColor() {
        TabGroupVisualDataStore.storeTabGroupColor(TAB_ID, TAB_COLOR);
        verify(mColorEditor).putInt(eq(String.valueOf(TAB_ID)), eq(TAB_COLOR));
        verify(mPutIntEditor).apply();
    }

    @Test
    public void testDeleteTabGroupCollapsed() {
        TabGroupVisualDataStore.deleteTabGroupCollapsed(TAB_ID);
        verify(mCollapsedEditor).remove(eq(String.valueOf(TAB_ID)));
        verify(mRemoveCollapsedEditor).apply();
    }

    @Test
    public void testGetTabGroupCollapsed() {
        when(mCollapsedSharedPreferences.getBoolean(String.valueOf(TAB_ID), false))
                .thenReturn(true);
        assertTrue(TabGroupVisualDataStore.getTabGroupCollapsed(TAB_ID));
    }

    @Test
    public void testStoreTabGroupCollapsed() {
        TabGroupVisualDataStore.storeTabGroupCollapsed(TAB_ID, true);
        verify(mCollapsedEditor).putBoolean(eq(String.valueOf(TAB_ID)), eq(true));
        verify(mPutBooleanEditor).apply();
    }
}
