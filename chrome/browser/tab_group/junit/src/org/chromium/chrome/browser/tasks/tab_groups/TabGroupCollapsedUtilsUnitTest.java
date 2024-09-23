// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_groups;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
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

/** Tests for {@link TabGroupCollapsedUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupCollapsedUtilsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String TAB_GROUP_COLLAPSED_FILE_NAME = "tab_group_collapsed";

    private static final int TAB_ID = 456;

    @Mock Context mContext;
    @Mock SharedPreferences mSharedPreferences;
    @Mock SharedPreferences.Editor mEditor;
    @Mock SharedPreferences.Editor mPutStringEditor;
    @Mock SharedPreferences.Editor mRemoveEditor;

    @Before
    public void setUp() {
        doReturn(mSharedPreferences)
                .when(mContext)
                .getSharedPreferences(TAB_GROUP_COLLAPSED_FILE_NAME, Context.MODE_PRIVATE);
        doReturn(mEditor).when(mSharedPreferences).edit();
        doReturn(mRemoveEditor).when(mEditor).remove(any(String.class));
        doReturn(mPutStringEditor).when(mEditor).putBoolean(any(String.class), any(Boolean.class));
        ContextUtils.initApplicationContextForTests(mContext);
    }

    @Test
    public void testDeleteTabGroupCollapsed() {
        TabGroupCollapsedUtils.deleteTabGroupCollapsed(TAB_ID);
        verify(mEditor).remove(eq(String.valueOf(TAB_ID)));
        verify(mRemoveEditor).apply();
    }

    @Test
    public void testGetTabGroupCollapsed() {
        when(mSharedPreferences.getBoolean(String.valueOf(TAB_ID), false)).thenReturn(true);
        assertTrue(TabGroupCollapsedUtils.getTabGroupCollapsed(TAB_ID));
    }

    @Test
    public void testStoreTabGroupCollapsed() {
        TabGroupCollapsedUtils.storeTabGroupCollapsed(TAB_ID, true);
        verify(mEditor).putBoolean(eq(String.valueOf(TAB_ID)), eq(true));
        verify(mPutStringEditor).apply();
    }
}
