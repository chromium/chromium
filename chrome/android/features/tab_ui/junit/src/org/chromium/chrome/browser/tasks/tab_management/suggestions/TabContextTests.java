// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.WebContents;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.Arrays;
import java.util.List;

/**
 * Tests functionality related to TabContext
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabContextTests {
    private static final int TAB_0_ID = 0;
    private static final int RELATED_TAB_0_ID = 1;
    private static final int RELATED_TAB_1_ID = 2;
    private static final int LAST_COMMITTED_INDEX = 1;

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private TabModelSelector mTabModelSelector;

    @Mock
    private TabModelFilterProvider mTabModelFilterProvider;

    @Mock
    private TabModelFilter mTabModelFilter;

    private static Tab sTab0 = mockTab(TAB_0_ID, 6, "mock_title_tab_0", "mock_url_tab_0",
            "mock_original_url_tab_0", "mock_referrer_url_tab_0", 100);
    private static Tab sRelatedTab0 =
            mockTab(RELATED_TAB_0_ID, 6, "mock_title_related_tab_0", "mock_url_related_tab_0",
                    "mock_original_url_related_tab_0", "mock_referrer_url_related_tab_0", 200);
    private static Tab sRelatedTab1 =
            mockTab(RELATED_TAB_1_ID, 6, "mock_title_related_tab_1", "mock_url_related_tab_1",
                    "mock_original_url_related_tab_1", "mock_referrer_url_related_tab_1", 300);

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doReturn(mTabModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
    }

    private static Tab mockTab(int id, int rootId, String title, String url, String originalUrl,
            String referrerUrl, long getTimestampMillis) {
        Tab tab = mock(Tab.class);
        doReturn(id).when(tab).getId();
        doReturn(rootId).when(tab).getRootId();
        doReturn(title).when(tab).getTitle();
        doReturn(url).when(tab).getUrl();
        doReturn(originalUrl).when(tab).getOriginalUrl();
        WebContents webContents = mock(WebContents.class);
        doReturn(webContents).when(tab).getWebContents();
        NavigationController navigationController = mock(NavigationController.class);
        doReturn(navigationController).when(webContents).getNavigationController();
        doReturn(LAST_COMMITTED_INDEX).when(navigationController).getLastCommittedEntryIndex();
        NavigationEntry navigationEntry = mock(NavigationEntry.class);
        doReturn(navigationEntry)
                .when(navigationController)
                .getEntryAtIndex(eq(LAST_COMMITTED_INDEX));
        doReturn(referrerUrl).when(navigationEntry).getReferrerUrl();
        return tab;
    }

    /**
     * Test finding related tabs
     */
    @Test
    public void testRelatedTabsExist() {
        doReturn(sTab0).when(mTabModelFilter).getTabAt(eq(TAB_0_ID));
        doReturn(1).when(mTabModelFilter).getCount();
        doReturn(Arrays.asList(sTab0, sRelatedTab0, sRelatedTab1))
                .when(mTabModelFilter)
                .getRelatedTabList(eq(TAB_0_ID));
        TabContext tabContext = TabContext.createCurrentContext(mTabModelSelector);
        Assert.assertEquals(tabContext.getUngroupedTabs().size(), 0);
        List<TabContext.TabGroupInfo> tabGroupInfo = tabContext.getTabGroups();
        Assert.assertEquals(1, tabGroupInfo.size());
        List<TabContext.TabInfo> groupedTabs = tabGroupInfo.get(0).tabs;
        Assert.assertEquals(3, groupedTabs.size());
        Assert.assertEquals(TAB_0_ID, groupedTabs.get(0).id);
        Assert.assertEquals(RELATED_TAB_0_ID, groupedTabs.get(1).id);
        Assert.assertEquals(RELATED_TAB_1_ID, groupedTabs.get(2).id);
    }

    /**
     * Test finding no related tabs
     */
    @Test
    public void testFindNoRelatedTabs() {
        doReturn(sTab0).when(mTabModelFilter).getTabAt(eq(TAB_0_ID));
        doReturn(1).when(mTabModelFilter).getCount();
        doReturn(Arrays.asList(sTab0)).when(mTabModelFilter).getRelatedTabList(eq(TAB_0_ID));
        TabContext tabContext = TabContext.createCurrentContext(mTabModelSelector);
        Assert.assertEquals(tabContext.getUngroupedTabs().size(), 1);
        List<TabContext.TabGroupInfo> tabGroups = tabContext.getTabGroups();
        Assert.assertEquals(0, tabGroups.size());
        List<TabContext.TabInfo> ungroupedTabs = tabContext.getUngroupedTabs();
        Assert.assertEquals(1, ungroupedTabs.size());
        Assert.assertEquals(TAB_0_ID, ungroupedTabs.get(0).id);
    }
}
