// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.pseudotab;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabUiUnitTestUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.stream.IntStream;

/**
 * Unit tests for {@link PseudoTab}.
 */
@SuppressWarnings({"ResultOfMethodCallIgnored", "deprecation"})
@RunWith(BaseRobolectricTestRunner.class)
public class PseudoTabUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;
    private static final int TAB4_ID = 159;

    @Mock
    TabModelFilter mTabModelFilter;
    @Mock
    TabModelFilter mTabModelFilter2;
    @Mock
    TabModelSelector mTabModelSelector;
    @Mock
    TabModelFilterProvider mTabModelFilterProvider;

    @Mock
    CriticalPersistedTabData mCriticalPersistedTabData;

    private TabImpl mTab1;
    private TabImpl mTab2;
    private TabImpl mTab3;
    private TabImpl mTab1Copy;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mTab1 = TabUiUnitTestUtils.prepareTab(TAB1_ID, mCriticalPersistedTabData);
        mTab2 = TabUiUnitTestUtils.prepareTab(TAB2_ID, mCriticalPersistedTabData);
        mTab3 = TabUiUnitTestUtils.prepareTab(TAB3_ID, mCriticalPersistedTabData);
        mTab1Copy = TabUiUnitTestUtils.prepareTab(TAB1_ID, mCriticalPersistedTabData);

        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
    }

    @After
    public void tearDown() {
        TabAttributeCache.clearAllForTesting();
        PseudoTab.clearForTesting();

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
    public void fromTabId_threadSafety() throws InterruptedException {
        final int count = 100000;
        ExecutorService service = Executors.newCachedThreadPool();

        IntStream.range(0, count).forEach(
                tabId -> service.submit(() -> PseudoTab.fromTabId(tabId)));
        service.awaitTermination(1000, TimeUnit.MILLISECONDS);

        Assert.assertEquals(count, PseudoTab.getAllTabsCountForTests());
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
    public void fromTab_obsoleteCache() {
        PseudoTab tab1 = PseudoTab.fromTab(mTab1);
        PseudoTab tab1copy = PseudoTab.fromTab(mTab1Copy);
        Assert.assertNotEquals(tab1, tab1copy);
        Assert.assertEquals(tab1.getId(), tab1copy.getId());
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
        List<PseudoTab> list = PseudoTab.getListOfPseudoTab((List<Tab>) null);
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
        List<PseudoTab> list = PseudoTab.getListOfPseudoTab((TabList) null);
        Assert.assertNull(list);
    }

    @Test
    public void testToString() {
        Assert.assertEquals("Tab 456", PseudoTab.fromTabId(TAB1_ID).toString());
    }

    @Test
    public void getTitle_provider() {
        String title = "title provider";
        PseudoTab.TitleProvider provider = (context, tab) -> title;

        PseudoTab tab = PseudoTab.fromTabId(TAB1_ID);
        Assert.assertEquals(title, tab.getTitle(ContextUtils.getApplicationContext(), provider));

        PseudoTab realTab = PseudoTab.fromTab(mTab1);
        Assert.assertNotEquals(tab, realTab);
        Assert.assertEquals(
                title, realTab.getTitle(ContextUtils.getApplicationContext(), provider));
    }

    @Test
    public void getTitle_nullProvider() {
        PseudoTab tab = PseudoTab.fromTabId(TAB1_ID);
        Assert.assertEquals(
                tab.getTitle(), tab.getTitle(ContextUtils.getApplicationContext(), null));
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
        GURL url = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);
        doReturn(url).when(mTab1).getUrl();

        PseudoTab tab = PseudoTab.fromTabId(TAB1_ID);
        Assert.assertEquals(GURL.emptyGURL(), tab.getUrl());

        PseudoTab realTab = PseudoTab.fromTab(mTab1);
        Assert.assertNotEquals(tab, realTab);
        Assert.assertEquals(url, realTab.getUrl());
    }

    @Test
    public void getUrl_cache() {
        String url = JUnitTestGURLs.URL_1;
        TabAttributeCache.setUrlForTesting(TAB1_ID, JUnitTestGURLs.getGURL(url));

        PseudoTab tab = PseudoTab.fromTabId(TAB1_ID);
        Assert.assertEquals(url, tab.getUrl().getSpec());

        PseudoTab realTab = PseudoTab.fromTab(mTab1);
        Assert.assertNotEquals(tab, realTab);
        Assert.assertNull(realTab.getUrl());
    }

    @Test
    public void getRootId_real() {
        int rootId = 1337;
        doReturn(rootId).when(mCriticalPersistedTabData).getRootId();

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
    public void getTimestampMillis_real() {
        long timestamp = 12345;
        doReturn(timestamp).when(mCriticalPersistedTabData).getTimestampMillis();

        PseudoTab tab = PseudoTab.fromTabId(TAB1_ID);
        Assert.assertEquals(CriticalPersistedTabData.INVALID_TIMESTAMP, tab.getTimestampMillis());

        PseudoTab realTab = PseudoTab.fromTab(mTab1);
        Assert.assertNotEquals(tab, realTab);
        Assert.assertEquals(timestamp, realTab.getTimestampMillis());
    }

    @Test
    public void getTimestampMillis_cache() {
        long timestamp = 42;
        TabAttributeCache.setTimestampMillisForTesting(TAB1_ID, timestamp);

        PseudoTab tab = PseudoTab.fromTabId(TAB1_ID);
        Assert.assertEquals(timestamp, tab.getTimestampMillis());

        PseudoTab realTab = PseudoTab.fromTab(mTab1);
        Assert.assertNotEquals(tab, realTab);
        Assert.assertNotEquals(timestamp, realTab.getTimestampMillis());
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
    @DisableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @EnableFeatures({ChromeFeatureList.INSTANT_START})
    public void getRelatedTabs_noProvider_groupDisabled_single() {
        doReturn(false).when(mTabModelSelector).isTabStateInitialized();

        PseudoTab tab1 = PseudoTab.fromTabId(TAB1_ID);
        List<PseudoTab> related = PseudoTab.getRelatedTabs(
                ContextUtils.getApplicationContext(), tab1, mTabModelSelector);
        Assert.assertEquals(1, related.size());
        Assert.assertEquals(TAB1_ID, related.get(0).getId());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @EnableFeatures({ChromeFeatureList.INSTANT_START})
    public void getRelatedTabs_noProvider_groupDisabled_group() {
        doReturn(false).when(mTabModelSelector).isTabStateInitialized();

        TabAttributeCache.setRootIdForTesting(TAB1_ID, TAB1_ID);
        TabAttributeCache.setRootIdForTesting(TAB2_ID, TAB1_ID);
        PseudoTab tab1 = PseudoTab.fromTabId(TAB1_ID);
        Assert.assertEquals(TAB1_ID, tab1.getRootId());
        PseudoTab tab2 = PseudoTab.fromTabId(TAB2_ID);
        Assert.assertEquals(TAB1_ID, tab2.getRootId());

        List<PseudoTab> related = PseudoTab.getRelatedTabs(
                ContextUtils.getApplicationContext(), tab1, mTabModelSelector);
        Assert.assertEquals(1, related.size());
        Assert.assertEquals(TAB1_ID, related.get(0).getId());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID, ChromeFeatureList.INSTANT_START})
    public void getRelatedTabs_noProvider_single() {
        doReturn(false).when(mTabModelSelector).isTabStateInitialized();

        PseudoTab tab1 = PseudoTab.fromTabId(TAB1_ID);
        List<PseudoTab> related = PseudoTab.getRelatedTabs(
                ContextUtils.getApplicationContext(), tab1, mTabModelSelector);
        Assert.assertEquals(1, related.size());
        Assert.assertEquals(TAB1_ID, related.get(0).getId());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID, ChromeFeatureList.INSTANT_START})
    public void getRelatedTabs_noProvider_group() {
        doReturn(false).when(mTabModelSelector).isTabStateInitialized();

        TabAttributeCache.setRootIdForTesting(TAB1_ID, TAB1_ID);
        TabAttributeCache.setRootIdForTesting(TAB2_ID, TAB1_ID);
        PseudoTab tab1 = PseudoTab.fromTabId(TAB1_ID);
        PseudoTab.fromTabId(TAB2_ID);

        List<PseudoTab> related = PseudoTab.getRelatedTabs(
                ContextUtils.getApplicationContext(), tab1, mTabModelSelector);
        Assert.assertEquals(2, related.size());
        Assert.assertEquals(TAB1_ID, related.get(0).getId());
        Assert.assertEquals(TAB2_ID, related.get(1).getId());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID, ChromeFeatureList.INSTANT_START})
    public void getRelatedTabs_noProvider_badGroup() {
        doReturn(false).when(mTabModelSelector).isTabStateInitialized();

        TabAttributeCache.setRootIdForTesting(TAB1_ID, TAB1_ID);
        TabAttributeCache.setRootIdForTesting(TAB2_ID, Tab.INVALID_TAB_ID);
        TabAttributeCache.setRootIdForTesting(TAB3_ID, TAB3_ID);
        PseudoTab tab1 = PseudoTab.fromTabId(TAB1_ID);
        PseudoTab.fromTabId(TAB2_ID);
        PseudoTab.fromTabId(TAB3_ID);

        List<PseudoTab> related = PseudoTab.getRelatedTabs(
                ContextUtils.getApplicationContext(), tab1, mTabModelSelector);
        Assert.assertEquals(1, related.size());
        Assert.assertEquals(TAB1_ID, related.get(0).getId());
    }

    @Test
    public void getRelatedTabs_provider_normal() {
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        doReturn(mTabModelFilter).when(mTabModelFilterProvider).getTabModelFilter(eq(false));
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3));
        doReturn(tabs).when(mTabModelFilter).getRelatedTabList(TAB1_ID);

        PseudoTab tab1 = PseudoTab.fromTabId(TAB1_ID);
        List<PseudoTab> related = PseudoTab.getRelatedTabs(
                ContextUtils.getApplicationContext(), tab1, mTabModelSelector);
        Assert.assertEquals(3, related.size());
        Assert.assertEquals(TAB1_ID, related.get(0).getId());
        Assert.assertEquals(TAB2_ID, related.get(1).getId());
        Assert.assertEquals(TAB3_ID, related.get(2).getId());
    }

    @Test
    public void getRelatedTabs_provider_incognito() {
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        doReturn(mTabModelFilter).when(mTabModelFilterProvider).getTabModelFilter(eq(false));
        List<Tab> empty = new ArrayList<>();
        doReturn(empty).when(mTabModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(mTabModelFilter2).when(mTabModelFilterProvider).getTabModelFilter(eq(true));
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        doReturn(tabs).when(mTabModelFilter2).getRelatedTabList(TAB1_ID);

        PseudoTab tab1 = PseudoTab.fromTabId(TAB1_ID);
        List<PseudoTab> related = PseudoTab.getRelatedTabs(
                ContextUtils.getApplicationContext(), tab1, mTabModelSelector);
        Assert.assertEquals(2, related.size());
        Assert.assertEquals(TAB1_ID, related.get(0).getId());
        Assert.assertEquals(TAB2_ID, related.get(1).getId());
    }

    @Test
    public void testTabDestroyedTitle() {
        Tab tab = new MockTab(TAB4_ID, false);
        PseudoTab pseudoTab = PseudoTab.fromTab(tab);
        tab.destroy();
        // Title was not set. Without the isInitialized() check,
        // pseudoTab.getTitle() would crash here with
        // UnsupportedOperationException
        Assert.assertEquals("", pseudoTab.getTitle());
    }

    @Test
    public void testTabDestroyedUrl() {
        Tab tab = new MockTab(TAB4_ID, false);
        PseudoTab pseudoTab = PseudoTab.fromTab(tab);
        tab.destroy();
        // Url was not set. Without the isInitialized() check,
        // pseudoTab.getUrl() would crash here with
        // UnsupportedOperationException
        Assert.assertEquals("", pseudoTab.getUrl().getSpec());
    }

    @Test
    public void testTabDestroyedRootId() {
        Tab tab = new MockTab(TAB4_ID, false);
        PseudoTab pseudoTab = PseudoTab.fromTab(tab);
        tab.destroy();
        // Root ID was not set. Without the isInitialized() check,
        // pseudoTab.getRootId() would crash here with
        // UnsupportedOperationException
        Assert.assertEquals(Tab.INVALID_TAB_ID, pseudoTab.getRootId());
    }

    @Test
    public void testTabDestroyedTimestamp() {
        Tab tab = new MockTab(TAB4_ID, false);
        PseudoTab pseudoTab = PseudoTab.fromTab(tab);
        tab.destroy();
        // Timestamp was not set. Without the isInitialized() check,
        // pseudoTab.getTimestampMillis() would crash here with
        // UnsupportedOperationException
        Assert.assertEquals(
                CriticalPersistedTabData.INVALID_TIMESTAMP, pseudoTab.getTimestampMillis());
    }
}
