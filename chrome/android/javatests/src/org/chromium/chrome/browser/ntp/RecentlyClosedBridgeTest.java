// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupTitleUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * Tests for {@link RecentlyClosedBridge} including native TabRestoreService.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, ChromeSwitches.DISABLE_STARTUP_PROMOS})
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

    private ChromeTabbedActivity mActivity;
    private TabModelSelector mTabModelSelector;
    private TabGroupModelFilter mTabGroupModelFilter;
    private TabModel mTabModel;
    private RecentlyClosedBridge mRecentlyClosedBridge;

    @Before
    public void setUp() {
        sActivityTestRule.waitForActivityNativeInitializationComplete();

        // Disable snackbars from the {@link UndoBarController} which can break this test.
        sActivityTestRule.getActivity().getSnackbarManager().disableForTesting();

        mActivity = sActivityTestRule.getActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mRecentlyClosedBridge = new RecentlyClosedBridge(Profile.getLastUsedRegularProfile(),
                    mActivity.getTabModelSelectorSupplier().get());
            mRecentlyClosedBridge.clearRecentlyClosedEntries();
            Assert.assertEquals(
                    0, mRecentlyClosedBridge.getRecentlyClosedEntries(MAX_ENTRY_COUNT).size());
        });
        mActivity = sActivityTestRule.getActivity();
        mTabModelSelector = mActivity.getTabModelSelectorSupplier().get();
        mTabModel = mTabModelSelector.getModel(false);
        TabModelFilter filter =
                mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(false);
        if (filter instanceof TabGroupModelFilter) {
            mTabGroupModelFilter = (TabGroupModelFilter) filter;
        } else {
            mTabGroupModelFilter = null;
        }
        final Tab tab = mActivity.getActivityTab();
        ChromeTabUtils.waitForInteractable(tab);
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mRecentlyClosedBridge.clearRecentlyClosedEntries();
            Assert.assertEquals(
                    0, mRecentlyClosedBridge.getRecentlyClosedEntries(MAX_ENTRY_COUNT).size());
            mRecentlyClosedBridge.destroy();
        });
    }

    /**
     * Tests opening the most recently closed tab in the background.
     */
    @Test
    @MediumTest
    public void testOpenMostRecentlyClosedEntry_Tab_InBackground() {
        final String urls[] = new String[] {getUrl(TEST_PAGE_A), getUrl(TEST_PAGE_B)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[0], /*incognito=*/false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(urls[1], /*incognito=*/false);

        final String[] titles = new String[2];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            titles[0] = tabA.getTitle();
            titles[1] = tabB.getTitle();
            mTabModel.closeTab(tabB);
            mTabModel.closeTab(tabA, false, false, true);
            mTabModel.commitTabClosure(tabA.getId());
        });

        final List<RecentlyClosedTab> recentTabs = new ArrayList<>();
        final int[] tabCount = new int[1];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            tabCount[0] = mTabModel.getCount();
            recentTabs.addAll(
                    (List<RecentlyClosedTab>) (List<? extends RecentlyClosedEntry>)
                            mRecentlyClosedBridge.getRecentlyClosedEntries(MAX_ENTRY_COUNT));
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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // No Renderer for background tabA.
            Assert.assertNull(tabs.get(1).getWebContents().getRenderWidgetHostView());
        });
    }

    /**
     * Tests opening a specific closed {@link Tab} as a new background tab.
     */
    @Test
    @MediumTest
    public void testOpenRecentlyClosedTab_InCurrentTab() {
        final String urls[] = new String[] {getUrl(TEST_PAGE_A), getUrl(TEST_PAGE_B)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[0], /*incognito=*/false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(urls[1], /*incognito=*/false);
        final Tab tabC =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_C), /*incognito=*/false);

        final String[] titles = new String[2];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabModel.setIndex(mTabModel.indexOf(tabC), TabSelectionType.FROM_USER, false);
            titles[0] = tabA.getTitle();
            titles[1] = tabB.getTitle();
            mTabModel.closeTab(tabB);
            mTabModel.closeTab(tabA);
        });

        final List<RecentlyClosedTab> recentTabs = new ArrayList<>();
        final int[] tabCount = new int[1];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            tabCount[0] = mTabModel.getCount();
            recentTabs.addAll(
                    (List<RecentlyClosedTab>) (List<? extends RecentlyClosedEntry>)
                            mRecentlyClosedBridge.getRecentlyClosedEntries(MAX_ENTRY_COUNT));
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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNotNull(tabC.getWebContents());
            // Should only have one navigation entry as it replaced TEST_PAGE_C.
            Assert.assertEquals(1,
                    tabC.getWebContents()
                            .getNavigationController()
                            .getNavigationHistory()
                            .getEntryCount());

            // Has renderer for foreground tab.
            Assert.assertNotNull(tabC.getWebContents().getRenderWidgetHostView());
        });
    }

    /**
     * Tests opening a specific closed {@link Tab} that was frozen as a new background tab.
     */
    @Test
    @MediumTest
    public void testOpenRecentlyClosedTab_Frozen_InBackground() {
        final String urls[] = new String[] {getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[0], /*incognito=*/false);
        final Tab tabB =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_B), /*incognito=*/false);
        final Tab frozenTabA = freezeTab(tabA);
        // Clear the entry created by freezing the tab.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mRecentlyClosedBridge.clearRecentlyClosedEntries(); });

        final String[] titles = new String[1];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            titles[0] = frozenTabA.getTitle();
            mTabModel.closeTab(frozenTabA);
        });

        final List<RecentlyClosedTab> recentTabs = new ArrayList<>();
        final int[] tabCount = new int[1];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            tabCount[0] = mTabModel.getCount();
            recentTabs.addAll(
                    (List<RecentlyClosedTab>) (List<? extends RecentlyClosedEntry>)
                            mRecentlyClosedBridge.getRecentlyClosedEntries(MAX_ENTRY_COUNT));
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
        final String urls[] = new String[] {getUrl(TEST_PAGE_B), getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[1], /*incognito=*/false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(urls[0], /*incognito=*/false);

        final String[] titles = new String[2];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            titles[1] = tabA.getTitle();
            titles[0] = tabB.getTitle();
            mTabModel.closeAllTabs();
            mTabModel.commitAllTabClosures();
        });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(0, tabCount);
        Assert.assertEquals(1, recentEntries.size());
        assertEntryIs(
                recentEntries.get(0), RecentlyClosedBulkEvent.class, new String[0], titles, urls);

        final RecentlyClosedBulkEvent event = (RecentlyClosedBulkEvent) recentEntries.get(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mRecentlyClosedBridge.openRecentlyClosedTab(
                    mTabModel, event.getTabs().get(1), WindowOpenDisposition.NEW_FOREGROUND_TAB);
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
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @Restriction({Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testOpenRecentlyClosedTab_FromGroupInBulkClosure_InBackgroundTab() {
        if (mTabGroupModelFilter == null) return;

        // Tab order is inverted in RecentlyClosedEntry as most recent comes first so log data in
        // reverse.
        final String urls[] =
                new String[] {getUrl(TEST_PAGE_C), getUrl(TEST_PAGE_B), getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[2], /*incognito=*/false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(urls[1], /*incognito=*/false);
        final Tab tabC = sActivityTestRule.loadUrlInNewTab(urls[0], /*incognito=*/false);

        final String[] titles = new String[3];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGroupModelFilter.mergeTabsToGroup(tabB.getId(), tabA.getId());
            titles[2] = tabA.getTitle();
            titles[1] = tabB.getTitle();
            titles[0] = tabC.getTitle();
            mTabModel.closeMultipleTabs(Arrays.asList(new Tab[] {tabA, tabB, tabC}), true);
            mTabModel.commitAllTabClosures();
        });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(1, tabCount);
        Assert.assertEquals(1, recentEntries.size());
        assertEntryIs(recentEntries.get(0), RecentlyClosedBulkEvent.class, new String[] {""},
                titles, urls);

        final RecentlyClosedBulkEvent event = (RecentlyClosedBulkEvent) recentEntries.get(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Tab order is inverted as most recent comes first so pick the last tab to restore ==
            // tabB.
            mRecentlyClosedBridge.openRecentlyClosedTab(
                    mTabModel, event.getTabs().get(1), WindowOpenDisposition.NEW_BACKGROUND_TAB);
        });

        // 1. Blank tab
        // 2. Restored tabB
        final List<Tab> tabs = getAllTabs();
        Assert.assertEquals(2, tabs.size());
        Assert.assertEquals(titles[1], ChromeTabUtils.getTitleOnUiThread(tabs.get(1)));
        Assert.assertEquals(urls[1], ChromeTabUtils.getUrlOnUiThread(tabs.get(1)).getSpec());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(mTabGroupModelFilter.hasOtherRelatedTabs(tabs.get(1)));
        });
    }

    /**
     * Tests opening a specific closed {@link Tab} that was closed as part of a group replacing the
     * current tab.
     */
    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @Restriction({Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testOpenRecentlyClosedTab_FromGroupClosure_InCurrentTab() {
        if (mTabGroupModelFilter == null) return;

        // Tab order is inverted in when closing.
        final String urls[] = new String[] {getUrl(TEST_PAGE_A), getUrl(TEST_PAGE_B)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[0], /*incognito=*/false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(urls[1], /*incognito=*/false);

        final String[] titles = new String[2];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGroupModelFilter.mergeTabsToGroup(tabB.getId(), tabA.getId());
            titles[0] = tabA.getTitle();
            titles[1] = tabB.getTitle();
            mTabModel.closeMultipleTabs(Arrays.asList(new Tab[] {tabB, tabA}), false);
        });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(1, tabCount);
        Assert.assertEquals(1, recentEntries.size());
        assertEntryIs(
                recentEntries.get(0), RecentlyClosedGroup.class, new String[] {""}, titles, urls);

        final RecentlyClosedGroup group = (RecentlyClosedGroup) recentEntries.get(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mRecentlyClosedBridge.openRecentlyClosedTab(
                    mTabModel, group.getTabs().get(1), WindowOpenDisposition.CURRENT_TAB);
        });

        // 1. tabA restored over blank tab.
        final List<Tab> tabs = getAllTabs();
        Assert.assertEquals(1, tabs.size());
        Assert.assertEquals(titles[1], ChromeTabUtils.getTitleOnUiThread(tabs.get(0)));
        Assert.assertEquals(urls[1], ChromeTabUtils.getUrlOnUiThread(tabs.get(0)).getSpec());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(mTabGroupModelFilter.hasOtherRelatedTabs(tabs.get(0)));
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mRecentlyClosedBridge.openRecentlyClosedTab(
                    mTabModel, group.getTabs().get(0), WindowOpenDisposition.NEW_BACKGROUND_TAB);
        });

        // 1. tabA restored over blank tab.
        // 2. tabB restored.
        tabs.clear();
        tabs.addAll(getAllTabs());
        Assert.assertEquals(2, tabs.size());
        Assert.assertEquals(titles[0], ChromeTabUtils.getTitleOnUiThread(tabs.get(1)));
        Assert.assertEquals(urls[0], ChromeTabUtils.getUrlOnUiThread(tabs.get(1)).getSpec());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(mTabGroupModelFilter.hasOtherRelatedTabs(tabs.get(1)));
        });
    }

    /**
     * Tests opening a specific closed {@link Tab} that was closed not as the most recent entry.
     */
    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @Restriction({Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testOpenRecentlyClosedEntry_Tab_FromMultipleTabs() {
        if (mTabGroupModelFilter == null) return;

        final String urlA[] = new String[] {getUrl(TEST_PAGE_A)};
        final String urlB[] = new String[] {getUrl(TEST_PAGE_B)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urlA[0], /*incognito=*/false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(urlB[0], /*incognito=*/false);

        final String[] titleA = new String[1];
        final String[] titleB = new String[1];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGroupModelFilter.mergeTabsToGroup(tabB.getId(), tabA.getId());
            titleA[0] = tabA.getTitle();
            titleB[0] = tabB.getTitle();
            mTabModel.closeTab(tabB, false, false, true);
            mTabModel.closeTab(tabA, false, false, true);
            mTabModel.commitAllTabClosures();
        });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(1, tabCount);
        Assert.assertEquals(2, recentEntries.size());
        assertEntryIs(recentEntries.get(0), RecentlyClosedTab.class, new String[0], titleA, urlA);
        assertEntryIs(recentEntries.get(1), RecentlyClosedTab.class, new String[0], titleB, urlB);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mRecentlyClosedBridge.openRecentlyClosedEntry(mTabModel, recentEntries.get(1));
        });

        // 1. Blank tab
        // 2. tabB restored in new tab.
        final List<Tab> tabs = getAllTabs();
        Assert.assertEquals(2, tabs.size());
        Assert.assertEquals(titleB[0], ChromeTabUtils.getTitleOnUiThread(tabs.get(1)));
        Assert.assertEquals(urlB[0], ChromeTabUtils.getUrlOnUiThread(tabs.get(1)).getSpec());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(mTabGroupModelFilter.hasOtherRelatedTabs(tabs.get(1)));
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mRecentlyClosedBridge.openRecentlyClosedEntry(mTabModel, recentEntries.get(0));
        });

        // 1. Blank tab
        // 2. tabB restored in new tab.
        // 3. tabA restored in new tab.
        tabs.clear();
        tabs.addAll(getAllTabs());
        Assert.assertEquals(3, tabs.size());
        Assert.assertEquals(titleA[0], ChromeTabUtils.getTitleOnUiThread(tabs.get(2)));
        Assert.assertEquals(urlA[0], ChromeTabUtils.getUrlOnUiThread(tabs.get(2)).getSpec());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(mTabGroupModelFilter.hasOtherRelatedTabs(tabs.get(2)));
        });
    }

    /**
     * Tests opening a specific closed {@link Tab} that was closed as part of a group.
     */
    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @Restriction({Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testOpenRecentlyClosedEntry_Tab_FromGroupClosure() {
        if (mTabGroupModelFilter == null) return;

        // Tab order is inverted in RecentlyClosedEntry as most recent comes first so log data in
        // reverse.
        final String urls[] = new String[] {getUrl(TEST_PAGE_B), getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[1], /*incognito=*/false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(urls[0], /*incognito=*/false);

        final String[] titles = new String[2];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGroupModelFilter.mergeTabsToGroup(tabB.getId(), tabA.getId());
            titles[1] = tabA.getTitle();
            titles[0] = tabB.getTitle();
            mTabModel.closeMultipleTabs(Arrays.asList(new Tab[] {tabA, tabB}), true);
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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Tab order is inverted as most recent comes first so pick the last tab to restore ==
            // tabA.
            mRecentlyClosedBridge.openRecentlyClosedEntry(mTabModel, group.getTabs().get(1));
        });

        // 1. Blank tab
        // 2. tabA restored in new tab.
        final List<Tab> tabs = getAllTabs();
        Assert.assertEquals(2, tabs.size());
        Assert.assertEquals(titles[1], ChromeTabUtils.getTitleOnUiThread(tabs.get(1)));
        Assert.assertEquals(urls[1], ChromeTabUtils.getUrlOnUiThread(tabs.get(1)).getSpec());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(mTabGroupModelFilter.hasOtherRelatedTabs(tabs.get(1)));
        });
    }

    /**
     * Tests opening a specific closed group.
     */
    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @Restriction({Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testOpenRecentlyClosedEntry_Group_FromGroupClosure() {
        if (mTabGroupModelFilter == null) return;

        // Tab order is inverted in RecentlyClosedEntry as most recent comes first so log data in
        // reverse.
        final String urls[] = new String[] {getUrl(TEST_PAGE_B), getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[1], /*incognito=*/false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(urls[0], /*incognito=*/false);

        final String[] titles = new String[2];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGroupModelFilter.mergeTabsToGroup(tabB.getId(), tabA.getId());
            TabGroupTitleUtils.storeTabGroupTitle(tabA.getId(), "Bar");
            titles[1] = tabA.getTitle();
            titles[0] = tabB.getTitle();
            mTabModel.closeMultipleTabs(Arrays.asList(new Tab[] {tabA, tabB}), false);
        });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(1, tabCount);
        Assert.assertEquals(1, recentEntries.size());
        assertEntryIs(recentEntries.get(0), RecentlyClosedGroup.class, new String[] {"Bar"}, titles,
                urls);

        final RecentlyClosedGroup group = (RecentlyClosedGroup) recentEntries.get(0);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mRecentlyClosedBridge.openRecentlyClosedEntry(mTabModel, group); });

        // 1. Blank tab
        // 2. tabA restored in new tab.
        // 3. tabB restored in new tab.
        final List<Tab> tabs = getAllTabs();
        Assert.assertEquals(3, tabs.size());
        Assert.assertEquals(titles[1], ChromeTabUtils.getTitleOnUiThread(tabs.get(1)));
        Assert.assertEquals(urls[1], ChromeTabUtils.getUrlOnUiThread(tabs.get(1)).getSpec());
        Assert.assertEquals(titles[0], ChromeTabUtils.getTitleOnUiThread(tabs.get(2)));
        Assert.assertEquals(urls[0], ChromeTabUtils.getUrlOnUiThread(tabs.get(2)).getSpec());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("Bar", TabGroupTitleUtils.getTabGroupTitle(tabs.get(1).getId()));
            Assert.assertTrue(mTabGroupModelFilter.hasOtherRelatedTabs(tabs.get(1)));
            Assert.assertEquals(Arrays.asList(new Tab[] {tabs.get(1), tabs.get(2)}),
                    mTabGroupModelFilter.getRelatedTabList(tabs.get(1).getId()));
        });
    }

    /**
     * Tests opening a specific closed group and that it persists across restarts.
     */
    @Test
    @LargeTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @Restriction({Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testOpenRecentlyClosedEntry_Group_FromGroupClosure_WithRestart() {
        if (mTabGroupModelFilter == null) return;

        // Tab order is inverted in RecentlyClosedEntry as most recent comes first so log data in
        // reverse.
        final String urls[] =
                new String[] {getUrl(TEST_PAGE_C), getUrl(TEST_PAGE_B), getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[2], /*incognito=*/false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(urls[1], /*incognito=*/false);
        final Tab tabC = sActivityTestRule.loadUrlInNewTab(urls[0], /*incognito=*/false);

        final String[] titles = new String[3];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGroupModelFilter.mergeTabsToGroup(tabB.getId(), tabA.getId());
            mTabGroupModelFilter.mergeTabsToGroup(tabC.getId(), tabA.getId());
            TabGroupTitleUtils.storeTabGroupTitle(tabA.getId(), "Bar");
            titles[2] = tabA.getTitle();
            titles[1] = tabB.getTitle();
            titles[0] = tabC.getTitle();
            mTabModel.closeMultipleTabs(Arrays.asList(new Tab[] {tabA, tabB, tabC}), false);
        });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(1, tabCount);
        Assert.assertEquals(1, recentEntries.size());
        assertEntryIs(recentEntries.get(0), RecentlyClosedGroup.class, new String[] {"Bar"}, titles,
                urls);

        final RecentlyClosedGroup group = (RecentlyClosedGroup) recentEntries.get(0);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mRecentlyClosedBridge.openRecentlyClosedEntry(mTabModel, group); });

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
        final int tabIds[] =
                new int[] {tabs.get(1).getId(), tabs.get(2).getId(), tabs.get(3).getId()};
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("Bar", TabGroupTitleUtils.getTabGroupTitle(tabs.get(1).getId()));
            Assert.assertTrue(mTabGroupModelFilter.hasOtherRelatedTabs(tabs.get(1)));
            Assert.assertTrue(mTabGroupModelFilter.hasOtherRelatedTabs(tabs.get(2)));
            Assert.assertTrue(mTabGroupModelFilter.hasOtherRelatedTabs(tabs.get(3)));
            Assert.assertEquals(Arrays.asList(new Tab[] {tabs.get(1), tabs.get(2), tabs.get(3)}),
                    mTabGroupModelFilter.getRelatedTabList(tabs.get(1).getId()));
        });

        // Restart activity.
        TestThreadUtils.runOnUiThreadBlocking(() -> { mActivity.saveState(); });
        sActivityTestRule.recreateActivity();
        mActivity = sActivityTestRule.getActivity();
        mTabModelSelector = mActivity.getTabModelSelectorSupplier().get();
        CriteriaHelper.pollUiThread(mTabModelSelector::isTabStateInitialized);
        mTabModel = mTabModelSelector.getModel(false);
        TabModelFilter filter =
                mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(false);
        assert filter instanceof TabGroupModelFilter;
        mTabGroupModelFilter = (TabGroupModelFilter) filter;

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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("Bar", TabGroupTitleUtils.getTabGroupTitle(tabs.get(1).getId()));
            Assert.assertTrue(mTabGroupModelFilter.hasOtherRelatedTabs(tabs.get(1)));
            Assert.assertTrue(mTabGroupModelFilter.hasOtherRelatedTabs(tabs.get(2)));
            Assert.assertTrue(mTabGroupModelFilter.hasOtherRelatedTabs(tabs.get(3)));
            Assert.assertEquals(Arrays.asList(new Tab[] {tabs.get(1), tabs.get(2), tabs.get(3)}),
                    mTabGroupModelFilter.getRelatedTabList(tabs.get(1).getId()));
        });
    }

    /**
     * Tests opening a specific closed {@link Tab} that was closed as part of a bulk closure.
     */
    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @Restriction({Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testOpenRecentlyClosedEntry_Tab_FromBulkClosure() {
        if (mTabGroupModelFilter == null) return;

        // Tab order is inverted in RecentlyClosedEntry as most recent comes first so log data in
        // reverse.
        final String urls[] =
                new String[] {getUrl(TEST_PAGE_C), getUrl(TEST_PAGE_B), getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[2], /*incognito=*/false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(urls[1], /*incognito=*/false);
        final Tab tabC = sActivityTestRule.loadUrlInNewTab(urls[0], /*incognito=*/false);

        final String[] titles = new String[3];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGroupModelFilter.mergeTabsToGroup(tabB.getId(), tabA.getId());
            titles[2] = tabA.getTitle();
            titles[1] = tabB.getTitle();
            titles[0] = tabC.getTitle();
            mTabModel.closeMultipleTabs(Arrays.asList(new Tab[] {tabA, tabB, tabC}), true);
            mTabModel.commitAllTabClosures();
        });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(1, tabCount);
        Assert.assertEquals(1, recentEntries.size());
        assertEntryIs(recentEntries.get(0), RecentlyClosedBulkEvent.class, new String[] {""},
                titles, urls);

        final RecentlyClosedBulkEvent event = (RecentlyClosedBulkEvent) recentEntries.get(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Tab order is inverted as most recent comes first so pick the last tab to restore ==
            // tabC.
            mRecentlyClosedBridge.openRecentlyClosedEntry(mTabModel, event.getTabs().get(0));
        });

        // 1. Blank tab
        // 2. Restored tabC
        final List<Tab> tabs = getAllTabs();
        Assert.assertEquals(2, tabs.size());
        Assert.assertEquals(titles[0], ChromeTabUtils.getTitleOnUiThread(tabs.get(1)));
        Assert.assertEquals(urls[0], ChromeTabUtils.getUrlOnUiThread(tabs.get(1)).getSpec());
    }

    /**
     * Tests opening a specific closed bulk closure.
     */
    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @Restriction({Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testOpenRecentlyClosedEntry_Bulk_FromBulkClosure() {
        if (mTabGroupModelFilter == null) return;

        // Tab order is inverted in RecentlyClosedEntry as most recent comes first so log data in
        // reverse.
        final String urls[] =
                new String[] {getUrl(TEST_PAGE_C), getUrl(TEST_PAGE_B), getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[2], /*incognito=*/false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(urls[1], /*incognito=*/false);
        final Tab tabC = sActivityTestRule.loadUrlInNewTab(urls[0], /*incognito=*/false);

        final String[] titles = new String[3];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGroupModelFilter.mergeTabsToGroup(tabB.getId(), tabA.getId());
            TabGroupTitleUtils.storeTabGroupTitle(tabA.getId(), "Foo");
            titles[2] = tabA.getTitle();
            titles[1] = tabB.getTitle();
            titles[0] = tabC.getTitle();
            mTabModel.closeMultipleTabs(Arrays.asList(new Tab[] {tabA, tabB, tabC}), true);
            mTabModel.commitAllTabClosures();
        });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(1, tabCount);
        Assert.assertEquals(1, recentEntries.size());
        assertEntryIs(recentEntries.get(0), RecentlyClosedBulkEvent.class, new String[] {"Foo"},
                titles, urls);

        final RecentlyClosedBulkEvent event = (RecentlyClosedBulkEvent) recentEntries.get(0);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mRecentlyClosedBridge.openRecentlyClosedEntry(mTabModel, event); });

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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("Foo", TabGroupTitleUtils.getTabGroupTitle(tabs.get(1).getId()));
            Assert.assertEquals(Arrays.asList(new Tab[] {tabs.get(1), tabs.get(2)}),
                    mTabGroupModelFilter.getRelatedTabList(tabs.get(1).getId()));
            Assert.assertFalse(mTabGroupModelFilter.hasOtherRelatedTabs(tabs.get(3)));
        });
    }

    /**
     * Tests opening the most recent group closure.
     */
    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @Restriction({Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testOpenMostRecentlyClosedEntry_Group() {
        if (mTabGroupModelFilter == null) return;

        // Tab order is inverted in RecentlyClosedEntry as most recent comes first so log data in
        // reverse.
        final String groupUrls[] = new String[] {getUrl(TEST_PAGE_C), getUrl(TEST_PAGE_B)};
        final String url[] = new String[] {getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(url[0], /*incognito=*/false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(groupUrls[1], /*incognito=*/false);
        final Tab tabC = sActivityTestRule.loadUrlInNewTab(groupUrls[0], /*incognito=*/false);

        final String[] groupTitles = new String[2];
        final String[] title = new String[1];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGroupModelFilter.mergeTabsToGroup(tabC.getId(), tabB.getId());
            title[0] = tabA.getTitle();
            groupTitles[1] = tabB.getTitle();
            groupTitles[0] = tabC.getTitle();
            mTabModel.closeMultipleTabs(Arrays.asList(new Tab[] {tabB, tabC}), true);
            mTabModel.closeTab(tabA, false, false, true);
            mTabModel.commitTabClosure(tabB.getId());
            mTabModel.commitTabClosure(tabA.getId());
            mTabModel.commitTabClosure(tabC.getId());
        });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(1, tabCount);
        Assert.assertEquals(2, recentEntries.size());
        assertEntryIs(recentEntries.get(0), RecentlyClosedGroup.class, new String[] {""},
                groupTitles, groupUrls);
        assertEntryIs(recentEntries.get(1), RecentlyClosedTab.class, new String[0], title, url);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mRecentlyClosedBridge.openMostRecentlyClosedEntry(mTabModel); });

        // 1. Blank tab
        // 2. Restored tabB
        // 3. Restored tabC
        final List<Tab> tabs = getAllTabs();
        Assert.assertEquals(3, tabs.size());
        Assert.assertEquals(groupTitles[1], ChromeTabUtils.getTitleOnUiThread(tabs.get(1)));
        Assert.assertEquals(groupUrls[1], ChromeTabUtils.getUrlOnUiThread(tabs.get(1)).getSpec());
        Assert.assertEquals(groupTitles[0], ChromeTabUtils.getTitleOnUiThread(tabs.get(2)));
        Assert.assertEquals(groupUrls[0], ChromeTabUtils.getUrlOnUiThread(tabs.get(2)).getSpec());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNull(TabGroupTitleUtils.getTabGroupTitle(tabs.get(1).getId()));
            Assert.assertTrue(mTabGroupModelFilter.hasOtherRelatedTabs(tabs.get(1)));
            Assert.assertEquals(Arrays.asList(new Tab[] {tabs.get(1), tabs.get(2)}),
                    mTabGroupModelFilter.getRelatedTabList(tabs.get(1).getId()));
        });

        tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(3, tabCount);
        Assert.assertEquals(1, recentEntries.size());
        assertEntryIs(recentEntries.get(0), RecentlyClosedTab.class, new String[0], title, url);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mRecentlyClosedBridge.openMostRecentlyClosedEntry(mTabModel); });

        // 1. Blank tab
        // 2. Restored tabB
        // 3. Restored tabC
        // 4. Restored tabA
        tabs.clear();
        tabs.addAll(getAllTabs());
        Assert.assertEquals(4, tabs.size());
        Assert.assertEquals(title[0], ChromeTabUtils.getTitleOnUiThread(tabs.get(3)));
        Assert.assertEquals(url[0], ChromeTabUtils.getUrlOnUiThread(tabs.get(3)).getSpec());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(mTabGroupModelFilter.hasOtherRelatedTabs(tabs.get(3)));
        });
    }

    /**
     * Tests opening the most recent bulk closure.
     */
    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @Restriction({Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testOpenMostRecentlyClosedEntry_Bulk() {
        if (mTabGroupModelFilter == null) return;

        // Tab order is inverted in RecentlyClosedEntry as most recent comes first so log data in
        // reverse.
        final String urls[] =
                new String[] {getUrl(TEST_PAGE_C), getUrl(TEST_PAGE_B), getUrl(TEST_PAGE_A)};
        final Tab tabA = sActivityTestRule.loadUrlInNewTab(urls[2], /*incognito=*/false);
        final Tab tabB = sActivityTestRule.loadUrlInNewTab(urls[1], /*incognito=*/false);
        final Tab tabC = sActivityTestRule.loadUrlInNewTab(urls[0], /*incognito=*/false);

        final String[] titles = new String[3];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGroupModelFilter.mergeTabsToGroup(tabB.getId(), tabA.getId());
            titles[2] = tabA.getTitle();
            titles[1] = tabB.getTitle();
            titles[0] = tabC.getTitle();
            mTabModel.closeMultipleTabs(Arrays.asList(new Tab[] {tabA, tabB, tabC}), true);
            mTabModel.commitAllTabClosures();
        });

        final List<RecentlyClosedEntry> recentEntries = new ArrayList<>();
        final int tabCount = getRecentEntriesAndReturnActiveTabCount(recentEntries);
        Assert.assertEquals(1, tabCount);
        Assert.assertEquals(1, recentEntries.size());
        assertEntryIs(recentEntries.get(0), RecentlyClosedBulkEvent.class, new String[] {""},
                titles, urls);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mRecentlyClosedBridge.openMostRecentlyClosedEntry(mTabModel); });

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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(mTabGroupModelFilter.hasOtherRelatedTabs(tabs.get(1)));
            Assert.assertNull(TabGroupTitleUtils.getTabGroupTitle(tabs.get(1).getId()));
            Assert.assertEquals(Arrays.asList(new Tab[] {tabs.get(1), tabs.get(2)}),
                    mTabGroupModelFilter.getRelatedTabList(tabs.get(1).getId()));
            Assert.assertFalse(mTabGroupModelFilter.hasOtherRelatedTabs(tabs.get(3)));
        });
    }

    // TODO(crbug.com/1307345): Add a test a case where bulk closures remain in the native service,
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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TabState state = TabStateExtractor.from(tab);
            mActivity.getCurrentTabModel().closeTab(tab);
            frozen[0] = mActivity.getCurrentTabCreator().createFrozenTab(
                    state, null, tab.getId(), tab.isIncognito(), 1);
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

    private void assertEntryIs(RecentlyClosedEntry entry, Class<? extends RecentlyClosedEntry> cls,
            String[] groupTitles, String[] titles, String[] urls) {
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
        final List<String> actualTitles = new ArrayList<>(event.getGroupIdToTitleMap().values());
        Assert.assertEquals(expectedTitles.size(), actualTitles.size());
        Assert.assertTrue(expectedTitles.containsAll(actualTitles)
                && actualTitles.containsAll(expectedTitles));
        assertTabsAre(event.getTabs(), titles, urls);
    }

    private List<Tab> getAllTabs() {
        final List<Tab> list = new ArrayList<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            for (int i = 0; i < mTabModel.getCount(); i++) {
                list.add(mTabModel.getTabAt(i));
            }
        });
        return list;
    }

    private int getRecentEntriesAndReturnActiveTabCount(final List<RecentlyClosedEntry> entries) {
        final int[] tabCount = new int[1];
        entries.clear();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            entries.addAll(mRecentlyClosedBridge.getRecentlyClosedEntries(MAX_ENTRY_COUNT));
            tabCount[0] = mTabModel.getCount();
        });
        return tabCount[0];
    }
}
