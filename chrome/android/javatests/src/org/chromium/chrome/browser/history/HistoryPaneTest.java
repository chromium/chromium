// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.containsString;
import static org.junit.Assert.assertEquals;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.test.transit.hub.HubBaseStation.HUB_PANE_SWITCHER;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.BlankCTATabInitialStatePublicTransitRule;

/** Public transit tests for the Hub's history pane. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@EnableFeatures(ChromeFeatureList.HISTORY_PANE_ANDROID)
public class HistoryPaneTest {

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStatePublicTransitRule mInitialStateRule =
            new BlankCTATabInitialStatePublicTransitRule(sActivityTestRule);

    @Before
    public void setUp() {
        mInitialStateRule.startOnBlankPage();
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        runOnUiThreadBlocking(
                () -> clearHistory(cta.getProfileProviderSupplier().get().getOriginalProfile()));
    }

    @Test
    @MediumTest
    public void testEmptyView() {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        enterTabSwitcher(cta);
        enterHistoryPane();

        onViewWaiting(withText("You’ll find your history here")).check(matches(isDisplayed()));
        onViewWaiting(
                        withText(
                                "You can see the pages you’ve visited or delete them from your"
                                        + " history"))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testOpenedHistoryItem_HistoryItemsAreDisplayed() {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        String urlOne =
                sActivityTestRule
                        .getTestServer()
                        .getURL("/chrome/test/data/android/navigate/one.html");
        String urlTwo =
                sActivityTestRule
                        .getTestServer()
                        .getURL("/chrome/test/data/android/navigate/two.html");
        sActivityTestRule.loadUrl(urlOne);
        sActivityTestRule.loadUrl(urlTwo);

        enterTabSwitcher(cta);
        enterHistoryPane();

        onViewWaiting(withText("One")).check(matches(isDisplayed()));
        onViewWaiting(withText("Two")).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testOpenedHistoryItem_SearchMatch() {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        String urlOne =
                sActivityTestRule
                        .getTestServer()
                        .getURL("/chrome/test/data/android/navigate/one.html");
        String urlTwo =
                sActivityTestRule
                        .getTestServer()
                        .getURL("/chrome/test/data/android/navigate/two.html");
        sActivityTestRule.loadUrl(urlOne);
        sActivityTestRule.loadUrl(urlTwo);

        enterTabSwitcher(cta);
        enterHistoryPane();

        onViewWaiting(withText("One")).check(matches(isDisplayed()));
        onViewWaiting(withText("Two")).check(matches(isDisplayed()));

        // Search for "One" in the history search box.
        onView(withId(R.id.search_menu_id)).perform(click());
        onView(withId(R.id.search_text)).perform(replaceText("One"));

        // Verify that "One" is displayed as a match.
        onViewWaiting(allOf(withText("One"), withId(R.id.title))).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testOpenedHistoryItem_SingleClickOpensInSameTab() {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        String urlOne =
                sActivityTestRule
                        .getTestServer()
                        .getURL("/chrome/test/data/android/navigate/one.html");
        String urlTwo =
                sActivityTestRule
                        .getTestServer()
                        .getURL("/chrome/test/data/android/navigate/two.html");
        sActivityTestRule.loadUrl(urlOne);
        sActivityTestRule.loadUrl(urlTwo);
        // Load two urls, and make sure that urlTwo is currently loaded.
        assertEquals(urlTwo, cta.getTabModelSelector().getCurrentTab().getUrl().getSpec());

        enterTabSwitcher(cta);
        enterHistoryPane();

        onViewWaiting(withText("One")).perform(click());
        // When the history view is clicked, it should replace the current tab's URL.
        CriteriaHelper.pollUiThread(
                () -> urlOne.equals(cta.getTabModelSelector().getCurrentTab().getUrl().getSpec()));
    }

    private void enterHistoryPane() {
        onView(
                        allOf(
                                isDescendantOfA(HUB_PANE_SWITCHER.getViewMatcher()),
                                withContentDescription(containsString("History"))))
                .perform(click());
    }

    private void clearHistory(Profile profile) {
        BrowsingDataBridge.getForProfile(profile)
                .clearBrowsingData(
                        () -> {}, new int[] {BrowsingDataType.HISTORY}, TimePeriod.ALL_TIME);
    }
}
