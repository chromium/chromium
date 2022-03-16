// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

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
import org.chromium.chrome.browser.ntp.RecentlyClosedBridge;
import org.chromium.chrome.browser.ntp.RecentlyClosedTab;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for {@link HistoricalTabSaver}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, ChromeSwitches.DISABLE_STARTUP_PROMOS})
@Batch(Batch.PER_CLASS)
public class HistoricalTabSaverTest {
    private static final int MAX_ENTRY_COUNT = 5;
    private static final String TEST_PAGE = "/chrome/test/data/android/about.html";

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private ChromeTabbedActivity mActivity;
    private RecentlyClosedBridge mRecentlyClosedBridge;
    private Tab mTab;

    @Before
    public void setUp() {
        sActivityTestRule.waitForActivityNativeInitializationComplete();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mRecentlyClosedBridge = new RecentlyClosedBridge(Profile.getLastUsedRegularProfile());
        });
        clearEntries();
        mActivity = sActivityTestRule.getActivity();
        mTab = mActivity.getActivityTab();
        ChromeTabUtils.waitForInteractable(mTab);
    }

    @After
    public void tearDown() {
        clearEntries();
        TestThreadUtils.runOnUiThreadBlocking(() -> { mRecentlyClosedBridge.destroy(); });
    }

    @Test
    @MediumTest
    public void testCreateHistoricalTab_NotFrozen_HistoricalTabCreated() {
        sActivityTestRule.loadUrl(getUrl(TEST_PAGE));
        createHistoricTab(mTab);
        ArrayList<RecentlyClosedTab> expectedEntries = new ArrayList<>();
        expectedEntries.add(new RecentlyClosedTab(
                0, ChromeTabUtils.getTitleOnUiThread(mTab), ChromeTabUtils.getUrlOnUiThread(mTab)));
        assertEntriesAre(expectedEntries);
    }

    @Test
    @MediumTest
    public void testCreateHistoricalTab_Frozen_HistoricalTabCreated() {
        final Tab tab = sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE), /*incognito=*/false);
        final Tab frozenTab = freezeTab(tab);
        // Clear the entry created by freezing the tab.
        clearEntries();

        createHistoricTab(frozenTab);
        ArrayList<RecentlyClosedTab> expectedEntries = new ArrayList<>();
        expectedEntries.add(new RecentlyClosedTab(0, ChromeTabUtils.getTitleOnUiThread(frozenTab),
                ChromeTabUtils.getUrlOnUiThread(frozenTab)));
        assertEntriesAre(expectedEntries);
    }

    @Test
    @MediumTest
    public void testCreateHistoricalTab_InvalidUrls() {
        sActivityTestRule.loadUrl("about:blank");
        createHistoricTab(mTab);
        assertEntriesAre(new ArrayList<RecentlyClosedTab>());

        sActivityTestRule.loadUrl("chrome://flags/");
        createHistoricTab(mTab);
        assertEntriesAre(new ArrayList<RecentlyClosedTab>());

        sActivityTestRule.loadUrl("chrome-native://recent-tabs/");
        createHistoricTab(mTab);
        assertEntriesAre(new ArrayList<RecentlyClosedTab>());
    }

    private static void createHistoricTab(Tab tab) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { HistoricalTabSaver.createHistoricalTab(tab); });
    }

    private void assertEntriesAre(List<RecentlyClosedTab> expectedEntries) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<RecentlyClosedTab> actualEntries =
                    mRecentlyClosedBridge.getRecentlyClosedTabs(MAX_ENTRY_COUNT);
            Assert.assertEquals(expectedEntries.size(), actualEntries.size());
            for (int i = 0; i < expectedEntries.size(); ++i) {
                Assert.assertEquals(expectedEntries.get(i).title, actualEntries.get(i).title);
                Assert.assertEquals(expectedEntries.get(i).url, actualEntries.get(i).url);
            }
        });
    }

    private void clearEntries() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mRecentlyClosedBridge.clearRecentlyClosedTabs();
            Assert.assertEquals(
                    0, mRecentlyClosedBridge.getRecentlyClosedTabs(MAX_ENTRY_COUNT).size());
        });
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

    private String getUrl(String relativeUrl) {
        return sActivityTestRule.getTestServer().getURL(relativeUrl);
    }
}
