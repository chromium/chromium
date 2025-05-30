// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.getTabSwitcherAncestorId;
import static org.chromium.ui.base.DeviceFormFactor.PHONE;
import static org.chromium.ui.base.DeviceFormFactor.TABLET;

import android.view.View;
import android.view.ViewGroup;

import androidx.test.espresso.ViewInteraction;
import androidx.test.espresso.matcher.ViewMatchers.Visibility;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.ActivityFinisher;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.history.BrowsingHistoryBridge;
import org.chromium.chrome.browser.history.HistoryItem;
import org.chromium.chrome.browser.history.HistoryProvider.BrowsingHistoryObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.Journeys;
import org.chromium.chrome.test.transit.hub.TabSwitcherSearchStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.ViewUtils;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Tests for search in the tab switcher. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class TabSwitcherSearchTest {
    private static final int SERVER_PORT = 13245;
    private static final String URL_PREFIX = "127.0.0.1:" + SERVER_PORT;

    // The Activity doesn't get reused because tearDown() closes it, but resetting the tab state
    // is necessary for some tests.
    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    private EmbeddedTestServer mTestServer;
    private WebPageStation mInitialPage;

    @Before
    public void setUp() {
        mTestServer =
                TabSwitcherSearchTestUtils.setServerPortAndGetTestServer(
                        mCtaTestRule.getActivityTestRule(), SERVER_PORT);
        mInitialPage = mCtaTestRule.startOnBlankPage();

        ChromeTabbedActivity cta = mCtaTestRule.getActivity();
        CriteriaHelper.pollUiThread(cta.getTabModelSelector()::isTabStateInitialized);
    }

    @After
    public void tearDown() {
        ActivityFinisher.finishAll();
    }

    @Test
    @MediumTest
    @Restriction(PHONE)
    public void testHubSearchBox_Phone() {
        ChromeTabbedActivity cta = mCtaTestRule.getActivity();
        mInitialPage.openRegularTabSwitcher();

        View tabSwitcher = cta.findViewById(R.id.tab_switcher_view_holder);
        assertEquals(ViewGroup.VISIBLE, tabSwitcher.findViewById(R.id.search_box).getVisibility());
        assertEquals(ViewGroup.GONE, tabSwitcher.findViewById(R.id.search_loupe).getVisibility());
    }

    @Test
    @MediumTest
    @Restriction(TABLET)
    public void testHubSearchLoupe_Tablet() {
        ChromeTabbedActivity cta = mCtaTestRule.getActivity();
        mInitialPage.openRegularTabSwitcher();

        View tabSwitcher = cta.findViewById(R.id.tab_switcher_view_holder);
        assertEquals(ViewGroup.GONE, tabSwitcher.findViewById(R.id.search_box).getVisibility());
        assertEquals(
                ViewGroup.VISIBLE, tabSwitcher.findViewById(R.id.search_loupe).getVisibility());
    }

    @Test
    @MediumTest
    public void testZeroPrefixSuggestions() {
        List<String> urlsToOpen =
                Arrays.asList(
                        "/chrome/test/data/android/test.html",
                        "/chrome/test/data/android/navigate/one.html");
        TabSwitcherSearchStation tabSwitcherSearchStation =
                Journeys.prepareRegularTabsWithWebPages(
                                mInitialPage, mTestServer.getURLs(urlsToOpen))
                        .openRegularTabSwitcher()
                        .openTabSwitcherSearch();
        tabSwitcherSearchStation.checkSuggestionsShown(true);

        // ZPS for open tabs only shows the most recent 4 tabs.
        verifySuggestions(urlsToOpen, /* includePrefix= */ true);

        // Check the header text.
        onView(withText("Last open tabs")).check(matches(isCompletelyDisplayed()));
    }

    @Test
    @MediumTest
    public void testZeroPrefixSuggestions_OpenSuggestion() {
        ChromeTabbedActivity cta = mCtaTestRule.getActivity();
        List<String> urlsToOpen =
                Arrays.asList(
                        "/chrome/test/data/android/test.html",
                        "/chrome/test/data/android/navigate/one.html");
        TabSwitcherSearchStation tabSwitcherSearchStation =
                Journeys.prepareRegularTabsWithWebPages(
                                mInitialPage, mTestServer.getURLs(urlsToOpen))
                        .openRegularTabSwitcher()
                        .openTabSwitcherSearch();
        tabSwitcherSearchStation.checkSuggestionsShown(true);

        clickSuggestion(urlsToOpen.get(0), /* includePrefix= */ true);
        CriteriaHelper.pollUiThread(
                () -> ActivityState.RESUMED == ApplicationStatus.getStateForActivity(cta));
        CriteriaHelper.pollUiThread(
                () ->
                        ActivityState.DESTROYED
                                == ApplicationStatus.getStateForActivity(
                                        tabSwitcherSearchStation.getActivity()));
        CriteriaHelper.pollUiThread(
                () -> cta.getLayoutManager().isLayoutVisible(LayoutType.BROWSING));
        assertEquals(
                mCtaTestRule.getTestServer().getURL(urlsToOpen.get(0)),
                cta.getCurrentTabModel().getCurrentTabSupplier().get().getUrl().getSpec());
    }

    @Test
    @MediumTest
    public void testZeroPrefixSuggestions_OpenSameTab() {
        ChromeTabbedActivity cta = mCtaTestRule.getActivity();
        List<String> urlsToOpen = Arrays.asList("/chrome/test/data/android/navigate/one.html");
        TabSwitcherSearchStation tabSwitcherSearchStation =
                Journeys.prepareRegularTabsWithWebPages(
                                mInitialPage, mTestServer.getURLs(urlsToOpen))
                        .openRegularTabSwitcher()
                        .openTabSwitcherSearch();
        tabSwitcherSearchStation.checkSuggestionsShown(true);

        clickSuggestion(urlsToOpen.get(0), /* includePrefix= */ true);
        CriteriaHelper.pollUiThread(
                () -> ActivityState.RESUMED == ApplicationStatus.getStateForActivity(cta));
        CriteriaHelper.pollUiThread(
                () ->
                        ActivityState.DESTROYED
                                == ApplicationStatus.getStateForActivity(
                                        tabSwitcherSearchStation.getActivity()));
        CriteriaHelper.pollUiThread(
                () -> cta.getLayoutManager().isLayoutVisible(LayoutType.BROWSING));
        assertEquals(
                mCtaTestRule.getTestServer().getURL(urlsToOpen.get(0)),
                cta.getCurrentTabModel().getCurrentTabSupplier().get().getUrl().getSpec());
    }

    @Test
    @MediumTest
    // Regression test for the currently selected tab being included/excluded randomly.
    public void testZeroPrefixSuggestions_IgnoresHiddenTabs() {
        ChromeTabbedActivity cta = mCtaTestRule.getActivity();
        List<String> urlsToOpen =
                Arrays.asList(
                        "/chrome/test/data/android/test.html",
                        "/chrome/test/data/android/navigate/one.html");
        TabSwitcherSearchStation tabSwitcherSearchStation =
                Journeys.prepareRegularTabsWithWebPages(
                                mInitialPage, mTestServer.getURLs(urlsToOpen))
                        .openRegularTabSwitcher()
                        .openTabSwitcherSearch();
        tabSwitcherSearchStation.checkSuggestionsShown(true);

        // ZPS for open tabs only shows the most recent 4 tabs.
        verifySuggestions(urlsToOpen, /* includePrefix= */ true);

        // Check the header text.
        onView(withText("Last open tabs")).check(matches(isCompletelyDisplayed()));

        closeSearchAndVerify();
        TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad(cta);

        // ZPS for open tabs only shows the most recent 4 tabs.
        verifySuggestions(urlsToOpen, /* includePrefix= */ true);

        // Check the header text.
        onView(withText("Last open tabs")).check(matches(isCompletelyDisplayed()));
    }

    @Test
    @MediumTest
    public void testZeroPrefixSuggestions_Incognito() {
        List<String> urlsToOpen = Arrays.asList("/chrome/test/data/android/navigate/one.html");
        TabSwitcherSearchStation tabSwitcherSearchStation =
                Journeys.createIncognitoTabsWithWebPages(
                                mInitialPage, mTestServer.getURLs(urlsToOpen))
                        .openIncognitoTabSwitcher()
                        .openTabSwitcherSearch();
        tabSwitcherSearchStation.checkSuggestionsShown(false);
    }

    @Test
    @LargeTest
    public void testZeroPrefixSuggestions_duplicateUrls() {
        List<String> urlsToOpen =
                Arrays.asList(
                        "/chrome/test/data/android/test.html",
                        "/chrome/test/data/android/test.html");
        TabSwitcherSearchStation tabSwitcherSearchStation =
                Journeys.prepareRegularTabsWithWebPages(
                                mInitialPage, mTestServer.getURLs(urlsToOpen))
                        .openRegularTabSwitcher()
                        .openTabSwitcherSearch();
        tabSwitcherSearchStation.checkSuggestionsShown(true);

        // Tab URLs will be de-duped.
        verifySuggestions(urlsToOpen, /* includePrefix= */ true);
    }

    @Test
    @MediumTest
    public void testTypedSuggestions() {
        List<String> urlsToOpen = Arrays.asList("/chrome/test/data/android/navigate/one.html");
        TabSwitcherSearchStation tabSwitcherSearchStation =
                Journeys.prepareRegularTabsWithWebPages(
                                mInitialPage, mTestServer.getURLs(urlsToOpen))
                        .openRegularTabSwitcher()
                        .openTabSwitcherSearch();
        tabSwitcherSearchStation.typeInOmnibox("one.html");
        tabSwitcherSearchStation.checkSuggestionsShown(true);

        verifySuggestions(urlsToOpen, /* includePrefix= */ true);
    }

    @Test
    @MediumTest
    public void testTypedSuggestions_OpenSuggestion() {
        ChromeTabbedActivity cta = mCtaTestRule.getActivity();
        List<String> urlsToOpen =
                Arrays.asList(
                        "/chrome/test/data/android/test.html",
                        "/chrome/test/data/android/test.html");
        TabSwitcherSearchStation tabSwitcherSearchStation =
                Journeys.prepareRegularTabsWithWebPages(
                                mInitialPage, mTestServer.getURLs(urlsToOpen))
                        .openRegularTabSwitcher()
                        .openTabSwitcherSearch();
        tabSwitcherSearchStation.typeInOmnibox("test.html");
        tabSwitcherSearchStation.waitForSuggestionAtIndexWithTitleText(0, "android/test.html");

        clickSuggestion(urlsToOpen.get(0), /* includePrefix= */ true);
        CriteriaHelper.pollUiThread(
                () -> ActivityState.RESUMED == ApplicationStatus.getStateForActivity(cta));
        CriteriaHelper.pollUiThread(
                () ->
                        ActivityState.DESTROYED
                                == ApplicationStatus.getStateForActivity(
                                        tabSwitcherSearchStation.getActivity()));
        CriteriaHelper.pollUiThread(
                () -> cta.getLayoutManager().isLayoutVisible(LayoutType.BROWSING));
        assertEquals(
                mCtaTestRule.getTestServer().getURL(urlsToOpen.get(0)),
                cta.getCurrentTabModel().getCurrentTabSupplier().get().getUrl().getSpec());
    }

    @Test
    @MediumTest
    public void testTypedSuggestions_OpenSameTab() {
        ChromeTabbedActivity cta = mCtaTestRule.getActivity();
        List<String> urlsToOpen = Arrays.asList("/chrome/test/data/android/navigate/one.html");
        TabSwitcherSearchStation tabSwitcherSearchStation =
                Journeys.prepareRegularTabsWithWebPages(
                                mInitialPage, mTestServer.getURLs(urlsToOpen))
                        .openRegularTabSwitcher()
                        .openTabSwitcherSearch();
        tabSwitcherSearchStation.typeInOmnibox("one.html");
        tabSwitcherSearchStation.waitForSuggestionAtIndexWithTitleText(0, "One");

        clickSuggestion(urlsToOpen.get(0), /* includePrefix= */ true);
        CriteriaHelper.pollUiThread(
                () -> ActivityState.RESUMED == ApplicationStatus.getStateForActivity(cta));
        CriteriaHelper.pollUiThread(
                () ->
                        ActivityState.DESTROYED
                                == ApplicationStatus.getStateForActivity(
                                        tabSwitcherSearchStation.getActivity()));
        CriteriaHelper.pollUiThread(
                () -> cta.getLayoutManager().isLayoutVisible(LayoutType.BROWSING));
        assertEquals(
                mCtaTestRule.getTestServer().getURL(urlsToOpen.get(0)),
                cta.getCurrentTabModel().getCurrentTabSupplier().get().getUrl().getSpec());
    }

    @Test
    @MediumTest
    public void testTypedSuggestions_OpenSuggestionWithEnter() {
        ChromeTabbedActivity cta = mCtaTestRule.getActivity();
        List<String> urlsToOpen =
                Arrays.asList(
                        "/chrome/test/data/android/navigate/one.html",
                        "/chrome/test/data/android/test.html");
        TabSwitcherSearchStation tabSwitcherSearchStation =
                Journeys.prepareRegularTabsWithWebPages(
                                mInitialPage, mTestServer.getURLs(urlsToOpen))
                        .openRegularTabSwitcher()
                        .openTabSwitcherSearch();
        tabSwitcherSearchStation.typeInOmnibox("one.html");
        tabSwitcherSearchStation.waitForSuggestionAtIndexWithTitleText(0, "One");
        tabSwitcherSearchStation.pressEnter();

        CriteriaHelper.pollUiThread(
                () -> ActivityState.RESUMED == ApplicationStatus.getStateForActivity(cta));
        CriteriaHelper.pollUiThread(
                () ->
                        ActivityState.DESTROYED
                                == ApplicationStatus.getStateForActivity(
                                        tabSwitcherSearchStation.getActivity()));
        CriteriaHelper.pollUiThread(
                () -> cta.getLayoutManager().isLayoutVisible(LayoutType.BROWSING));
    }

    @Test
    @MediumTest
    public void testTypedSuggestions_Incognito() {
        List<String> urlsToOpen = Arrays.asList("/chrome/test/data/android/navigate/one.html");
        TabSwitcherSearchStation tabSwitcherSearchStation =
                Journeys.createIncognitoTabsWithWebPages(
                                mInitialPage, mTestServer.getURLs(urlsToOpen))
                        .openIncognitoTabSwitcher()
                        .openTabSwitcherSearch();
        tabSwitcherSearchStation.typeInOmnibox("one.html");
        tabSwitcherSearchStation.waitForSuggestionAtIndexWithTitleText(0, "One");

        verifySuggestions(urlsToOpen, /* includePrefix= */ true);
    }

    @Test
    @MediumTest
    public void testSearchActivityBackButton() {
        mInitialPage.openRegularTabSwitcher().openTabSwitcherSearch();
        closeSearchAndVerify();
    }

    @Test
    @MediumTest
    public void testTypedSuggestions_OpenSearchSuggestion() {
        ChromeTabbedActivity cta = mCtaTestRule.getActivity();
        TabSwitcherSearchStation tabSwitcherSearchStation =
                mInitialPage.openRegularTabSwitcher().openTabSwitcherSearch();
        tabSwitcherSearchStation.typeInOmnibox("foobar");
        tabSwitcherSearchStation.waitForSectionAtIndexWithText(0, "Search the web");
        tabSwitcherSearchStation.waitForSuggestionAtIndexWithTitleText(1, "foobar");

        clickSuggestion("foobar", /* includePrefix= */ false);
        CriteriaHelper.pollUiThread(
                () -> ActivityState.RESUMED == ApplicationStatus.getStateForActivity(cta));
        CriteriaHelper.pollUiThread(
                () ->
                        ActivityState.DESTROYED
                                == ApplicationStatus.getStateForActivity(
                                        tabSwitcherSearchStation.getActivity()));
        CriteriaHelper.pollUiThread(
                () -> cta.getLayoutManager().isLayoutVisible(LayoutType.BROWSING));

        TabModel currentTabModel = cta.getCurrentTabModel();
        Tab currentTab = currentTabModel.getCurrentTabSupplier().get();
        assertTrue(currentTab.getUrl().getSpec().contains("foobar"));
        assertFalse(currentTabModel.isOffTheRecord());
    }

    @Test
    @MediumTest
    public void testTypedSuggestions_OpenSearchSuggestion_Incognito() {
        ChromeTabbedActivity cta = mCtaTestRule.getActivity();
        List<String> urlsToOpen = Arrays.asList("/chrome/test/data/android/navigate/one.html");
        TabSwitcherSearchStation tabSwitcherSearchStation =
                Journeys.createIncognitoTabsWithWebPages(
                                mInitialPage, mTestServer.getURLs(urlsToOpen))
                        .openIncognitoTabSwitcher()
                        .openTabSwitcherSearch();
        tabSwitcherSearchStation.typeInOmnibox("foobar");
        tabSwitcherSearchStation.waitForSectionAtIndexWithText(0, "Search the web");
        tabSwitcherSearchStation.waitForSuggestionAtIndexWithTitleText(1, "foobar");

        clickSuggestion("foobar", /* includePrefix= */ false);
        CriteriaHelper.pollUiThread(
                () -> ActivityState.RESUMED == ApplicationStatus.getStateForActivity(cta));
        CriteriaHelper.pollUiThread(
                () ->
                        ActivityState.DESTROYED
                                == ApplicationStatus.getStateForActivity(
                                        tabSwitcherSearchStation.getActivity()));
        CriteriaHelper.pollUiThread(
                () -> cta.getLayoutManager().isLayoutVisible(LayoutType.BROWSING));
        TabModel currentTabModel = cta.getCurrentTabModel();
        Tab currentTab = currentTabModel.getCurrentTabSupplier().get();
        assertTrue(currentTab.getUrl().getSpec().contains("foobar"));
        assertTrue(currentTabModel.isOffTheRecord());
    }

    @Test
    @MediumTest
    @RequiresRestart("Adding the bookmark affects suggestions in subsequent tests")
    // TODO(crbug.com/394401323): Add some PT station for searching bookmarks.
    public void testBookmarkSuggestions() {
        WebPageStation openPage =
                mInitialPage
                        .openNewTabFast()
                        .loadWebPageProgrammatically(
                                mTestServer.getURL("/chrome/test/data/android/navigate/one.html"));
        BookmarkTestUtil.waitForBookmarkModelLoaded();

        // Click star button to bookmark the current tab.
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                mCtaTestRule.getActivity(),
                R.id.bookmark_this_page_id);

        TabSwitcherSearchStation tabSwitcherSearchStation =
                openPage.loadWebPageProgrammatically(
                                mTestServer.getURL("/chrome/test/data/android/test.html"))
                        .openRegularTabSwitcher()
                        .openTabSwitcherSearch();
        tabSwitcherSearchStation.typeInOmnibox("one.html");
        tabSwitcherSearchStation.waitForSectionAtIndexWithText(0, "Bookmarks");
        tabSwitcherSearchStation.waitForSuggestionAtIndexWithTitleText(1, "One");
    }

    @Test
    @MediumTest
    // TODO(crbug.com/394401463): Add some PT station for searching history.
    public void testHistorySuggestions() throws TimeoutException {
        TabSwitcherSearchStation tabSwitcherSearchStation =
                mInitialPage
                        .openNewTabFast()
                        .loadWebPageProgrammatically(
                                mTestServer.getURL("/chrome/test/data/android/navigate/one.html"))
                        .loadWebPageProgrammatically(
                                mTestServer.getURL("/chrome/test/data/android/test.html"))
                        .openRegularTabSwitcher()
                        .openTabSwitcherSearch();

        CallbackHelper helper = new CallbackHelper();
        BrowsingHistoryBridge historyBridge =
                runOnUiThreadBlocking(
                        () ->
                                new BrowsingHistoryBridge(
                                        mCtaTestRule
                                                .getActivity()
                                                .getProfileProviderSupplier()
                                                .get()
                                                .getOriginalProfile()));
        historyBridge.setObserver(
                new BrowsingHistoryObserver() {
                    @Override
                    public void onQueryHistoryComplete(
                            List<HistoryItem> items, boolean hasMorePotentialMatches) {
                        if (items.size() > 0) {
                            for (HistoryItem item : items) {
                                if (item.getTitle().contains("One")) {
                                    helper.notifyCalled();
                                }
                            }
                        }
                    }

                    @Override
                    public void onHistoryDeleted() {}

                    @Override
                    public void hasOtherFormsOfBrowsingData(boolean hasOtherForms) {}

                    @Override
                    public void onQueryAppsComplete(List<String> items) {}
                });
        runOnUiThreadBlocking(() -> historyBridge.queryHistory("one.html", /* appId= */ null));
        helper.waitForNext();

        tabSwitcherSearchStation.typeInOmnibox("One");
        tabSwitcherSearchStation.waitForSectionAtIndexWithText(0, "History");
        tabSwitcherSearchStation.waitForSuggestionAtIndexWithTitleText(1, "One");
    }

    private void closeSearchAndVerify() {
        // Click the back button which is setup as the status view icon.
        onView(withId(R.id.location_bar_status)).perform(click());

        // Check that the tab switcher is now fully showing again.
        onView(
                        allOf(
                                isDescendantOfA(
                                        withId(
                                                getTabSwitcherAncestorId(
                                                        mCtaTestRule.getActivity()))),
                                withId(R.id.tab_list_recycler_view)))
                .check(matches(isCompletelyDisplayed()));
    }

    private void verifySuggestions(List<String> suggestionUrls, boolean includePrefix) {
        for (int i = 0; i < suggestionUrls.size(); i++) {
            String url = adjustUrl(suggestionUrls.get(i), includePrefix);
            findMatchWithTextAndId(url, includePrefix ? R.id.line_2 : R.id.line_1)
                    .check(matches(isDisplayed()));
        }
    }

    private void clickSuggestion(String url, boolean includePrefix) {
        url = adjustUrl(url, includePrefix);
        findMatchWithTextAndId(url, includePrefix ? R.id.line_2 : R.id.line_1).perform(click());
    }

    private ViewInteraction findMatchWithTextAndId(String text, int id) {
        return ViewUtils.onViewWaiting(
                allOf(withId(id), withText(text), withEffectiveVisibility(Visibility.VISIBLE)));
    }

    private String adjustUrl(String url, boolean includePrefix) {
        if (includePrefix) {
            return URL_PREFIX + url;
        }
        return url;
    }
}
