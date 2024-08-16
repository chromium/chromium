// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.mockito.Mockito.when;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Set;

/** Tests for {@link RecentlyClosedBridge} including native TabRestoreService. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_STARTUP_PROMOS
})
@Batch(Batch.PER_CLASS)
public class RecentlyClosedBridgeTest {
    private static final int MAX_ENTRY_COUNT = 5;
    private static final String TEST_PAGE_A = "/chrome/test/data/android/about.html";
    private static final String TEST_PAGE_B = "/chrome/test/data/android/google.html";
    private static final String TEST_PAGE_C = "/chrome/test/data/android/simple.html";

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, true);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private ChromeTabbedActivity mActivity;
    private TabModelSelector mTabModelSelector;
    private TabGroupModelFilter mTabGroupModelFilter;
    private TabModel mTabModel;
    private RecentlyClosedBridge mRecentlyClosedBridge;
    @Mock private SyncService mSyncService;

    @Before
    public void setUp() {
        when(mSyncService.getActiveDataTypes()).thenReturn(Set.of(DataType.SAVED_TAB_GROUP));
        SyncServiceFactory.setInstanceForTesting(mSyncService);

        sActivityTestRule.waitForActivityNativeInitializationComplete();

        // Disable snackbars from the {@link UndoBarController} which can break this test.
        sActivityTestRule.getActivity().getSnackbarManager().disableForTesting();

        mActivity = sActivityTestRule.getActivity();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRecentlyClosedBridge =
                            new RecentlyClosedBridge(
                                    ProfileManager.getLastUsedRegularProfile(),
                                    mActivity.getTabModelSelectorSupplier().get());
                    mRecentlyClosedBridge.clearRecentlyClosedEntries();
                    Assert.assertEquals(
                            0,
                            mRecentlyClosedBridge.getRecentlyClosedEntries(MAX_ENTRY_COUNT).size());
                });
        mActivity = sActivityTestRule.getActivity();
        mTabModelSelector = mActivity.getTabModelSelectorSupplier().get();
        mTabModel = mTabModelSelector.getModel(false);
        TabModelFilter filter =
                mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(false);
        mTabGroupModelFilter = (TabGroupModelFilter) filter;
        final Tab tab = mActivity.getActivityTab();
        ChromeTabUtils.waitForInteractable(tab);
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRecentlyClosedBridge.clearRecentlyClosedEntries();
                    Assert.assertEquals(
                            0,
                            mRecentlyClosedBridge.getRecentlyClosedEntries(MAX_ENTRY_COUNT).size());
                    mRecentlyClosedBridge.destroy();
                });
    }

    /** Tests opening the most recently closed tab in the foreground. */
    @Test
    @MediumTest
    public void testOpenMostRecentlyClosedEntry_Tab_InForeground() {
        final String[] urls = new String[] {getUrl(TEST_PAGE_A), getUrl(TEST_PAGE_B)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[0], /* incognito= */ false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(urls[1], /* incognito= */ false);

        final String[] titles = new String[2];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    titles[0] = tabA.getTitle();
                    titles[1] = tabB.getTitle();
                    mTabModel.closeTabs(TabClosureParams.closeTab(tabB).allowUndo(false).build());
                    mTabModel.closeTabs(TabClosureParams.closeTab(tabA).build());
                    mTabModel.commitTabClosure(tabA.getId());
                });

        final List<RecentlyClosedTab> recentTabs = new ArrayList<>();
        final int[] tabCount = new int[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tabCount[0] = mTabModel.getCount();
                    recentTabs.addAll(
                            (List<RecentlyClosedTab>)
                                    (List<? extends RecentlyClosedEntry>)
                                            mRecentlyClosedBridge.getRecentlyClosedEntries(
                                                    MAX_ENTRY_COUNT));
                    mRecentlyClosedBridge.openMostRecentlyClosedEntry(mTabModel);
                });
        // 1. Blank Tab
        Assert.assertEquals(1, tabCount[0]);

        assertTabsAre(recentTabs, titles, urls);

        // 1. Blank Tab
        // 2. tabA - restored.
        final List<Tab> tabs = getAllTabs();
        Assert.assertEquals(2, tabs.size());
        Assert.assertEquals(titles[0], ChromeTabUtils.getTitleOnUiThread(tabs.get(1)));
        Assert.assertEquals(urls[0], ChromeTabUtils.getUrlOnUiThread(tabs.get(1)).getSpec());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // tabA is launched in foreground.
                    Assert.assertNotNull(tabs.get(1).getWebContents().getRenderWidgetHostView());
                });
    }

    /** Tests opening a specific closed {@link Tab} as a new background tab. */
    @Test
    @MediumTest
    public void testOpenRecentlyClosedTab_InCurrentTab() {
        final String[] urls = new String[] {getUrl(TEST_PAGE_A), getUrl(TEST_PAGE_B)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[0], /* incognito= */ false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(urls[1], /* incognito= */ false);
        final Tab tabC =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_C), /* incognito= */ false);

        final String[] titles = new String[2];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.setIndex(mTabModel.indexOf(tabC), TabSelectionType.FROM_USER);
                    titles[0] = tabA.getTitle();
                    titles[1] = tabB.getTitle();
                    mTabModel.closeTabs(TabClosureParams.closeTab(tabB).allowUndo(false).build());
                    mTabModel.closeTabs(TabClosureParams.closeTab(tabA).allowUndo(false).build());
                });

        final List<RecentlyClosedTab> recentTabs = new ArrayList<>();
        final int[] tabCount = new int[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tabCount[0] = mTabModel.getCount();
                    recentTabs.addAll(
                            (List<RecentlyClosedTab>)
                                    (List<? extends RecentlyClosedEntry>)
                                            mRecentlyClosedBridge.getRecentlyClosedEntries(
                                                    MAX_ENTRY_COUNT));
                    mRecentlyClosedBridge.openRecentlyClosedTab(
                            mTabModel, recentTabs.get(1), WindowOpenDisposition.CURRENT_TAB);
                });
        // 1. Blank Tab
        // 2. tabC
        Assert.assertEquals(2, recentTabs.size());

        assertTabsAre(recentTabs, titles, urls);

        // 1. Blank Tab
        // 2. tabC - now TEST_PAGE_B.
        final List<Tab> tabs = getAllTabs();
        Assert.assertEquals(2, tabs.size());
        // Restored onto tab B.
        Assert.assertEquals(tabC, tabs.get(1));
        Assert.assertEquals(titles[1], ChromeTabUtils.getTitleOnUiThread(tabC));
        Assert.assertEquals(urls[1], ChromeTabUtils.getUrlOnUiThread(tabC).getSpec());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertNotNull(tabC.getWebContents());
                    // Should only have one navigation entry as it replaced TEST_PAGE_C.
                    Assert.assertEquals(
                            1,
                            tabC.getWebContents()
                                    .getNavigationController()
                                    .getNavigationHistory()
                                    .getEntryCount());

                    // Has renderer for foreground tab.
                    Assert.assertNotNull(tabC.getWebContents().getRenderWidgetHostView());
                });
    }

    /** Tests opening a specific closed {@link Tab} that was frozen as a new background tab. */
    @Test
    @MediumTest
    public void testOpenRecentlyClosedTab_Frozen_InBackground() {
        final String[] urls = new String[] {getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[0], /* incognito= */ false);
        sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_B), /* incognito= */ false);
        final Tab frozenTabA = freezeTab(tabA);
        // Clear the entry created by freezing the tab.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRecentlyClosedBridge.clearRecentlyClosedEntries();
                });

        final String[] titles = new String[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    titles[0] = frozenTabA.getTitle();
                    mTabModel.closeTabs(
                            TabClosureParams.closeTab(frozenTabA).allowUndo(false).build());
                });

        final List<RecentlyClosedTab> recentTabs = new ArrayList<>();
        final int[] tabCount = new int[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tabCount[0] = mTabModel.getCount();
                    recentTabs.addAll(
                            (List<RecentlyClosedTab>)
                                    (List<? extends RecentlyClosedEntry>)
                                            mRecentlyClosedBridge.getRecentlyClosedEntries(
                                                    MAX_ENTRY_COUNT));
                    mRecentlyClosedBridge.openRecentlyClosedTab(
                            mTabModel, recentTabs.get(0), WindowOpenDisposition.NEW_BACKGROUND_TAB);
                });
        // 1. Blank Tab
        // 2. tabB
        Assert.assertEquals(2, tabCount[0]);

        assertTabsAre(recentTabs, titles, urls);

        // 1. Blank Tab
        // 2. tabB
        // 3. tabA - restored.
        final List<Tab> tabs = getAllTabs();
        Assert.assertEquals(3, tabs.size());
        Assert.assertEquals(titles[0], ChromeTabUtils.getTitleOnUiThread(tabs.get(2)));
        Assert.assertEquals(urls[0], ChromeTabUtils.getUrlOnUiThread(tabs.get(2)).getSpec());
    }

    /**
     * Tests opening a specific closed {@link Tab} that was closed as part of a bulk closure
     * replacing the current tab.
     */
    @Test
    @MediumTest
    public void testOpenRecentlyClosedTab_FromBulkClosure_InNewTab() {
        // Tab order is inverted in RecentlyClosedEntry as most recent comes first so log data in
        // reverse.
        final String[] urls = new String[] {getUrl(TEST_PAGE_B), getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[1], /* incognito= */ false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(urls[0], /* incognito= */ false);

        final String[] titles = new String[2];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    titles[1] = tabA.getTitle();
                    titles[0] = tabB.getTitle();
                    mTabModel.closeTabs(TabClosureParams.closeAllTabs().build());
                    mTabModel.commitAllTabClosures();
                });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(0, tabCount);
        Assert.assertEquals(1, recentEntries.size());
        assertEntryIs(
                recentEntries.get(0), RecentlyClosedBulkEvent.class, new String[0], titles, urls);

        final RecentlyClosedBulkEvent event = (RecentlyClosedBulkEvent) recentEntries.get(0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRecentlyClosedBridge.openRecentlyClosedTab(
                            mTabModel,
                            event.getTabs().get(1),
                            WindowOpenDisposition.NEW_FOREGROUND_TAB);
                });

        // 1. tabA - new restored.
        final List<Tab> tabs = getAllTabs();
        Assert.assertEquals(1, tabs.size());
        Assert.assertEquals(titles[1], ChromeTabUtils.getTitleOnUiThread(tabs.get(0)));
        Assert.assertEquals(urls[1], ChromeTabUtils.getUrlOnUiThread(tabs.get(0)).getSpec());
    }

    /**
     * Tests opening a specific closed {@link Tab} that was closed as part of a group in a bulk
     * closure as a new background tab.
     */
    @Test
    @MediumTest
    public void testOpenRecentlyClosedTab_FromGroupInBulkClosure_InBackgroundTab() {
        if (mTabGroupModelFilter == null) return;

        // Tab order is inverted in RecentlyClosedEntry as most recent comes first so log data in
        // reverse.
        final String[] urls =
                new String[] {getUrl(TEST_PAGE_C), getUrl(TEST_PAGE_B), getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[2], /* incognito= */ false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(urls[1], /* incognito= */ false);
        final Tab tabC = sActivityTestRule.loadUrlInNewTab(urls[0], /* incognito= */ false);

        final String[] titles = new String[3];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.mergeTabsToGroup(tabB.getId(), tabA.getId());
                    titles[2] = tabA.getTitle();
                    titles[1] = tabB.getTitle();
                    titles[0] = tabC.getTitle();
                    mTabModel.closeTabs(
                            TabClosureParams.closeTabs(List.of(tabA, tabB, tabC))
                                    .hideTabGroups(true)
                                    .build());
                    mTabModel.commitAllTabClosures();
                });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(1, tabCount);
        Assert.assertEquals(1, recentEntries.size());
        assertEntryIs(
                recentEntries.get(0),
                RecentlyClosedBulkEvent.class,
                new String[] {""},
                titles,
                urls);

        final RecentlyClosedBulkEvent event = (RecentlyClosedBulkEvent) recentEntries.get(0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Tab order is inverted as most recent comes first so pick the last tab to
                    // restore ==
                    // tabB.
                    mRecentlyClosedBridge.openRecentlyClosedTab(
                            mTabModel,
                            event.getTabs().get(1),
                            WindowOpenDisposition.NEW_BACKGROUND_TAB);
                });

        // 1. Blank tab
        // 2. Restored tabB
        final List<Tab> tabs = getAllTabs();
        Assert.assertEquals(2, tabs.size());
        Assert.assertEquals(titles[1], ChromeTabUtils.getTitleOnUiThread(tabs.get(1)));
        Assert.assertEquals(urls[1], ChromeTabUtils.getUrlOnUiThread(tabs.get(1)).getSpec());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(mTabGroupModelFilter.isTabInTabGroup(tabs.get(1)));
                });
    }

    /**
     * Tests opening a specific closed {@link Tab} that was closed as part of a group replacing the
     * current tab.
     */
    @Test
    @MediumTest
    public void testOpenRecentlyClosedTab_FromGroupClosure_InCurrentTab() {
        if (mTabGroupModelFilter == null) return;

        // Tab order is inverted in when closing.
        final String[] urls = new String[] {getUrl(TEST_PAGE_A), getUrl(TEST_PAGE_B)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[0], /* incognito= */ false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(urls[1], /* incognito= */ false);

        final String[] titles = new String[2];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.mergeTabsToGroup(tabB.getId(), tabA.getId());
                    titles[0] = tabA.getTitle();
                    titles[1] = tabB.getTitle();
                    mTabModel.closeTabs(
                            TabClosureParams.closeTabs(List.of(tabB, tabA))
                                    .allowUndo(false)
                                    .hideTabGroups(true)
                                    .build());
                });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(1, tabCount);
        Assert.assertEquals(1, recentEntries.size());
        assertEntryIs(
                recentEntries.get(0), RecentlyClosedGroup.class, new String[] {""}, titles, urls);

        final RecentlyClosedGroup group = (RecentlyClosedGroup) recentEntries.get(0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRecentlyClosedBridge.openRecentlyClosedTab(
                            mTabModel, group.getTabs().get(1), WindowOpenDisposition.CURRENT_TAB);
                });

        // 1. tabA restored over blank tab.
        final List<Tab> tabs = getAllTabs();
        Assert.assertEquals(1, tabs.size());
        Assert.assertEquals(titles[1], ChromeTabUtils.getTitleOnUiThread(tabs.get(0)));
        Assert.assertEquals(urls[1], ChromeTabUtils.getUrlOnUiThread(tabs.get(0)).getSpec());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(mTabGroupModelFilter.isTabInTabGroup(tabs.get(0)));
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRecentlyClosedBridge.openRecentlyClosedTab(
                            mTabModel,
                            group.getTabs().get(0),
                            WindowOpenDisposition.NEW_BACKGROUND_TAB);
                });

        // 1. tabA restored over blank tab.
        // 2. tabB restored.
        tabs.clear();
        tabs.addAll(getAllTabs());
        Assert.assertEquals(2, tabs.size());
        Assert.assertEquals(titles[0], ChromeTabUtils.getTitleOnUiThread(tabs.get(1)));
        Assert.assertEquals(urls[0], ChromeTabUtils.getUrlOnUiThread(tabs.get(1)).getSpec());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(mTabGroupModelFilter.isTabInTabGroup(tabs.get(1)));
                });
    }

    /** Tests opening a specific closed {@link Tab} that was closed not as the most recent entry. */
    @Test
    @MediumTest
    public void testOpenRecentlyClosedEntry_Tab_FromMultipleTabs_SingleTabGroup() {
        if (mTabGroupModelFilter == null) return;

        final String[] urlA = new String[] {getUrl(TEST_PAGE_A)};
        final String[] urlB = new String[] {getUrl(TEST_PAGE_B)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urlA[0], /* incognito= */ false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(urlB[0], /* incognito= */ false);

        final String[] titleA = new String[1];
        final String[] titleB = new String[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.mergeTabsToGroup(tabB.getId(), tabA.getId());
                    titleA[0] = tabA.getTitle();
                    titleB[0] = tabB.getTitle();
                    mTabModel.closeTabs(TabClosureParams.closeTab(tabB).build());
                    mTabModel.closeTabs(TabClosureParams.closeTab(tabA).build());
                    mTabModel.commitAllTabClosures();
                });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(1, tabCount);
        Assert.assertEquals(2, recentEntries.size());
        assertEntryIs(
                recentEntries.get(0), RecentlyClosedGroup.class, new String[] {""}, titleA, urlA);
        assertEntryIs(recentEntries.get(1), RecentlyClosedTab.class, new String[0], titleB, urlB);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRecentlyClosedBridge.openRecentlyClosedEntry(mTabModel, recentEntries.get(1));
                });

        // 1. Blank tab
        // 2. tabB restored in new tab.
        final List<Tab> tabs = getAllTabs();
        Assert.assertEquals(2, tabs.size());
        Assert.assertEquals(titleB[0], ChromeTabUtils.getTitleOnUiThread(tabs.get(1)));
        Assert.assertEquals(urlB[0], ChromeTabUtils.getUrlOnUiThread(tabs.get(1)).getSpec());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(mTabGroupModelFilter.isTabInTabGroup(tabs.get(1)));
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRecentlyClosedBridge.openRecentlyClosedEntry(mTabModel, recentEntries.get(0));
                });

        // 1. Blank tab
        // 2. tabB restored in new tab.
        // 3. tabA restored in new tab as group.
        tabs.clear();
        tabs.addAll(getAllTabs());
        Assert.assertEquals(3, tabs.size());
        Assert.assertEquals(titleA[0], ChromeTabUtils.getTitleOnUiThread(tabs.get(2)));
        Assert.assertEquals(urlA[0], ChromeTabUtils.getUrlOnUiThread(tabs.get(2)).getSpec());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(mTabGroupModelFilter.isTabInTabGroup(tabs.get(2)));
                });
    }

    /**
     * Tests opening a specific closed {@link Tab} that was closed as part of a group. There is no
     * UI that currently facilitates this flow. A Group or Bulk closure is either all restored or
     * not.
     */
    @Test
    @MediumTest
    public void testOpenRecentlyClosedEntry_Tab_FromGroupClosure() {
        if (mTabGroupModelFilter == null) return;

        // Tab order is inverted in RecentlyClosedEntry as most recent comes first so log data in
        // reverse.
        final String[] urls = new String[] {getUrl(TEST_PAGE_B), getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[1], /* incognito= */ false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(urls[0], /* incognito= */ false);

        final String[] titles = new String[2];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.mergeTabsToGroup(tabB.getId(), tabA.getId());
                    titles[1] = tabA.getTitle();
                    titles[0] = tabB.getTitle();
                    mTabModel.closeTabs(
                            TabClosureParams.closeTabs(List.of(tabA, tabB))
                                    .hideTabGroups(true)
                                    .build());
                    mTabModel.commitTabClosure(tabA.getId());
                    mTabModel.commitTabClosure(tabB.getId());
                });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(1, tabCount);
        Assert.assertEquals(1, recentEntries.size());
        assertEntryIs(
                recentEntries.get(0), RecentlyClosedGroup.class, new String[] {""}, titles, urls);

        final RecentlyClosedGroup group = (RecentlyClosedGroup) recentEntries.get(0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Tab order is inverted as most recent comes first so pick the last tab to
                    // restore == tabA.
                    mRecentlyClosedBridge.openRecentlyClosedEntry(
                            mTabModel, group.getTabs().get(1));
                });

        // 1. Blank tab
        // 2. tabA restored in new tab in a group.
        final List<Tab> tabs = getAllTabs();
        Assert.assertEquals(2, tabs.size());
        Assert.assertEquals(titles[1], ChromeTabUtils.getTitleOnUiThread(tabs.get(1)));
        Assert.assertEquals(urls[1], ChromeTabUtils.getUrlOnUiThread(tabs.get(1)).getSpec());

        // This behavior mirrors desktop.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(mTabGroupModelFilter.isTabInTabGroup(tabs.get(1)));
                });
    }

    /** Tests opening a specific closed {@link Tab} that was closed as part of a group. */
    @Test
    @MediumTest
    public void testOpenRecentlyClosedEntry_SingleRemainingTabInGroupAsGroup_FromGroupClosure() {
        if (mTabGroupModelFilter == null) return;

        // Tab order is inverted in RecentlyClosedEntry as most recent comes first so log data in
        // reverse.
        final String[] urls = new String[] {getUrl(TEST_PAGE_B), getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[1], /* incognito= */ false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(urls[0], /* incognito= */ false);

        final String[] titles = new String[2];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.mergeTabsToGroup(tabB.getId(), tabA.getId());
                    titles[1] = tabA.getTitle();
                    titles[0] = tabB.getTitle();
                    mTabModel.closeTabs(
                            TabClosureParams.closeTabs(List.of(tabA, tabB))
                                    .hideTabGroups(true)
                                    .build());
                    mTabModel.commitTabClosure(tabA.getId());
                    mTabModel.commitTabClosure(tabB.getId());
                });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(1, tabCount);
        Assert.assertEquals(1, recentEntries.size());
        assertEntryIs(
                recentEntries.get(0), RecentlyClosedGroup.class, new String[] {""}, titles, urls);

        final RecentlyClosedGroup group = (RecentlyClosedGroup) recentEntries.get(0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Tab order is inverted as most recent comes first so pick the last tab to
                    // restore ==
                    // tabA.
                    mRecentlyClosedBridge.openRecentlyClosedEntry(
                            mTabModel, group.getTabs().get(1));
                });

        // 1. Blank tab
        // 2. tabA restored in new tab.
        final List<Tab> tabs = getAllTabs();
        Assert.assertEquals(2, tabs.size());
        Assert.assertEquals(titles[1], ChromeTabUtils.getTitleOnUiThread(tabs.get(1)));
        Assert.assertEquals(urls[1], ChromeTabUtils.getUrlOnUiThread(tabs.get(1)).getSpec());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(mTabGroupModelFilter.isTabInTabGroup(tabs.get(1)));
                });
    }

    /** Tests opening a specific closed group. */
    @Test
    @MediumTest
    public void testOpenRecentlyClosedEntry_Group_FromGroupClosure() {
        if (mTabGroupModelFilter == null) return;

        // Tab order is inverted in RecentlyClosedEntry as most recent comes first so log data in
        // reverse.
        final String[] urls = new String[] {getUrl(TEST_PAGE_B), getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[1], /* incognito= */ false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(urls[0], /* incognito= */ false);

        final String[] titles = new String[2];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.mergeTabsToGroup(tabB.getId(), tabA.getId());
                    mTabGroupModelFilter.setTabGroupTitle(tabA.getId(), "Bar");
                    titles[1] = tabA.getTitle();
                    titles[0] = tabB.getTitle();
                    mTabModel.closeTabs(
                            TabClosureParams.closeTabs(List.of(tabA, tabB))
                                    .allowUndo(false)
                                    .hideTabGroups(true)
                                    .build());
                });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(1, tabCount);
        Assert.assertEquals(1, recentEntries.size());
        assertEntryIs(
                recentEntries.get(0),
                RecentlyClosedGroup.class,
                new String[] {"Bar"},
                titles,
                urls);

        final RecentlyClosedGroup group = (RecentlyClosedGroup) recentEntries.get(0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRecentlyClosedBridge.openRecentlyClosedEntry(mTabModel, group);
                });

        // 1. Blank tab
        // 2. tabA restored in new tab.
        // 3. tabB restored in new tab.
        final List<Tab> tabs = getAllTabs();
        Assert.assertEquals(3, tabs.size());
        Assert.assertEquals(titles[1], ChromeTabUtils.getTitleOnUiThread(tabs.get(1)));
        Assert.assertEquals(urls[1], ChromeTabUtils.getUrlOnUiThread(tabs.get(1)).getSpec());
        Assert.assertEquals(titles[0], ChromeTabUtils.getTitleOnUiThread(tabs.get(2)));
        Assert.assertEquals(urls[0], ChromeTabUtils.getUrlOnUiThread(tabs.get(2)).getSpec());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            "Bar", mTabGroupModelFilter.getTabGroupTitle(tabs.get(1).getId()));
                    Assert.assertTrue(mTabGroupModelFilter.isTabInTabGroup(tabs.get(1)));
                    Assert.assertEquals(
                            Arrays.asList(new Tab[] {tabs.get(1), tabs.get(2)}),
                            mTabGroupModelFilter.getRelatedTabList(tabs.get(1).getId()));
                });
    }

    /** Tests opening a tabs that are a subset of a group. */
    @Test
    @MediumTest
    public void testOpenRecentlyClosedEntry_SubsetOfTabs_FromGroupSubsetClosure_NotUndoable() {
        if (mTabGroupModelFilter == null) return;

        // Tab order is inverted in RecentlyClosedEntry as most recent comes first so log data in
        // reverse.
        final String[] urls = new String[] {getUrl(TEST_PAGE_C), getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[1], /* incognito= */ false);
        final Tab tabB =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_B), /* incognito= */ false);
        final Tab tabC = sActivityTestRule.loadUrlInNewTab(urls[0], /* incognito= */ false);

        final String[] titles = new String[2];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.mergeTabsToGroup(tabB.getId(), tabA.getId());
                    mTabGroupModelFilter.mergeTabsToGroup(tabC.getId(), tabA.getId());
                    mTabGroupModelFilter.setTabGroupTitle(tabA.getId(), "Bar");
                    titles[1] = tabA.getTitle();
                    titles[0] = tabC.getTitle();
                    mTabModel.closeTabs(
                            TabClosureParams.closeTabs(List.of(tabA, tabC))
                                    .allowUndo(false)
                                    .hideTabGroups(true)
                                    .build());
                });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(2, tabCount);
        Assert.assertEquals(1, recentEntries.size());
        assertEntryIs(
                recentEntries.get(0), RecentlyClosedBulkEvent.class, new String[] {}, titles, urls);

        final RecentlyClosedBulkEvent bulkEvent = (RecentlyClosedBulkEvent) recentEntries.get(0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRecentlyClosedBridge.openRecentlyClosedEntry(mTabModel, bulkEvent);
                });

        // 1. Blank tab
        // 2. tabB
        // 3. tabA restored in new tab.
        // 4. tabC restored in new tab.
        final List<Tab> tabs = getAllTabs();
        Assert.assertEquals(4, tabs.size());
        Assert.assertEquals(titles[1], ChromeTabUtils.getTitleOnUiThread(tabs.get(2)));
        Assert.assertEquals(urls[1], ChromeTabUtils.getUrlOnUiThread(tabs.get(2)).getSpec());
        Assert.assertEquals(titles[0], ChromeTabUtils.getTitleOnUiThread(tabs.get(3)));
        Assert.assertEquals(urls[0], ChromeTabUtils.getUrlOnUiThread(tabs.get(3)).getSpec());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(mTabGroupModelFilter.isTabInTabGroup(tabs.get(1)));
                    Assert.assertFalse(mTabGroupModelFilter.isTabInTabGroup(tabs.get(2)));
                    Assert.assertFalse(mTabGroupModelFilter.isTabInTabGroup(tabs.get(3)));
                });
    }

    /** Tests opening a tabs that are a subset of a group. */
    @Test
    @MediumTest
    public void testOpenRecentlyClosedEntry_SubsetOfTabs_FromGroupSubsetClosure_Undoable() {
        if (mTabGroupModelFilter == null) return;

        // Tab order is inverted in RecentlyClosedEntry as most recent comes first so log data in
        // reverse.
        final String[] urls = new String[] {getUrl(TEST_PAGE_C), getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[1], /* incognito= */ false);
        final Tab tabB =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_B), /* incognito= */ false);
        final Tab tabC = sActivityTestRule.loadUrlInNewTab(urls[0], /* incognito= */ false);

        final String[] titles = new String[2];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.mergeTabsToGroup(tabB.getId(), tabA.getId());
                    mTabGroupModelFilter.mergeTabsToGroup(tabC.getId(), tabA.getId());
                    mTabGroupModelFilter.setTabGroupTitle(tabA.getId(), "Bar");
                    titles[1] = tabA.getTitle();
                    titles[0] = tabC.getTitle();
                    mTabModel.closeTabs(
                            TabClosureParams.closeTabs(List.of(tabA, tabC))
                                    .hideTabGroups(true)
                                    .build());
                    mTabModel.commitTabClosure(tabA.getId());
                    mTabModel.commitTabClosure(tabC.getId());
                });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(2, tabCount);
        Assert.assertEquals(1, recentEntries.size());
        assertEntryIs(
                recentEntries.get(0), RecentlyClosedBulkEvent.class, new String[] {}, titles, urls);

        final RecentlyClosedBulkEvent bulkEvent = (RecentlyClosedBulkEvent) recentEntries.get(0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRecentlyClosedBridge.openRecentlyClosedEntry(mTabModel, bulkEvent);
                });

        // 1. Blank tab
        // 2. tabB
        // 3. tabA restored in new tab.
        // 4. tabC restored in new tab.
        final List<Tab> tabs = getAllTabs();
        Assert.assertEquals(4, tabs.size());
        Assert.assertEquals(titles[1], ChromeTabUtils.getTitleOnUiThread(tabs.get(2)));
        Assert.assertEquals(urls[1], ChromeTabUtils.getUrlOnUiThread(tabs.get(2)).getSpec());
        Assert.assertEquals(titles[0], ChromeTabUtils.getTitleOnUiThread(tabs.get(3)));
        Assert.assertEquals(urls[0], ChromeTabUtils.getUrlOnUiThread(tabs.get(3)).getSpec());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(mTabGroupModelFilter.isTabInTabGroup(tabs.get(1)));
                    Assert.assertFalse(mTabGroupModelFilter.isTabInTabGroup(tabs.get(2)));
                    Assert.assertFalse(mTabGroupModelFilter.isTabInTabGroup(tabs.get(3)));
                });
    }

    /** Tests opening a tab that is a subset of a group. */
    @Test
    @MediumTest
    public void testOpenRecentlyClosedEntry_SingleTab_FromGroupSubsetClosure_Undoable() {
        if (mTabGroupModelFilter == null) return;

        // Tab order is inverted in RecentlyClosedEntry as most recent comes first so log data in
        // reverse.
        final String[] urls = new String[] {getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[0], /* incognito= */ false);
        final Tab tabB =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_B), /* incognito= */ false);

        final String[] titles = new String[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.mergeTabsToGroup(tabB.getId(), tabA.getId());
                    mTabGroupModelFilter.setTabGroupTitle(tabA.getId(), "Bar");
                    titles[0] = tabA.getTitle();
                    mTabModel.closeTabs(
                            TabClosureParams.closeTabs(List.of(tabA)).hideTabGroups(true).build());
                    mTabModel.commitTabClosure(tabA.getId());
                });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(2, tabCount);
        Assert.assertEquals(1, recentEntries.size());
        assertEntryIs(recentEntries.get(0), RecentlyClosedTab.class, new String[] {}, titles, urls);

        final RecentlyClosedTab recentTab = (RecentlyClosedTab) recentEntries.get(0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRecentlyClosedBridge.openRecentlyClosedEntry(mTabModel, recentTab);
                });

        // 1. Blank tab
        // 2. tabB
        // 3. tabA restored in new tab.
        final List<Tab> tabs = getAllTabs();
        Assert.assertEquals(3, tabs.size());
        Assert.assertEquals(titles[0], ChromeTabUtils.getTitleOnUiThread(tabs.get(2)));
        Assert.assertEquals(urls[0], ChromeTabUtils.getUrlOnUiThread(tabs.get(2)).getSpec());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(mTabGroupModelFilter.isTabInTabGroup(tabs.get(1)));
                    Assert.assertFalse(mTabGroupModelFilter.isTabInTabGroup(tabs.get(2)));
                });
    }

    /** Tests opening a specific closed single tab group that is not undoable. */
    @Test
    @MediumTest
    public void testOpenRecentlyClosedEntry_SingleTabGroupSupported_FromGroupClosure_NotUndoable() {
        if (mTabGroupModelFilter == null) return;

        final String[] urls = new String[] {getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[0], /* incognito= */ false);

        final String[] titles = new String[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.createSingleTabGroup(tabA, /* notify= */ false);
                    mTabGroupModelFilter.setTabGroupTitle(tabA.getId(), "Bar");
                    titles[0] = tabA.getTitle();
                    mTabModel.closeTabs(
                            TabClosureParams.closeTabs(List.of(tabA))
                                    .allowUndo(false)
                                    .hideTabGroups(true)
                                    .build());
                });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(1, tabCount);
        Assert.assertEquals(1, recentEntries.size());
        assertEntryIs(
                recentEntries.get(0),
                RecentlyClosedGroup.class,
                new String[] {"Bar"},
                titles,
                urls);

        final RecentlyClosedGroup group = (RecentlyClosedGroup) recentEntries.get(0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRecentlyClosedBridge.openRecentlyClosedEntry(mTabModel, group);
                });

        // 1. Blank tab
        // 2. tabA restored in new tab group.
        final List<Tab> tabs = getAllTabs();
        Assert.assertEquals(2, tabs.size());
        Assert.assertEquals(titles[0], ChromeTabUtils.getTitleOnUiThread(tabs.get(1)));
        Assert.assertEquals(urls[0], ChromeTabUtils.getUrlOnUiThread(tabs.get(1)).getSpec());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            "Bar", mTabGroupModelFilter.getTabGroupTitle(tabs.get(1).getId()));
                    Assert.assertTrue(mTabGroupModelFilter.isTabInTabGroup(tabs.get(1)));
                });
    }

    /** Tests opening a specific closed single tab group. */
    @Test
    @MediumTest
    public void testOpenRecentlyClosedEntry_SingleTabGroupSupported_FromGroupClosure_Undoable() {
        if (mTabGroupModelFilter == null) return;

        final String[] urls = new String[] {getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[0], /* incognito= */ false);

        final String[] titles = new String[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.createSingleTabGroup(tabA, /* notify= */ false);
                    mTabGroupModelFilter.setTabGroupTitle(tabA.getId(), "Bar");
                    titles[0] = tabA.getTitle();
                    mTabModel.closeTabs(
                            TabClosureParams.closeTabs(List.of(tabA)).hideTabGroups(true).build());
                    mTabModel.commitTabClosure(tabA.getId());
                });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(1, tabCount);
        Assert.assertEquals(1, recentEntries.size());
        assertEntryIs(
                recentEntries.get(0),
                RecentlyClosedGroup.class,
                new String[] {"Bar"},
                titles,
                urls);

        final RecentlyClosedGroup group = (RecentlyClosedGroup) recentEntries.get(0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRecentlyClosedBridge.openRecentlyClosedEntry(mTabModel, group);
                });

        // 1. Blank tab
        // 2. tabA restored in new tab.
        final List<Tab> tabs = getAllTabs();
        Assert.assertEquals(2, tabs.size());
        Assert.assertEquals(titles[0], ChromeTabUtils.getTitleOnUiThread(tabs.get(1)));
        Assert.assertEquals(urls[0], ChromeTabUtils.getUrlOnUiThread(tabs.get(1)).getSpec());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            "Bar", mTabGroupModelFilter.getTabGroupTitle(tabs.get(1).getId()));
                    Assert.assertTrue(mTabGroupModelFilter.isTabInTabGroup(tabs.get(1)));
                });
    }

    /** Tests a hiding tab group is not saved when undoable. */
    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_SYNC_ANDROID})
    public void testNoRecentlyClosedEntry_ForHidingTabGroup_Undoable() {
        if (mTabGroupModelFilter == null) return;

        final String[] urls = new String[] {getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[0], /* incognito= */ false);

        final String[] titles = new String[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.createSingleTabGroup(tabA, /* notify= */ false);
                    mTabGroupModelFilter.setTabGroupTitle(tabA.getId(), "Bar");
                    titles[0] = tabA.getTitle();
                    mTabGroupModelFilter.closeTabs(
                            TabClosureParams.closeTabs(List.of(tabA)).hideTabGroups(true).build());
                    mTabModel.commitTabClosure(tabA.getId());
                });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(1, tabCount);
        Assert.assertEquals(0, recentEntries.size());
    }

    /** Tests a hiding tab group is not saved when not undoable. */
    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_SYNC_ANDROID})
    public void testNoRecentlyClosedEntry_ForHidingTabGroup_NotUndoable() {
        if (mTabGroupModelFilter == null) return;

        final String[] urls = new String[] {getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[0], /* incognito= */ false);

        final String[] titles = new String[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.createSingleTabGroup(tabA, /* notify= */ false);
                    mTabGroupModelFilter.setTabGroupTitle(tabA.getId(), "Bar");
                    titles[0] = tabA.getTitle();
                    mTabGroupModelFilter.closeTabs(
                            TabClosureParams.closeTabs(List.of(tabA))
                                    .allowUndo(false)
                                    .hideTabGroups(true)
                                    .build());
                });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(1, tabCount);
        Assert.assertEquals(0, recentEntries.size());
    }

    /** Tests opening a specific closed group and that it persists across restarts. */
    @Test
    @LargeTest
    public void testOpenRecentlyClosedEntry_Group_FromGroupClosure_WithRestart() {
        if (mTabGroupModelFilter == null) return;

        // Tab order is inverted in RecentlyClosedEntry as most recent comes first so log data in
        // reverse.
        final String[] urls =
                new String[] {getUrl(TEST_PAGE_C), getUrl(TEST_PAGE_B), getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[2], /* incognito= */ false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(urls[1], /* incognito= */ false);
        final Tab tabC = sActivityTestRule.loadUrlInNewTab(urls[0], /* incognito= */ false);

        final String[] titles = new String[3];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.mergeTabsToGroup(tabB.getId(), tabA.getId());
                    mTabGroupModelFilter.mergeTabsToGroup(tabC.getId(), tabA.getId());
                    mTabGroupModelFilter.setTabGroupTitle(tabA.getId(), "Bar");
                    titles[2] = tabA.getTitle();
                    titles[1] = tabB.getTitle();
                    titles[0] = tabC.getTitle();
                    mTabModel.closeTabs(
                            TabClosureParams.closeTabs(List.of(tabA, tabB, tabC))
                                    .allowUndo(false)
                                    .hideTabGroups(true)
                                    .build());
                });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(1, tabCount);
        Assert.assertEquals(1, recentEntries.size());
        assertEntryIs(
                recentEntries.get(0),
                RecentlyClosedGroup.class,
                new String[] {"Bar"},
                titles,
                urls);

        final RecentlyClosedGroup group = (RecentlyClosedGroup) recentEntries.get(0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRecentlyClosedBridge.openRecentlyClosedEntry(mTabModel, group);
                });

        // 1. Blank tab
        // 2. tabA restored in new tab.
        // 3. tabB restored in new tab.
        // 4. tabC restored in new tab.
        final List<Tab> tabs = getAllTabs();
        Assert.assertEquals(4, tabs.size());
        Assert.assertEquals(titles[2], ChromeTabUtils.getTitleOnUiThread(tabs.get(1)));
        Assert.assertEquals(urls[2], ChromeTabUtils.getUrlOnUiThread(tabs.get(1)).getSpec());
        Assert.assertEquals(titles[1], ChromeTabUtils.getTitleOnUiThread(tabs.get(2)));
        Assert.assertEquals(urls[1], ChromeTabUtils.getUrlOnUiThread(tabs.get(2)).getSpec());
        Assert.assertEquals(titles[0], ChromeTabUtils.getTitleOnUiThread(tabs.get(3)));
        Assert.assertEquals(urls[0], ChromeTabUtils.getUrlOnUiThread(tabs.get(3)).getSpec());
        final int[] tabIds =
                new int[] {tabs.get(1).getId(), tabs.get(2).getId(), tabs.get(3).getId()};
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            "Bar", mTabGroupModelFilter.getTabGroupTitle(tabs.get(1).getId()));
                    Assert.assertTrue(mTabGroupModelFilter.isTabInTabGroup(tabs.get(1)));
                    Assert.assertTrue(mTabGroupModelFilter.isTabInTabGroup(tabs.get(2)));
                    Assert.assertTrue(mTabGroupModelFilter.isTabInTabGroup(tabs.get(3)));
                    Assert.assertEquals(
                            Arrays.asList(new Tab[] {tabs.get(1), tabs.get(2), tabs.get(3)}),
                            mTabGroupModelFilter.getRelatedTabList(tabs.get(1).getId()));
                });

        // Restart activity.
        restartActivity();

        // Confirm the same tabs are present with the same group structure.
        tabs.clear();
        tabs.addAll(getAllTabs());
        Assert.assertEquals(4, tabs.size());
        Assert.assertEquals(tabIds[0], tabs.get(1).getId());
        Assert.assertEquals(titles[2], ChromeTabUtils.getTitleOnUiThread(tabs.get(1)));
        Assert.assertEquals(urls[2], ChromeTabUtils.getUrlOnUiThread(tabs.get(1)).getSpec());
        Assert.assertEquals(tabIds[1], tabs.get(2).getId());
        Assert.assertEquals(titles[1], ChromeTabUtils.getTitleOnUiThread(tabs.get(2)));
        Assert.assertEquals(urls[1], ChromeTabUtils.getUrlOnUiThread(tabs.get(2)).getSpec());
        Assert.assertEquals(tabIds[2], tabs.get(3).getId());
        Assert.assertEquals(titles[0], ChromeTabUtils.getTitleOnUiThread(tabs.get(3)));
        Assert.assertEquals(urls[0], ChromeTabUtils.getUrlOnUiThread(tabs.get(3)).getSpec());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            "Bar", mTabGroupModelFilter.getTabGroupTitle(tabs.get(1).getId()));
                    Assert.assertTrue(mTabGroupModelFilter.isTabInTabGroup(tabs.get(1)));
                    Assert.assertTrue(mTabGroupModelFilter.isTabInTabGroup(tabs.get(2)));
                    Assert.assertTrue(mTabGroupModelFilter.isTabInTabGroup(tabs.get(3)));
                    Assert.assertEquals(
                            Arrays.asList(new Tab[] {tabs.get(1), tabs.get(2), tabs.get(3)}),
                            mTabGroupModelFilter.getRelatedTabList(tabs.get(1).getId()));
                });
    }

    /** Tests opening a specific closed {@link Tab} that was closed as part of a bulk closure. */
    @Test
    @MediumTest
    public void testOpenRecentlyClosedEntry_Tab_FromBulkClosure() {
        if (mTabGroupModelFilter == null) return;

        // Tab order is inverted in RecentlyClosedEntry as most recent comes first so log data in
        // reverse.
        final String[] urls =
                new String[] {getUrl(TEST_PAGE_C), getUrl(TEST_PAGE_B), getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[2], /* incognito= */ false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(urls[1], /* incognito= */ false);
        final Tab tabC = sActivityTestRule.loadUrlInNewTab(urls[0], /* incognito= */ false);

        final String[] titles = new String[3];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.mergeTabsToGroup(tabB.getId(), tabA.getId());
                    titles[2] = tabA.getTitle();
                    titles[1] = tabB.getTitle();
                    titles[0] = tabC.getTitle();
                    mTabModel.closeTabs(
                            TabClosureParams.closeTabs(List.of(tabA, tabB, tabC))
                                    .hideTabGroups(true)
                                    .build());
                    mTabModel.commitAllTabClosures();
                });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(1, tabCount);
        Assert.assertEquals(1, recentEntries.size());
        assertEntryIs(
                recentEntries.get(0),
                RecentlyClosedBulkEvent.class,
                new String[] {""},
                titles,
                urls);

        final RecentlyClosedBulkEvent event = (RecentlyClosedBulkEvent) recentEntries.get(0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Tab order is inverted as most recent comes first so pick the last tab to
                    // restore ==
                    // tabC.
                    mRecentlyClosedBridge.openRecentlyClosedEntry(
                            mTabModel, event.getTabs().get(0));
                });

        // 1. Blank tab
        // 2. Restored tabC
        final List<Tab> tabs = getAllTabs();
        Assert.assertEquals(2, tabs.size());
        Assert.assertEquals(titles[0], ChromeTabUtils.getTitleOnUiThread(tabs.get(1)));
        Assert.assertEquals(urls[0], ChromeTabUtils.getUrlOnUiThread(tabs.get(1)).getSpec());
    }

    /** Tests opening a specific closed bulk closure. */
    @Test
    @MediumTest
    public void testOpenRecentlyClosedEntry_Bulk_FromBulkClosure() {
        if (mTabGroupModelFilter == null) return;

        // Tab order is inverted in RecentlyClosedEntry as most recent comes first so log data in
        // reverse.
        final String[] urls =
                new String[] {getUrl(TEST_PAGE_C), getUrl(TEST_PAGE_B), getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[2], /* incognito= */ false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(urls[1], /* incognito= */ false);
        final Tab tabC = sActivityTestRule.loadUrlInNewTab(urls[0], /* incognito= */ false);

        final String[] titles = new String[3];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.mergeTabsToGroup(tabB.getId(), tabA.getId());
                    mTabGroupModelFilter.setTabGroupTitle(tabA.getId(), "Foo");
                    titles[2] = tabA.getTitle();
                    titles[1] = tabB.getTitle();
                    titles[0] = tabC.getTitle();
                    mTabModel.closeTabs(
                            TabClosureParams.closeTabs(List.of(tabA, tabB, tabC))
                                    .hideTabGroups(true)
                                    .build());
                    mTabModel.commitAllTabClosures();
                });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(1, tabCount);
        Assert.assertEquals(1, recentEntries.size());
        assertEntryIs(
                recentEntries.get(0),
                RecentlyClosedBulkEvent.class,
                new String[] {"Foo"},
                titles,
                urls);

        final RecentlyClosedBulkEvent event = (RecentlyClosedBulkEvent) recentEntries.get(0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRecentlyClosedBridge.openRecentlyClosedEntry(mTabModel, event);
                });

        // 1. Blank tab
        // 2. Restored tabA
        // 3. Restored tabB
        // 4. Restored tabC
        final List<Tab> tabs = getAllTabs();
        Assert.assertEquals(4, tabs.size());
        Assert.assertEquals(titles[2], ChromeTabUtils.getTitleOnUiThread(tabs.get(1)));
        Assert.assertEquals(urls[2], ChromeTabUtils.getUrlOnUiThread(tabs.get(1)).getSpec());
        Assert.assertEquals(titles[1], ChromeTabUtils.getTitleOnUiThread(tabs.get(2)));
        Assert.assertEquals(urls[1], ChromeTabUtils.getUrlOnUiThread(tabs.get(2)).getSpec());
        Assert.assertEquals(titles[0], ChromeTabUtils.getTitleOnUiThread(tabs.get(3)));
        Assert.assertEquals(urls[0], ChromeTabUtils.getUrlOnUiThread(tabs.get(3)).getSpec());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            "Foo", mTabGroupModelFilter.getTabGroupTitle(tabs.get(1).getId()));
                    Assert.assertEquals(
                            Arrays.asList(new Tab[] {tabs.get(1), tabs.get(2)}),
                            mTabGroupModelFilter.getRelatedTabList(tabs.get(1).getId()));
                    Assert.assertFalse(mTabGroupModelFilter.isTabInTabGroup(tabs.get(3)));
                });
    }

    /** Tests opening the most recent group closure. */
    @Test
    @MediumTest
    public void testOpenMostRecentlyClosedEntry_Group() {
        if (mTabGroupModelFilter == null) return;

        // Tab order is inverted in RecentlyClosedEntry as most recent comes first so log data in
        // reverse.
        final String[] groupUrls = new String[] {getUrl(TEST_PAGE_C), getUrl(TEST_PAGE_B)};
        final String[] url = new String[] {getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(url[0], /* incognito= */ false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(groupUrls[1], /* incognito= */ false);
        final Tab tabC = sActivityTestRule.loadUrlInNewTab(groupUrls[0], /* incognito= */ false);

        final String[] groupTitles = new String[2];
        final String[] title = new String[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.mergeTabsToGroup(tabC.getId(), tabB.getId());
                    title[0] = tabA.getTitle();
                    groupTitles[1] = tabB.getTitle();
                    groupTitles[0] = tabC.getTitle();
                    mTabModel.closeTabs(
                            TabClosureParams.closeTabs(List.of(tabB, tabC))
                                    .hideTabGroups(true)
                                    .build());
                    mTabModel.closeTabs(TabClosureParams.closeTab(tabA).build());
                    mTabModel.commitTabClosure(tabB.getId());
                    mTabModel.commitTabClosure(tabA.getId());
                    mTabModel.commitTabClosure(tabC.getId());
                });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(1, tabCount);
        Assert.assertEquals(2, recentEntries.size());
        assertEntryIs(
                recentEntries.get(0),
                RecentlyClosedGroup.class,
                new String[] {""},
                groupTitles,
                groupUrls);
        assertEntryIs(recentEntries.get(1), RecentlyClosedTab.class, new String[0], title, url);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRecentlyClosedBridge.openMostRecentlyClosedEntry(mTabModel);
                });

        // 1. Blank tab
        // 2. Restored tabB
        // 3. Restored tabC
        final List<Tab> tabs = getAllTabs();
        Assert.assertEquals(3, tabs.size());
        Assert.assertEquals(groupTitles[1], ChromeTabUtils.getTitleOnUiThread(tabs.get(1)));
        Assert.assertEquals(groupUrls[1], ChromeTabUtils.getUrlOnUiThread(tabs.get(1)).getSpec());
        Assert.assertEquals(groupTitles[0], ChromeTabUtils.getTitleOnUiThread(tabs.get(2)));
        Assert.assertEquals(groupUrls[0], ChromeTabUtils.getUrlOnUiThread(tabs.get(2)).getSpec());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertNull(mTabGroupModelFilter.getTabGroupTitle(tabs.get(1).getId()));
                    Assert.assertTrue(mTabGroupModelFilter.isTabInTabGroup(tabs.get(1)));
                    Assert.assertEquals(
                            Arrays.asList(new Tab[] {tabs.get(1), tabs.get(2)}),
                            mTabGroupModelFilter.getRelatedTabList(tabs.get(1).getId()));
                });

        tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(3, tabCount);
        Assert.assertEquals(1, recentEntries.size());
        assertEntryIs(recentEntries.get(0), RecentlyClosedTab.class, new String[0], title, url);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRecentlyClosedBridge.openMostRecentlyClosedEntry(mTabModel);
                });

        // 1. Blank tab
        // 2. Restored tabB
        // 3. Restored tabC
        // 4. Restored tabA
        tabs.clear();
        tabs.addAll(getAllTabs());
        Assert.assertEquals(4, tabs.size());
        Assert.assertEquals(title[0], ChromeTabUtils.getTitleOnUiThread(tabs.get(3)));
        Assert.assertEquals(url[0], ChromeTabUtils.getUrlOnUiThread(tabs.get(3)).getSpec());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(mTabGroupModelFilter.isTabInTabGroup(tabs.get(3)));
                });
    }

    /** Tests opening the most recent bulk closure. */
    @Test
    @MediumTest
    public void testOpenMostRecentlyClosedEntry_Bulk() {
        if (mTabGroupModelFilter == null) return;

        // Tab order is inverted in RecentlyClosedEntry as most recent comes first so log data in
        // reverse.
        final String[] urls =
                new String[] {getUrl(TEST_PAGE_C), getUrl(TEST_PAGE_B), getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[2], /* incognito= */ false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(urls[1], /* incognito= */ false);
        final Tab tabC = sActivityTestRule.loadUrlInNewTab(urls[0], /* incognito= */ false);

        final String[] titles = new String[3];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.mergeTabsToGroup(tabB.getId(), tabA.getId());
                    titles[2] = tabA.getTitle();
                    titles[1] = tabB.getTitle();
                    titles[0] = tabC.getTitle();
                    mTabModel.closeTabs(
                            TabClosureParams.closeTabs(List.of(tabA, tabB, tabC))
                                    .hideTabGroups(true)
                                    .build());
                    mTabModel.commitAllTabClosures();
                });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(1, tabCount);
        Assert.assertEquals(1, recentEntries.size());
        assertEntryIs(
                recentEntries.get(0),
                RecentlyClosedBulkEvent.class,
                new String[] {""},
                titles,
                urls);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRecentlyClosedBridge.openMostRecentlyClosedEntry(mTabModel);
                });

        // 1. Blank tab
        // 2. Restored tabA
        // 3. Restored tabB
        // 4. Restored tabC
        final List<Tab> tabs = getAllTabs();
        Assert.assertEquals(4, tabs.size());
        Assert.assertEquals(titles[2], ChromeTabUtils.getTitleOnUiThread(tabs.get(1)));
        Assert.assertEquals(urls[2], ChromeTabUtils.getUrlOnUiThread(tabs.get(1)).getSpec());
        Assert.assertEquals(titles[1], ChromeTabUtils.getTitleOnUiThread(tabs.get(2)));
        Assert.assertEquals(urls[1], ChromeTabUtils.getUrlOnUiThread(tabs.get(2)).getSpec());
        Assert.assertEquals(titles[0], ChromeTabUtils.getTitleOnUiThread(tabs.get(3)));
        Assert.assertEquals(urls[0], ChromeTabUtils.getUrlOnUiThread(tabs.get(3)).getSpec());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(mTabGroupModelFilter.isTabInTabGroup(tabs.get(1)));
                    Assert.assertNull(mTabGroupModelFilter.getTabGroupTitle(tabs.get(1).getId()));
                    Assert.assertEquals(
                            Arrays.asList(new Tab[] {tabs.get(1), tabs.get(2)}),
                            mTabGroupModelFilter.getRelatedTabList(tabs.get(1).getId()));
                    Assert.assertFalse(mTabGroupModelFilter.isTabInTabGroup(tabs.get(3)));
                });
    }

    /** Tests tabs are not saved when unrestorable. */
    @Test
    @MediumTest
    public void testNoRecentlyClosedEntry_FromBulkClosure_Unrestorable() {
        final String[] urls = new String[] {getUrl(TEST_PAGE_B), getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[1], /* incognito= */ false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(urls[0], /* incognito= */ false);

        final String[] titles = new String[2];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    titles[1] = tabA.getTitle();
                    titles[0] = tabB.getTitle();
                    mTabModel.closeTabs(
                            TabClosureParams.closeTabs(List.of(tabB, tabA))
                                    .allowUndo(false)
                                    .hideTabGroups(true)
                                    .saveToTabRestoreService(false)
                                    .build());
                    mTabModel.commitAllTabClosures();
                });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(1, tabCount);
        Assert.assertEquals(0, recentEntries.size());
    }

    /** Tests tab groups are not saved when unrestorable. */
    @Test
    @MediumTest
    public void testNoRecentlyClosedEntry_FromGroupClosure_Unrestorable() {
        if (mTabGroupModelFilter == null) return;

        final String[] urls = new String[] {getUrl(TEST_PAGE_B), getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[1], /* incognito= */ false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(urls[0], /* incognito= */ false);

        final String[] titles = new String[2];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.mergeTabsToGroup(tabB.getId(), tabA.getId());
                    titles[1] = tabA.getTitle();
                    titles[0] = tabB.getTitle();
                    mTabGroupModelFilter.closeTabs(
                            TabClosureParams.closeTabs(List.of(tabB, tabA))
                                    .allowUndo(false)
                                    .hideTabGroups(true)
                                    .saveToTabRestoreService(false)
                                    .build());
                });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(1, tabCount);
        Assert.assertEquals(0, recentEntries.size());
    }

    /** Tests closing a tab will be saved as a TAB session entry in tab restore service. */
    @Test
    @MediumTest
    public void testCloseTabSaveAsTabSessionRestoreEntry() {
        final String[] urls = new String[] {getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[0], /* incognito= */ false);

        final String[] titles = new String[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    titles[0] = tabA.getTitle();
                    mTabModel.closeTabs(TabClosureParams.closeTab(tabA).build());
                    mTabModel.commitTabClosure(tabA.getId());
                });
        final List<RecentlyClosedEntry> recentEntries = new ArrayList();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    recentEntries.addAll(
                            mRecentlyClosedBridge.getRecentlyClosedEntries(MAX_ENTRY_COUNT));
                });
        Assert.assertEquals(1, recentEntries.size());
        RecentlyClosedEntry recentEntry = recentEntries.get(0);
        // Verify recentEntry is from a TAB session entry returned by tab restore service.
        // Note: RecentlyClosedBridgeJni.getRecentlyClosedEntries returns a RecentlyClosedTab
        // instance for a session entry of type sessions::tab_restore::Type::TAB.
        Assert.assertTrue(RecentlyClosedTab.class.isInstance(recentEntry));
        final List<RecentlyClosedTab> recentTabs =
                (List<RecentlyClosedTab>) (List<? extends RecentlyClosedEntry>) recentEntries;
        Assert.assertEquals(1, recentTabs.size());
        assertTabsAre(recentTabs, titles, urls);
    }

    /** Tests closing all tabs will be saved as a WINDOW session entry in tab restore service. */
    @Test
    @MediumTest
    public void testCloseAllTabsSaveAsWindowSessionRestoreEntry() {
        // Tab order is inverted in RecentlyClosedEntry as most recent comes first so log data in
        // reverse.
        final String[] urls = new String[] {getUrl(TEST_PAGE_B), getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[1], /* incognito= */ false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(urls[0], /* incognito= */ false);

        final String[] titles = new String[2];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    titles[1] = tabA.getTitle();
                    titles[0] = tabB.getTitle();
                    mTabModel.closeTabs(TabClosureParams.closeAllTabs().build());
                    mTabModel.commitAllTabClosures();
                });
        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    recentEntries.addAll(
                            mRecentlyClosedBridge.getRecentlyClosedEntries(MAX_ENTRY_COUNT));
                });
        Assert.assertEquals(1, recentEntries.size());
        RecentlyClosedEntry recentEntry = recentEntries.get(0);
        // Verify recentEntry is from a WINDOW session entry returned by tab restore service.
        // Note: RecentlyClosedBridgeJni.getRecentlyClosedEntries returns a RecentlyClosedBulkEvent
        // instance for a session entry of type sessions::tab_restore::Type::WINDOW.
        Assert.assertTrue(RecentlyClosedBulkEvent.class.isInstance(recentEntry));
        final RecentlyClosedBulkEvent event = (RecentlyClosedBulkEvent) recentEntry;
        final List<RecentlyClosedTab> recentTabs = event.getTabs();
        Assert.assertEquals(2, recentTabs.size());
        assertTabsAre(recentTabs, titles, urls);
    }

    /**
     * Tests closing multiple tabs will be saved as a WINDOW session entry in tab restore service.
     */
    @Test
    @MediumTest
    public void testCloseMultipleTabsSaveAsWindowSessionRestoreEntry() {
        // Tab order is inverted in RecentlyClosedEntry as most recent comes first so log data in
        // reverse.
        final String[] urls = new String[] {getUrl(TEST_PAGE_B), getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[1], /* incognito= */ false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(urls[0], /* incognito= */ false);

        final String[] titles = new String[2];
        final int[] tabCountBeforeClosingTabs = new int[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    titles[1] = tabA.getTitle();
                    titles[0] = tabB.getTitle();
                    tabCountBeforeClosingTabs[0] = mTabModel.getCount();
                    mTabGroupModelFilter.closeTabs(
                            TabClosureParams.closeTabs(List.of(tabA, tabB))
                                    .hideTabGroups(true)
                                    .build());
                    mTabModel.commitAllTabClosures();
                });
        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCountAfterClosingTabs = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(3, tabCountBeforeClosingTabs[0]);
        Assert.assertEquals(1, tabCountAfterClosingTabs);
        Assert.assertEquals(1, recentEntries.size());
        RecentlyClosedEntry recentEntry = recentEntries.get(0);
        // Verify recentEntry is from a WINDOW session entry returned by tab restore service.
        // Note: RecentlyClosedBridgeJni.getRecentlyClosedEntries returns a RecentlyClosedBulkEvent
        // instance for a session entry of type sessions::tab_restore::Type::WINDOW.
        Assert.assertTrue(RecentlyClosedBulkEvent.class.isInstance(recentEntry));
        final RecentlyClosedBulkEvent event = (RecentlyClosedBulkEvent) recentEntry;
        final List<RecentlyClosedTab> recentTabs = event.getTabs();
        Assert.assertEquals(2, recentTabs.size());
        assertTabsAre(recentTabs, titles, urls);
    }

    // TODO(crbug.com/40218713): Add a test a case where bulk closures remain in the native service,
    // but the flag state is flipped.

    private Tab findTabWithUrlAndTitle(TabList list, String url, String title) {
        Tab targetTab = null;
        for (int i = 0; i < list.getCount(); ++i) {
            Tab tab = list.getTabAt(i);
            if (tab.getUrl().getSpec().equals(url) && tab.getTitle().equals(title)) {
                targetTab = tab;
                break;
            }
        }
        return targetTab;
    }

    private String getUrl(String relativeUrl) {
        return sActivityTestRule.getTestServer().getURL(relativeUrl);
    }

    private Tab freezeTab(Tab tab) {
        Tab[] frozen = new Tab[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabState state = TabStateExtractor.from(tab);
                    mActivity
                            .getCurrentTabModel()
                            .closeTabs(TabClosureParams.closeTab(tab).allowUndo(false).build());
                    frozen[0] =
                            mActivity.getCurrentTabCreator().createFrozenTab(state, tab.getId(), 1);
                });
        return frozen[0];
    }

    private void assertTabsAre(List<RecentlyClosedTab> tabs, String[] titles, String[] urls) {
        assert titles.length == urls.length;
        Assert.assertEquals("Unexpected number of tabs.", titles.length, tabs.size());
        for (int i = 0; i < titles.length; i++) {
            Assert.assertEquals("Tab " + i + " title mismatch.", titles[i], tabs.get(i).getTitle());
            Assert.assertEquals(
                    "Tab " + i + " url mismatch.", urls[i], tabs.get(i).getUrl().getSpec());
        }
    }

    private void assertEntryIs(
            RecentlyClosedEntry entry,
            Class<? extends RecentlyClosedEntry> cls,
            String[] groupTitles,
            String[] titles,
            String[] urls) {
        assert titles.length == urls.length;
        Assert.assertTrue(cls.isInstance(entry));

        if (cls == RecentlyClosedTab.class) {
            assert groupTitles.length == 0;
            assert titles.length == 1;
            RecentlyClosedTab tab = (RecentlyClosedTab) entry;
            assertTabsAre(Collections.singletonList(tab), titles, urls);
            return;
        }
        if (cls == RecentlyClosedGroup.class) {
            assert groupTitles.length == 1;
            RecentlyClosedGroup group = (RecentlyClosedGroup) entry;
            Assert.assertEquals(groupTitles[0], group.getTitle());
            assertTabsAre(group.getTabs(), titles, urls);
            return;
        }

        RecentlyClosedBulkEvent event = (RecentlyClosedBulkEvent) entry;
        final List<String> expectedTitles = Arrays.asList(groupTitles);
        final List<String> actualTitles = new ArrayList<>(event.getTabGroupIdToTitleMap().values());
        Assert.assertEquals(expectedTitles.size(), actualTitles.size());
        Assert.assertTrue(
                expectedTitles.containsAll(actualTitles)
                        && actualTitles.containsAll(expectedTitles));
        assertTabsAre(event.getTabs(), titles, urls);
    }

    private List<Tab> getAllTabs() {
        final List<Tab> list = new ArrayList<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (int i = 0; i < mTabModel.getCount(); i++) {
                        list.add(mTabModel.getTabAt(i));
                    }
                });
        return list;
    }

    private int getRecentEntriesAndReturnActiveTabCount(final List<RecentlyClosedEntry> entries) {
        final int[] tabCount = new int[1];
        entries.clear();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    entries.addAll(mRecentlyClosedBridge.getRecentlyClosedEntries(MAX_ENTRY_COUNT));
                    tabCount[0] = mTabModel.getCount();
                });
        return tabCount[0];
    }

    private void restartActivity() {
        ThreadUtils.runOnUiThreadBlocking(mActivity::saveState);
        sActivityTestRule.recreateActivity();
        mActivity = sActivityTestRule.getActivity();
        mTabModelSelector = mActivity.getTabModelSelectorSupplier().get();
        CriteriaHelper.pollUiThread(mTabModelSelector::isTabStateInitialized);
        mTabModel = mTabModelSelector.getModel(false);
        TabModelFilter filter =
                mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(false);
        assert filter instanceof TabGroupModelFilter;
        mTabGroupModelFilter = (TabGroupModelFilter) filter;
    }
}
