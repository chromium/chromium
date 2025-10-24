// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.transit.Triggers.noopTo;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.hub.HistoryPaneStation;
import org.chromium.chrome.test.transit.hub.HistoryPaneStation.HistorySearchFacility;
import org.chromium.chrome.test.transit.hub.HistoryPaneStation.HistoryWithEntriesFacility;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.DeviceInput;
import org.chromium.ui.test.transit.SoftKeyboardCondition;

import java.util.concurrent.atomic.AtomicBoolean;

/** Public transit tests for the Hub's history pane. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@EnableFeatures({
    ChromeFeatureList.HISTORY_PANE_ANDROID,
    ChromeFeatureList.ENABLE_ESCAPE_HANDLING_FOR_SECONDARY_ACTIVITIES
})
public class HistoryPaneTest {
    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    private WebPageStation mStartingPage;
    private boolean mIsLLFDevice;

    @Before
    public void setUp() {
        mStartingPage = mCtaTestRule.startOnBlankPage();
        ChromeTabbedActivity cta = mCtaTestRule.getActivity();
        final AtomicBoolean supportsKeyboard = new AtomicBoolean();
        final AtomicBoolean isTablet = new AtomicBoolean();
        runOnUiThreadBlocking(
                () -> {
                    clearHistory(cta.getProfileProviderSupplier().get().getOriginalProfile());
                    supportsKeyboard.set(DeviceInput.supportsKeyboard());
                    isTablet.set(DeviceFormFactor.isNonMultiDisplayContextOnTablet(cta));
                });
        mIsLLFDevice = supportsKeyboard.get() && isTablet.get();
    }

    @Test
    @MediumTest
    public void testEmptyView() {
        RegularTabSwitcherStation tabSwitcher = mStartingPage.openRegularTabSwitcher();
        tabSwitcher.selectHistoryPane().expectEmptyState(mIsLLFDevice);
    }

    @Test
    @MediumTest
    public void testOpenedHistoryItem_HistoryItemsAreDisplayed() {
        String urlOne =
                mCtaTestRule.getTestServer().getURL("/chrome/test/data/android/navigate/one.html");
        String urlTwo =
                mCtaTestRule.getTestServer().getURL("/chrome/test/data/android/navigate/two.html");
        RegularTabSwitcherStation tabSwitcher =
                mStartingPage
                        .loadWebPageProgrammatically(urlOne)
                        .loadWebPageProgrammatically(urlTwo)
                        .openRegularTabSwitcher();
        HistoryWithEntriesFacility history =
                tabSwitcher.selectHistoryPane().expectEntries(mIsLLFDevice);
        history.expectEntry("One");
        history.expectEntry("Two");
    }

    @Test
    @MediumTest
    public void testOpenedHistoryItem_SearchMatch() {
        String urlOne =
                mCtaTestRule.getTestServer().getURL("/chrome/test/data/android/navigate/one.html");
        String urlTwo =
                mCtaTestRule.getTestServer().getURL("/chrome/test/data/android/navigate/two.html");
        RegularTabSwitcherStation tabSwitcher =
                mStartingPage
                        .loadWebPageProgrammatically(urlOne)
                        .loadWebPageProgrammatically(urlTwo)
                        .openRegularTabSwitcher();
        HistoryPaneStation historyPaneStation = tabSwitcher.selectHistoryPane();
        HistoryWithEntriesFacility history = historyPaneStation.expectEntries(mIsLLFDevice);
        history.expectEntry("One");
        history.expectEntry("Two");

        // Search for "One" in the history search box.
        HistorySearchFacility search = history.openSearch(mIsLLFDevice);
        search.typeSearchTerm("One");

        // Verify that "One" is displayed as a match.
        history.expectEntry("One");
        history.expectNoEntry("Two");

        noopTo().waitFor(
                        new SoftKeyboardCondition(
                                historyPaneStation.getActivityElement(),
                                /* expectShowing= */ false));
    }

    @Test
    @MediumTest
    public void testOpenedHistoryItem_SingleClickOpensInSameTab() {
        String urlOne =
                mCtaTestRule.getTestServer().getURL("/chrome/test/data/android/navigate/one.html");
        String urlTwo =
                mCtaTestRule.getTestServer().getURL("/chrome/test/data/android/navigate/two.html");
        WebPageStation page =
                mStartingPage
                        .loadWebPageProgrammatically(urlOne)
                        .loadWebPageProgrammatically(urlTwo);
        HistoryWithEntriesFacility history =
                page.openRegularTabSwitcher().selectHistoryPane().expectEntries(mIsLLFDevice);
        history.expectEntry("One").selectToOpenWebPage(page, urlOne);
    }

    private void clearHistory(Profile profile) {
        BrowsingDataBridge.getForProfile(profile)
                .clearBrowsingData(
                        () -> {}, new int[] {BrowsingDataType.HISTORY}, TimePeriod.ALL_TIME);
    }
}
