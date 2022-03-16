// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

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
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.mojom.WindowOpenDisposition;

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
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private ChromeTabbedActivity mActivity;
    private RecentlyClosedBridge mRecentlyClosedBridge;

    @Before
    public void setUp() {
        sActivityTestRule.waitForActivityNativeInitializationComplete();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mRecentlyClosedBridge = new RecentlyClosedBridge(Profile.getLastUsedRegularProfile());
            mRecentlyClosedBridge.clearRecentlyClosedTabs();
            Assert.assertEquals(
                    0, mRecentlyClosedBridge.getRecentlyClosedTabs(MAX_ENTRY_COUNT).size());
        });
        mActivity = sActivityTestRule.getActivity();
        final Tab tab = mActivity.getActivityTab();
        ChromeTabUtils.waitForInteractable(tab);
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mRecentlyClosedBridge.clearRecentlyClosedTabs();
            Assert.assertEquals(
                    0, mRecentlyClosedBridge.getRecentlyClosedTabs(MAX_ENTRY_COUNT).size());
            mRecentlyClosedBridge.destroy();
        });
    }

    @Test
    @MediumTest
    public void testOpenMostRecentlyClosedInBackground() {
        final Tab tabA =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_A), /*incognito=*/false);
        final Tab tabB =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_B), /*incognito=*/false);

        String[] titles = new String[2];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            final TabModel model = mActivity.getTabModelSelectorSupplier().get().getModel(false);
            titles[0] = tabA.getTitle();
            titles[1] = tabB.getTitle();
            model.closeTab(tabB);
            model.closeTab(tabA);
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // 1. Blank Tab
            final TabModel model = mActivity.getTabModelSelectorSupplier().get().getModel(false);
            final TabList list = model.getComprehensiveModel();
            Assert.assertEquals(1, list.getCount());

            final List<RecentlyClosedTab> recentTabs =
                    mRecentlyClosedBridge.getRecentlyClosedTabs(MAX_ENTRY_COUNT);
            Assert.assertEquals(2, recentTabs.size());
            Assert.assertEquals(titles[0], recentTabs.get(0).title);
            Assert.assertEquals(titles[1], recentTabs.get(1).title);
            Assert.assertEquals(getUrl(TEST_PAGE_A), recentTabs.get(0).url.getSpec());
            Assert.assertEquals(getUrl(TEST_PAGE_B), recentTabs.get(1).url.getSpec());
            mRecentlyClosedBridge.openMostRecentlyClosedTab(model);
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // 1. Blank Tab
            // 2. tabA
            final TabModel model = mActivity.getTabModelSelectorSupplier().get().getModel(false);
            final TabList list = model.getComprehensiveModel();
            Assert.assertEquals(2, list.getCount());
            final Tab tab = findTabWithUrlAndTitle(list, getUrl(TEST_PAGE_A), titles[0]);
            Assert.assertNotNull(tab);

            // No Renderer for background tab.
            Assert.assertNull(tab.getWebContents().getRenderWidgetHostView());
        });
    }

    @Test
    @MediumTest
    public void testOpenSpecificRecentlyClosedTabInCurrentTab() {
        final Tab tabA =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_A), /*incognito=*/false);
        final Tab tabB =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_B), /*incognito=*/false);
        final Tab tabC =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_C), /*incognito=*/false);

        String[] titles = new String[2];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            final TabModel model = mActivity.getTabModelSelectorSupplier().get().getModel(false);
            titles[0] = tabA.getTitle();
            titles[1] = tabB.getTitle();
            model.closeTab(tabB);
            model.closeTab(tabA);
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // 1. Blank Tab
            // 2. tabC
            final TabModel model = mActivity.getTabModelSelectorSupplier().get().getModel(false);
            final TabList list = model.getComprehensiveModel();
            Assert.assertEquals(2, list.getCount());

            final List<RecentlyClosedTab> recentTabs =
                    mRecentlyClosedBridge.getRecentlyClosedTabs(MAX_ENTRY_COUNT);
            Assert.assertEquals(2, recentTabs.size());
            Assert.assertEquals(titles[0], recentTabs.get(0).title);
            Assert.assertEquals(titles[1], recentTabs.get(1).title);
            Assert.assertEquals(getUrl(TEST_PAGE_A), recentTabs.get(0).url.getSpec());
            Assert.assertEquals(getUrl(TEST_PAGE_B), recentTabs.get(1).url.getSpec());
            mRecentlyClosedBridge.openRecentlyClosedTab(
                    model, recentTabs.get(1), WindowOpenDisposition.CURRENT_TAB);
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // 1. Blank Tab
            // 2. tabC - now TEST_PAGE_B
            final TabModel model = mActivity.getTabModelSelectorSupplier().get().getModel(false);
            final TabList list = model.getComprehensiveModel();
            Assert.assertEquals(2, list.getCount());
            Assert.assertEquals(
                    tabC, mActivity.getTabModelSelectorSupplier().get().getCurrentTab());
            Assert.assertEquals(getUrl(TEST_PAGE_B), tabC.getUrl().getSpec());
            Assert.assertEquals(titles[1], tabC.getTitle());
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

    @Test
    @MediumTest
    public void testOpenRecentlyClosedFrozenTab() {
        final Tab tabA =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_A), /*incognito=*/false);
        final Tab tabB =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_B), /*incognito=*/false);
        final Tab frozenTabA = freezeTab(tabA);
        // Clear the entry created by freezing the tab.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mRecentlyClosedBridge.clearRecentlyClosedTabs(); });

        String[] titles = new String[1];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            final TabModel model = mActivity.getTabModelSelectorSupplier().get().getModel(false);
            titles[0] = frozenTabA.getTitle();
            model.closeTab(frozenTabA);
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // 1. Blank Tab
            // 2. tabB
            final TabModel model = mActivity.getTabModelSelectorSupplier().get().getModel(false);
            final TabList list = model.getComprehensiveModel();
            Assert.assertEquals(2, list.getCount());

            final List<RecentlyClosedTab> recentTabs =
                    mRecentlyClosedBridge.getRecentlyClosedTabs(MAX_ENTRY_COUNT);
            Assert.assertEquals(1, recentTabs.size());
            Assert.assertEquals(titles[0], recentTabs.get(0).title);
            Assert.assertEquals(getUrl(TEST_PAGE_A), recentTabs.get(0).url.getSpec());
            mRecentlyClosedBridge.openRecentlyClosedTab(
                    model, recentTabs.get(0), WindowOpenDisposition.NEW_BACKGROUND_TAB);
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // 1. Blank Tab
            // 2. tabB
            // 3. tabA - restored.
            final TabModel model = mActivity.getTabModelSelectorSupplier().get().getModel(false);
            final TabList list = model.getComprehensiveModel();
            Assert.assertEquals(3, list.getCount());
            final Tab tab = findTabWithUrlAndTitle(list, getUrl(TEST_PAGE_A), titles[0]);
            Assert.assertNotNull(tab);

            // No renderer for background tab.
            Assert.assertNull(tab.getWebContents().getRenderWidgetHostView());
        });
    }

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
}
