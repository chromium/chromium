// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.pseudotab;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.test.util.browser.Features;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Unit tests for {@link PseudoTab}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class PseudoTabUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;

    @Mock
    TabModelFilter mTabModelFilter;
    @Mock
    TabModelFilter mTabModelFilter2;
    @Mock
    TabModelFilterProvider mTabModelFilterProvider;

    private Tab mTab1;
    private Tab mTab2;
    private Tab mTab3;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mTab1 = prepareTab(TAB1_ID);
        mTab2 = prepareTab(TAB2_ID);
        mTab3 = prepareTab(TAB3_ID);
    }

    @After
    public void tearDown() {
        TabAttributeCache.clearAllForTesting();

        // This is necessary to get the cache behavior correct.
        Runtime runtime = Runtime.getRuntime();
        runtime.runFinalization();
        runtime.gc();
    }

    @Test
    public void fromTabId() {
        PseudoTab tab = PseudoTab.fromTabId(TAB1_ID);
        Assert.assertEquals(TAB1_ID, tab.getId());
        Assert.assertFalse(tab.hasRealTab());
        Assert.assertNull(tab.getTab());
    }

    @Test
    public void fromTabId_cached() {
        PseudoTab tab1 = PseudoTab.fromTabId(TAB1_ID);
        PseudoTab tab2 = PseudoTab.fromTabId(TAB2_ID);
        PseudoTab tab1prime = PseudoTab.fromTabId(TAB1_ID);
        Assert.assertNotEquals(tab1, tab2);
        Assert.assertEquals(tab1, tab1prime);
    }

    @Test
    public void fromTab() {
        PseudoTab tab = PseudoTab.fromTab(mTab1);
        Assert.assertEquals(TAB1_ID, tab.getId());
        Assert.assertTrue(tab.hasRealTab());
        Assert.assertEquals(mTab1, tab.getTab());
    }

    @Test
    public void fromTab_cached() {
        PseudoTab tab1 = PseudoTab.fromTab(mTab1);
        PseudoTab tab2 = PseudoTab.fromTab(mTab2);
        PseudoTab tab1prime = PseudoTab.fromTab(mTab1);
        Assert.assertNotEquals(tab1, tab2);
        Assert.assertEquals(tab1, tab1prime);
    }

    @Test
    public void fromTab_cached_upgrade() {
        PseudoTab tab1 = PseudoTab.fromTabId(TAB1_ID);
        Assert.assertFalse(tab1.hasRealTab());

        PseudoTab tab1upgraded = PseudoTab.fromTab(mTab1);
        Assert.assertTrue(tab1upgraded.hasRealTab());

        Assert.assertNotEquals(tab1, tab1upgraded);

        PseudoTab tab1prime = PseudoTab.fromTabId(TAB1_ID);
        Assert.assertEquals(tab1upgraded, tab1prime);
    }

    @Test
    public void getListOfPseudoTab_listOfTab() {
        List<PseudoTab> list = PseudoTab.getListOfPseudoTab(Arrays.asList(mTab1, mTab2));
        Assert.assertEquals(2, list.size());
        Assert.assertEquals(TAB1_ID, list.get(0).getId());
        Assert.assertEquals(TAB2_ID, list.get(1).getId());
    }

    @Test
    public void getListOfPseudoTab_listOfTab_null() {
        List<Tab> tabs = null;
        List<PseudoTab> list = PseudoTab.getListOfPseudoTab(tabs);
        Assert.assertNull(list);
    }

    @Test
    public void getListOfPseudoTab_TabList() {
        doReturn(mTab1).when(mTabModelFilter).getTabAt(0);
        doReturn(mTab2).when(mTabModelFilter).getTabAt(1);
        doReturn(mTab3).when(mTabModelFilter).getTabAt(2);
        doReturn(3).when(mTabModelFilter).getCount();

        List<PseudoTab> list = PseudoTab.getListOfPseudoTab(mTabModelFilter);
        Assert.assertEquals(3, list.size());
        Assert.assertEquals(TAB1_ID, list.get(0).getId());
        Assert.assertEquals(TAB2_ID, list.get(1).getId());
        Assert.assertEquals(TAB3_ID, list.get(2).getId());
    }

    @Test
    public void getListOfPseudoTab_TabList_null() {
        TabList tabs = null;
        List<PseudoTab> list = PseudoTab.getListOfPseudoTab(tabs);
        Assert.assertNull(list);
    }

    @Test
    public void testToString() {
        Assert.assertEquals("Tab 456", PseudoTab.fromTabId(TAB1_ID).toString());
    }

    @Test
    public void getTitle_provider() {
        String title = "title provider";
        PseudoTab.TitleProvider provider = (tab) -> title;

        PseudoTab tab = PseudoTab.fromTabId(TAB1_ID);
        Assert.assertEquals(title, tab.getTitle(provider));

        PseudoTab realTab = PseudoTab.fromTab(mTab1);
        Assert.assertNotEquals(tab, realTab);
        Assert.assertEquals(title, realTab.getTitle(provider));
    }

    @Test
    public void getTitle_nullProvider() {
        PseudoTab.TitleProvider provider = null;

        PseudoTab tab = PseudoTab.fromTabId(TAB1_ID);
        Assert.assertEquals(tab.getTitle(), tab.getTitle(provider));
    }

    @Test
    public void getTitle_realTab() {
        String title = "title 1 real";
        doReturn(title).when(mTab1).getTitle();

        PseudoTab tab = PseudoTab.fromTabId(TAB1_ID);
        Assert.assertEquals("", tab.getTitle());

        PseudoTab realTab = PseudoTab.fromTab(mTab1);
        Assert.assertNotEquals(tab, realTab);
        Assert.assertEquals(title, realTab.getTitle());
    }

    @Test
    public void getTitle_cache() {
        String title = "title 1";
        TabAttributeCache.setTitleForTesting(TAB1_ID, title);

        PseudoTab tab = PseudoTab.fromTabId(TAB1_ID);
        Assert.assertEquals(title, tab.getTitle());

        PseudoTab realTab = PseudoTab.fromTab(mTab1);
        Assert.assertNotEquals(tab, realTab);
        Assert.assertNull(realTab.getTitle());
    }

    @Test
    public void getUrl_real() {
        String url = "url 1 real";
        doReturn(url).when(mTab1).getUrl();

        PseudoTab tab = PseudoTab.fromTabId(TAB1_ID);
        Assert.assertEquals("", tab.getUrl());

        PseudoTab realTab = PseudoTab.fromTab(mTab1);
        Assert.assertNotEquals(tab, realTab);
        Assert.assertEquals(url, realTab.getUrl());
    }

    @Test
    public void getUrl_cache() {
        String url = "url 1";
        TabAttributeCache.setUrlForTesting(TAB1_ID, url);

        PseudoTab tab = PseudoTab.fromTabId(TAB1_ID);
        Assert.assertEquals(url, tab.getUrl());

        PseudoTab realTab = PseudoTab.fromTab(mTab1);
        Assert.assertNotEquals(tab, realTab);
        Assert.assertNull(realTab.getUrl());
    }

    @Test
    public void getRootId_real() {
        int rootId = 1337;
        doReturn(rootId).when(mTab1).getRootId();

        PseudoTab tab = PseudoTab.fromTabId(TAB1_ID);
        Assert.assertEquals(Tab.INVALID_TAB_ID, tab.getRootId());

        PseudoTab realTab = PseudoTab.fromTab(mTab1);
        Assert.assertNotEquals(tab, realTab);
        Assert.assertEquals(rootId, realTab.getRootId());
    }

    @Test
    public void getRootId_cache() {
        int rootId = 42;
        TabAttributeCache.setRootIdForTesting(TAB1_ID, rootId);

        PseudoTab tab = PseudoTab.fromTabId(TAB1_ID);
        Assert.assertEquals(rootId, tab.getRootId());

        PseudoTab realTab = PseudoTab.fromTab(mTab1);
        Assert.assertNotEquals(tab, realTab);
        Assert.assertNotEquals(rootId, realTab.getRootId());
    }

    @Test
    public void isIncognito() {
        doReturn(true).when(mTab1).isIncognito();

        PseudoTab tab = PseudoTab.fromTabId(TAB1_ID);
        Assert.assertFalse(tab.isIncognito());

        PseudoTab realTab = PseudoTab.fromTab(mTab1);
        Assert.assertNotEquals(tab, realTab);
        Assert.assertTrue(realTab.isIncognito());

        doReturn(false).when(mTab1).isIncognito();
        Assert.assertFalse(realTab.isIncognito());
    }

    @Test
    public void getTimestampMillis_realTab() {
        long timestamp = 12345;
        doReturn(timestamp).when(mTab1).getTimestampMillis();

        PseudoTab tab = PseudoTab.fromTab(mTab1);
        Assert.assertEquals(timestamp, tab.getTimestampMillis());
    }

    @Test(expected = AssertionError.class)
    public void getTimestampMillis_notRealTab() {
        PseudoTab tab = PseudoTab.fromTabId(TAB1_ID);
        tab.getTimestampMillis();
    }

    @Test
    public void getRelatedTabs_noProvider_groupDisabled_single() {
        doReturn(null).when(mTabModelFilterProvider).getTabModelFilter(anyBoolean());
        FeatureUtilities.setTabGroupsAndroidEnabledForTesting(false);

        PseudoTab tab1 = PseudoTab.fromTabId(TAB1_ID);
        List<PseudoTab> related = PseudoTab.getRelatedTabs(tab1, mTabModelFilterProvider);
        Assert.assertEquals(1, related.size());
        Assert.assertEquals(TAB1_ID, related.get(0).getId());
    }

    @Test
    public void getRelatedTabs_noProvider_groupDisabled_group() {
        doReturn(null).when(mTabModelFilterProvider).getTabModelFilter(anyBoolean());
        FeatureUtilities.setTabGroupsAndroidEnabledForTesting(false);

        TabAttributeCache.setRootIdForTesting(TAB1_ID, TAB1_ID);
        TabAttributeCache.setRootIdForTesting(TAB2_ID, TAB1_ID);
        PseudoTab tab1 = PseudoTab.fromTabId(TAB1_ID);
        Assert.assertEquals(TAB1_ID, tab1.getRootId());
        PseudoTab tab2 = PseudoTab.fromTabId(TAB2_ID);
        Assert.assertEquals(TAB1_ID, tab2.getRootId());

        List<PseudoTab> related = PseudoTab.getRelatedTabs(tab1, mTabModelFilterProvider);
        Assert.assertEquals(1, related.size());
        Assert.assertEquals(TAB1_ID, related.get(0).getId());
    }

    @Test
    public void getRelatedTabs_noProvider_single() {
        doReturn(null).when(mTabModelFilterProvider).getTabModelFilter(anyBoolean());
        FeatureUtilities.setTabGroupsAndroidEnabledForTesting(true);

        PseudoTab tab1 = PseudoTab.fromTabId(TAB1_ID);
        List<PseudoTab> related = PseudoTab.getRelatedTabs(tab1, mTabModelFilterProvider);
        Assert.assertEquals(1, related.size());
        Assert.assertEquals(TAB1_ID, related.get(0).getId());
    }

    @Test
    public void getRelatedTabs_noProvider_group() {
        doReturn(null).when(mTabModelFilterProvider).getTabModelFilter(anyBoolean());
        FeatureUtilities.setTabGroupsAndroidEnabledForTesting(true);

        TabAttributeCache.setRootIdForTesting(TAB1_ID, TAB1_ID);
        TabAttributeCache.setRootIdForTesting(TAB2_ID, TAB1_ID);
        PseudoTab tab1 = PseudoTab.fromTabId(TAB1_ID);
        PseudoTab tab2 = PseudoTab.fromTabId(TAB2_ID);

        List<PseudoTab> related = PseudoTab.getRelatedTabs(tab1, mTabModelFilterProvider);
        Assert.assertEquals(2, related.size());
        Assert.assertEquals(TAB1_ID, related.get(0).getId());
        Assert.assertEquals(TAB2_ID, related.get(1).getId());
    }

    @Test
    public void getRelatedTabs_noProvider_badGroup() {
        doReturn(null).when(mTabModelFilterProvider).getTabModelFilter(anyBoolean());
        FeatureUtilities.setTabGroupsAndroidEnabledForTesting(true);

        TabAttributeCache.setRootIdForTesting(TAB1_ID, TAB1_ID);
        TabAttributeCache.setRootIdForTesting(TAB2_ID, Tab.INVALID_TAB_ID);
        TabAttributeCache.setRootIdForTesting(TAB3_ID, TAB3_ID);
        PseudoTab tab1 = PseudoTab.fromTabId(TAB1_ID);
        PseudoTab tab2 = PseudoTab.fromTabId(TAB2_ID);
        PseudoTab tab3 = PseudoTab.fromTabId(TAB3_ID);

        List<PseudoTab> related = PseudoTab.getRelatedTabs(tab1, mTabModelFilterProvider);
        Assert.assertEquals(1, related.size());
        Assert.assertEquals(TAB1_ID, related.get(0).getId());
    }

    @Test
    public void getRelatedTabs_provider_normal() {
        doReturn(mTabModelFilter).when(mTabModelFilterProvider).getTabModelFilter(eq(false));
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3));
        doReturn(tabs).when(mTabModelFilter).getRelatedTabList(TAB1_ID);

        PseudoTab tab1 = PseudoTab.fromTabId(TAB1_ID);
        List<PseudoTab> related = PseudoTab.getRelatedTabs(tab1, mTabModelFilterProvider);
        Assert.assertEquals(3, related.size());
        Assert.assertEquals(TAB1_ID, related.get(0).getId());
        Assert.assertEquals(TAB2_ID, related.get(1).getId());
        Assert.assertEquals(TAB3_ID, related.get(2).getId());
    }

    @Test
    public void getRelatedTabs_provider_incognito() {
        doReturn(mTabModelFilter).when(mTabModelFilterProvider).getTabModelFilter(eq(false));
        List<Tab> empty = new ArrayList<>();
        doReturn(empty).when(mTabModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(mTabModelFilter2).when(mTabModelFilterProvider).getTabModelFilter(eq(true));
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        doReturn(tabs).when(mTabModelFilter2).getRelatedTabList(TAB1_ID);

        PseudoTab tab1 = PseudoTab.fromTabId(TAB1_ID);
        List<PseudoTab> related = PseudoTab.getRelatedTabs(tab1, mTabModelFilterProvider);
        Assert.assertEquals(2, related.size());
        Assert.assertEquals(TAB1_ID, related.get(0).getId());
        Assert.assertEquals(TAB2_ID, related.get(1).getId());
    }

    private Tab prepareTab(int id) {
        Tab tab = mock(Tab.class);
        doReturn(id).when(tab).getId();
        return tab;
    }
}
