// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.text.TextUtils;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatcher;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Tests for {@link TabGroupUtils}. */
@SuppressWarnings("ResultOfMethodCallIgnored")
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupUtilsUnitTest {
    private static final String TAB1_TITLE = "Tab1";
    private static final String TAB2_TITLE = "Tab2";
    private static final String TAB3_TITLE = "Tab3";
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;
    private static final int POSITION1 = 0;
    private static final int POSITION2 = 1;
    private static final int POSITION3 = 2;
    private static final Token TAB_GROUP_ID = new Token(2L, 2L);
    private static final String TAB_GROUP_TITLE = "Regrouped tabs";
    private static final LinkedHashMap<Integer, String> TAB_IDS_TO_URLS =
            new LinkedHashMap<>(
                    Map.ofEntries(
                            Map.entry(TAB1_ID, "https://www.amazon.com/"),
                            Map.entry(TAB2_ID, "https://www.youtube.com/"),
                            Map.entry(TAB3_ID, "https://www.facebook.com/")));

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock TabModel mTabModel;
    @Mock TabModelSelector mTabModelSelector;
    @Mock TabGroupModelFilterProvider mTabGroupModelFilterProvider;
    @Mock TabGroupModelFilter mTabGroupModelFilter;
    @Mock TabCreator mTabCreator;

    private Tab mTab1;
    private Tab mTab2;
    private Tab mTab3;

    @Before
    public void setUp() {
        mTab1 = TabUiUnitTestUtils.prepareTab(TAB1_ID, TAB1_TITLE, GURL.emptyGURL());
        mTab2 = TabUiUnitTestUtils.prepareTab(TAB2_ID, TAB2_TITLE, GURL.emptyGURL());
        mTab3 = TabUiUnitTestUtils.prepareTab(TAB3_ID, TAB3_TITLE, GURL.emptyGURL());

        doReturn(mTabGroupModelFilterProvider)
                .when(mTabModelSelector)
                .getTabGroupModelFilterProvider();
        doReturn(mTabGroupModelFilter)
                .when(mTabGroupModelFilterProvider)
                .getCurrentTabGroupModelFilter();
        doReturn(POSITION1).when(mTabModel).indexOf(mTab1);
        doReturn(POSITION2).when(mTabModel).indexOf(mTab2);
        doReturn(POSITION3).when(mTabModel).indexOf(mTab3);
    }

    @Test
    public void testGetSelectedTabInGroupForTab() {
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID);
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION1);
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(mTab1);
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(mTab2);

        assertThat(
                TabGroupUtils.getSelectedTabInGroupForTab(mTabGroupModelFilter, mTab1),
                equalTo(mTab1));
        assertThat(
                TabGroupUtils.getSelectedTabInGroupForTab(mTabGroupModelFilter, mTab2),
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
    public void testOpenUrlInGroup() {
        when(mTabGroupModelFilter.getRelatedTabList(eq(TAB1_ID))).thenReturn(Arrays.asList(mTab1));
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);

        @TabLaunchType int launchType = TabLaunchType.FROM_TAB_GROUP_UI;
        String url = JUnitTestGURLs.URL_1.getSpec();
        TabGroupUtils.openUrlInGroup(mTabGroupModelFilter, url, TAB1_ID, launchType);
        ArgumentMatcher<LoadUrlParams> matcher = params -> TextUtils.equals(params.getUrl(), url);
        verify(mTabCreator).createNewTab(argThat(matcher), eq(launchType), eq(mTab1));
    }

    @Test
    public void testRegroupTabs() {
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3));
        TabGroupMetadata tabGroupMetadata =
                new TabGroupMetadata(
                        /* rootId= */ TAB1_ID,
                        /* selectedTabId= */ TAB1_ID,
                        /* sourceWindowId= */ 1,
                        TAB_GROUP_ID,
                        TAB_IDS_TO_URLS,
                        /* tabGroupColor= */ 0,
                        TAB_GROUP_TITLE,
                        /* mhtmlTabTitle= */ null,
                        /* tabGroupCollapsed= */ true,
                        /* isGroupShared= */ false,
                        /* isIncognito= */ false);
        TabGroupUtils.regroupTabs(mTabGroupModelFilter, tabs, tabGroupMetadata);

        for (Tab tab : tabs) {
            verify(mTabGroupModelFilter).mergeTabsToGroup(eq(tab.getId()), eq(TAB1_ID), eq(true));
            verify(tab).setTabGroupId(TAB_GROUP_ID);
            verify(tab).setRootId(TAB1_ID);
        }
        verify(mTabGroupModelFilter).setTabGroupColor(eq(TAB1_ID), eq(0));
        verify(mTabGroupModelFilter).setTabGroupCollapsed(eq(TAB1_ID), eq(true));
        verify(mTabGroupModelFilter).setTabGroupTitle(eq(TAB1_ID), eq(TAB_GROUP_TITLE));
    }

    private void createTabGroup(List<Tab> tabs, int rootId) {
        for (Tab tab : tabs) {
            when(mTabGroupModelFilter.getRelatedTabList(tab.getId())).thenReturn(tabs);
            when(tab.getRootId()).thenReturn(rootId);
        }
    }
}
