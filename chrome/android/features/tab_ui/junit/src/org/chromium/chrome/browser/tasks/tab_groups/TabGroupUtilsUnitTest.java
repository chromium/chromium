// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_groups;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
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
import org.chromium.base.UserDataHost;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabUiUnitTestUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link TabGroupUtils}.
 */
@SuppressWarnings("ResultOfMethodCallIgnored")
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupUtilsUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final String TAB_GROUP_TITLES_FILE_NAME = "tab_group_titles";
    private static final String TAB1_TITLE = "Tab1";
    private static final String TAB2_TITLE = "Tab2";
    private static final String TAB3_TITLE = "Tab3";
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;
    private static final int POSITION1 = 0;
    private static final int POSITION2 = 1;
    private static final int POSITION3 = 2;

    @Mock
    Context mContext;
    @Mock
    TabModel mTabModel;
    @Mock
    TabModelSelector mTabModelSelector;
    @Mock
    TabModelFilterProvider mTabModelFilterProvider;
    @Mock
    TabGroupModelFilter mTabGroupModelFilter;
    @Mock
    SharedPreferences mSharedPreferences;
    @Mock
    SharedPreferences.Editor mEditor;
    @Mock
    SharedPreferences.Editor mPutStringEditor;
    @Mock
    SharedPreferences.Editor mRemoveEditor;

    private TabImpl mTab1;
    private TabImpl mTab2;
    private TabImpl mTab3;

    @Before
    public void setUp() {

        MockitoAnnotations.initMocks(this);

        mTab1 = TabUiUnitTestUtils.prepareTab(TAB1_ID, TAB1_TITLE, "");
        mTab2 = TabUiUnitTestUtils.prepareTab(TAB2_ID, TAB2_TITLE, "");
        mTab3 = TabUiUnitTestUtils.prepareTab(TAB3_ID, TAB3_TITLE, "");

        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(POSITION1).when(mTabModel).indexOf(mTab1);
        doReturn(POSITION2).when(mTabModel).indexOf(mTab2);
        doReturn(POSITION3).when(mTabModel).indexOf(mTab3);
        doReturn(mSharedPreferences)
                .when(mContext)
                .getSharedPreferences(TAB_GROUP_TITLES_FILE_NAME, Context.MODE_PRIVATE);
        doReturn(mEditor).when(mSharedPreferences).edit();
        doReturn(mRemoveEditor).when(mEditor).remove(any(String.class));
        doReturn(mPutStringEditor).when(mEditor).putString(any(String.class), any(String.class));
        ContextUtils.initApplicationContextForTests(mContext);
    }

    @Test
    public void testGetSelectedTabInGroupForTab() {
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab2);

        assertThat(TabGroupUtils.getSelectedTabInGroupForTab(mTabModelSelector, mTab1),
                equalTo(mTab1));
        assertThat(TabGroupUtils.getSelectedTabInGroupForTab(mTabModelSelector, mTab2),
                equalTo(mTab1));
    }

    @Test
    public void testGetFirstTabModelIndexForList() {
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3));

        assertThat(TabGroupUtils.getFirstTabModelIndexForList(mTabModel, tabs), equalTo(POSITION1));
    }

    @Test
    public void testGetLastTabModelIndexForList() {
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3));

        assertThat(TabGroupUtils.getLastTabModelIndexForList(mTabModel, tabs), equalTo(POSITION3));
    }

    @Test
    public void testDeleteTabGroupTitle() {
        TabGroupUtils.deleteTabGroupTitle(TAB1_ID);

        verify(mEditor).remove(eq(String.valueOf(TAB1_ID)));
        verify(mRemoveEditor).apply();
    }

    @Test
    public void testGetTabGroupTitle() {
        // Mock that we have a stored tab group title with reference to TAB1_ID.
        when(mSharedPreferences.getString(
                     String.valueOf(CriticalPersistedTabData.from(mTab1).getRootId()), null))
                .thenReturn(TAB1_TITLE);

        assertThat(TabGroupUtils.getTabGroupTitle(TAB1_ID), equalTo(TAB1_TITLE));
    }

    @Test
    public void testStoreTabGroupTitle() {
        TabGroupUtils.storeTabGroupTitle(TAB1_ID, TAB1_TITLE);

        verify(mEditor).putString(eq(String.valueOf(TAB1_ID)), eq(TAB1_TITLE));
        verify(mPutStringEditor).apply();
    }

    private void createTabGroup(List<Tab> tabs, int rootId) {
        for (Tab tab : tabs) {
            when(mTabGroupModelFilter.getRelatedTabList(tab.getId())).thenReturn(tabs);
            CriticalPersistedTabData criticalPersistedTabData =
                    mock(CriticalPersistedTabData.class);
            UserDataHost userDataHost = new UserDataHost();
            userDataHost.setUserData(CriticalPersistedTabData.class, criticalPersistedTabData);
            doReturn(userDataHost).when(tab).getUserDataHost();
            doReturn(rootId).when(criticalPersistedTabData).getRootId();
        }
    }
}
