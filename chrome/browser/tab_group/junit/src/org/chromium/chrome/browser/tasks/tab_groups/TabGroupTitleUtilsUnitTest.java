// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_groups;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
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
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.Features;

/** Tests for {@link TabGroupTitleUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupTitleUtilsUnitTest {
    @Rule public TestRule mProcessor = new Features.JUnitProcessor();

    private static final String TAB_GROUP_TITLES_FILE_NAME = "tab_group_titles";

    private static final int TAB_ID = 456;
    private static final String TAB_TITLE = "Tab";

    @Mock Context mContext;
    @Mock Tab mTab;
    @Mock SharedPreferences mSharedPreferences;
    @Mock SharedPreferences.Editor mEditor;
    @Mock SharedPreferences.Editor mPutStringEditor;
    @Mock SharedPreferences.Editor mRemoveEditor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

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
}
