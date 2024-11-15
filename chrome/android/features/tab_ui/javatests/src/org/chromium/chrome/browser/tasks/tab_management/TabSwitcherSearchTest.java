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

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
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
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.ExecutionException;

/** Tests for search in the tab switcher. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(OmniboxFeatureList.ANDROID_HUB_SEARCH)
public class TabSwitcherSearchTest {
    private static final int SERVER_PORT = 13245;
    private static final String URL_PREFIX = "127.0.0.1:" + SERVER_PORT;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws ExecutionException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestServer =
                TabSwitcherSearchTestUtils.setServerPortAndGetTestServer(
                        mActivityTestRule, SERVER_PORT);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(cta.getTabModelSelector()::isTabStateInitialized);
    }

    @After
    public void tearDown() {
        ActivityFinisher.finishAll();
    }

    @Test
    @MediumTest
    public void testSearchClickedOpensSearchActivity() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad(cta);
    }

    @Test
    @MediumTest
    @Restriction(PHONE)
    public void testHubSearchBox_Phone() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        View tabSwitcher = cta.findViewById(R.id.tab_switcher_view_holder);
        assertEquals(ViewGroup.VISIBLE, tabSwitcher.findViewById(R.id.search_box).getVisibility());
        assertEquals(ViewGroup.GONE, tabSwitcher.findViewById(R.id.search_loupe).getVisibility());
    }

    @Test
    @MediumTest
    @Restriction(TABLET)
    public void testHubSearchLoupe_Tablet() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

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
        TabSwitcherSearchTestUtils.openUrls(mActivityTestRule, urlsToOpen, /* incognito= */ false);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        SearchActivity searchActivity =
                TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad(cta);

        // ZPS for open tabs only shows the most recent 4 tabs.
        verifySuggestions(urlsToOpen, /* includePrefix= */ true);

        // Check the header text.
        onView(withText("Last open tabs")).check(matches(isCompletelyDisplayed()));
    }

    @Test
    @MediumTest
    public void testZeroPrefixSuggestions_OpenSuggestion() {
        List<String> urlsToOpen =
                Arrays.asList(
                        "/chrome/test/data/android/test.html",
                        "/chrome/test/data/android/navigate/one.html");
        TabSwitcherSearchTestUtils.openUrls(mActivityTestRule, urlsToOpen, /* incognito= */ false);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        SearchActivity searchActivity =
                TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad(cta);
        assertEquals(ActivityState.STOPPED, ApplicationStatus.getStateForActivity(cta));
        assertEquals(ActivityState.RESUMED, ApplicationStatus.getStateForActivity(searchActivity));

        clickSuggestion(urlsToOpen.get(0), /* includePrefix= */ true);
        CriteriaHelper.pollUiThread(
                () -> ActivityState.RESUMED == ApplicationStatus.getStateForActivity(cta));
        CriteriaHelper.pollUiThread(
                () ->
                        ActivityState.DESTROYED
                                == ApplicationStatus.getStateForActivity(searchActivity));
        CriteriaHelper.pollUiThread(
                () -> cta.getLayoutManager().isLayoutVisible(LayoutType.BROWSING));
        assertEquals(
                mActivityTestRule.getTestServer().getURL(urlsToOpen.get(0)),
                cta.getCurrentTabModel().getCurrentTabSupplier().get().getUrl().getSpec());
    }

    @Test
    @MediumTest
    public void testZeroPrefixSuggestions_OpenSameTab() {
        List<String> urlsToOpen = Arrays.asList("/chrome/test/data/android/navigate/one.html");
        TabSwitcherSearchTestUtils.openUrls(mActivityTestRule, urlsToOpen, /* incognito= */ false);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        SearchActivity searchActivity =
                TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad(cta);
        assertEquals(ActivityState.STOPPED, ApplicationStatus.getStateForActivity(cta));
        assertEquals(ActivityState.RESUMED, ApplicationStatus.getStateForActivity(searchActivity));

        clickSuggestion(urlsToOpen.get(0), /* includePrefix= */ true);
        CriteriaHelper.pollUiThread(
                () -> ActivityState.RESUMED == ApplicationStatus.getStateForActivity(cta));
        CriteriaHelper.pollUiThread(
                () ->
                        ActivityState.DESTROYED
                                == ApplicationStatus.getStateForActivity(searchActivity));
        CriteriaHelper.pollUiThread(
                () -> cta.getLayoutManager().isLayoutVisible(LayoutType.BROWSING));
        assertEquals(
                mActivityTestRule.getTestServer().getURL(urlsToOpen.get(0)),
                cta.getCurrentTabModel().getCurrentTabSupplier().get().getUrl().getSpec());
    }

    @Test
    @MediumTest
    // Regression test for the currently selected tab being included/excluded randomly.
    public void testZeroPrefixSuggestions_IgnoresHiddenTabs() {
        List<String> urlsToOpen =
                Arrays.asList(
                        "/chrome/test/data/android/test.html",
                        "/chrome/test/data/android/navigate/one.html");
        TabSwitcherSearchTestUtils.openUrls(mActivityTestRule, urlsToOpen, /* incognito= */ false);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        SearchActivity searchActivity =
                TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad(cta);

        // ZPS for open tabs only shows the most recent 4 tabs.
        ViewGroup suggestions = searchActivity.findViewById(R.id.omnibox_suggestions_dropdown);
        verifySuggestions(urlsToOpen, /* includePrefix= */ true);

        // Check the header text.
        onView(withText("Last open tabs")).check(matches(isCompletelyDisplayed()));

        closeSearchAndVerify();
        searchActivity =
                TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad(cta);

        // ZPS for open tabs only shows the most recent 4 tabs.
        suggestions = searchActivity.findViewById(R.id.omnibox_suggestions_dropdown);
        verifySuggestions(urlsToOpen, /* includePrefix= */ true);

        // Check the header text.
        onView(withText("Last open tabs")).check(matches(isCompletelyDisplayed()));
    }

    @Test
    @MediumTest
    public void testZeroPrefixSuggestions_Incognito() {
        List<String> urlsToOpen = Arrays.asList("/chrome/test/data/android/navigate/one.html");
        TabSwitcherSearchTestUtils.openUrls(mActivityTestRule, urlsToOpen, /* incognito= */ true);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        SearchActivity searchActivity =
                TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad(cta);

        OmniboxTestUtils omniboxTestUtils = new OmniboxTestUtils(searchActivity);
        omniboxTestUtils.checkSuggestionsShown(/* shown= */ false);
    }

    @Test
    @LargeTest
    public void testZeroPrefixSuggestions_duplicateUrls() {
        List<String> urlsToOpen =
                Arrays.asList(
                        "/chrome/test/data/android/test.html",
                        "/chrome/test/data/android/test.html");
        TabSwitcherSearchTestUtils.openUrls(mActivityTestRule, urlsToOpen, /* incognito= */ false);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        SearchActivity searchActivity =
                TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad(cta);

        // Tab URLs will be de-duped.
        verifySuggestions(urlsToOpen, /* includePrefix= */ true);
    }

    @Test
    @MediumTest
    public void testTypedSuggestions() {
        List<String> urlsToOpen = Arrays.asList("/chrome/test/data/android/navigate/one.html");
        TabSwitcherSearchTestUtils.openUrls(mActivityTestRule, urlsToOpen, /* incognito= */ false);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        SearchActivity searchActivity =
                TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad(cta);

        OmniboxTestUtils omniboxTestUtils = new OmniboxTestUtils(searchActivity);
        omniboxTestUtils.requestFocus();
        omniboxTestUtils.typeText("one.html", /* execute= */ false);
        omniboxTestUtils.waitAnimationsComplete();

        verifySuggestions(urlsToOpen, /* includePrefix= */ true);
    }

    @Test
    @MediumTest
    public void testTypedSuggestions_OpenSuggestion() {
        List<String> urlsToOpen =
                Arrays.asList(
                        "/chrome/test/data/android/test.html",
                        "/chrome/test/data/android/test.html");
        TabSwitcherSearchTestUtils.openUrls(mActivityTestRule, urlsToOpen, /* incognito= */ false);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        SearchActivity searchActivity =
                TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad(cta);
        assertEquals(ActivityState.STOPPED, ApplicationStatus.getStateForActivity(cta));
        assertEquals(ActivityState.RESUMED, ApplicationStatus.getStateForActivity(searchActivity));

        OmniboxTestUtils omniboxTestUtils = new OmniboxTestUtils(searchActivity);
        omniboxTestUtils.requestFocus();
        omniboxTestUtils.typeText(urlsToOpen.get(0), /* execute= */ false);
        omniboxTestUtils.waitAnimationsComplete();

        clickSuggestion(urlsToOpen.get(0), /* includePrefix= */ true);
        CriteriaHelper.pollUiThread(
                () -> ActivityState.RESUMED == ApplicationStatus.getStateForActivity(cta));
        CriteriaHelper.pollUiThread(
                () ->
                        ActivityState.DESTROYED
                                == ApplicationStatus.getStateForActivity(searchActivity));
        CriteriaHelper.pollUiThread(
                () -> cta.getLayoutManager().isLayoutVisible(LayoutType.BROWSING));
        assertEquals(
                mActivityTestRule.getTestServer().getURL(urlsToOpen.get(0)),
                cta.getCurrentTabModel().getCurrentTabSupplier().get().getUrl().getSpec());
    }

    @Test
    @MediumTest
    public void testTypedSuggestions_OpenSameTab() {
        List<String> urlsToOpen = Arrays.asList("/chrome/test/data/android/navigate/one.html");
        TabSwitcherSearchTestUtils.openUrls(mActivityTestRule, urlsToOpen, /* incognito= */ false);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        SearchActivity searchActivity =
                TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad(cta);
        assertEquals(ActivityState.STOPPED, ApplicationStatus.getStateForActivity(cta));
        assertEquals(ActivityState.RESUMED, ApplicationStatus.getStateForActivity(searchActivity));

        OmniboxTestUtils omniboxTestUtils = new OmniboxTestUtils(searchActivity);
        omniboxTestUtils.requestFocus();
        omniboxTestUtils.typeText("one.html", /* execute= */ false);
        omniboxTestUtils.waitAnimationsComplete();

        clickSuggestion(urlsToOpen.get(0), /* includePrefix= */ true);
        CriteriaHelper.pollUiThread(
                () -> ActivityState.RESUMED == ApplicationStatus.getStateForActivity(cta));
        CriteriaHelper.pollUiThread(
                () ->
                        ActivityState.DESTROYED
                                == ApplicationStatus.getStateForActivity(searchActivity));
        CriteriaHelper.pollUiThread(
                () -> cta.getLayoutManager().isLayoutVisible(LayoutType.BROWSING));
        assertEquals(
                mActivityTestRule.getTestServer().getURL(urlsToOpen.get(0)),
                cta.getCurrentTabModel().getCurrentTabSupplier().get().getUrl().getSpec());
    }

    @Test
    @MediumTest
    public void testTypedSuggestions_Incognito() {
        List<String> urlsToOpen = Arrays.asList("/chrome/test/data/android/navigate/one.html");
        TabSwitcherSearchTestUtils.openUrls(mActivityTestRule, urlsToOpen, /* incognito= */ true);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        SearchActivity searchActivity =
                TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad(cta);

        OmniboxTestUtils omniboxTestUtils = new OmniboxTestUtils(searchActivity);
        omniboxTestUtils.requestFocus();
        omniboxTestUtils.typeText("one.html", /* execute= */ false);
        omniboxTestUtils.waitAnimationsComplete();

        verifySuggestions(urlsToOpen, /* includePrefix= */ true);
    }

    @Test
    @MediumTest
    public void testSearchActivityBackButton() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad(cta);
        closeSearchAndVerify();
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
                                                        mActivityTestRule.getActivity()))),
                                withId(R.id.tab_list_recycler_view)))
                .check(matches(isCompletelyDisplayed()));
    }

    @Test
    @MediumTest
    public void testTypedSuggestions_OpenSearchSuggestion() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        SearchActivity searchActivity =
                TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad(cta);
        assertEquals(ActivityState.STOPPED, ApplicationStatus.getStateForActivity(cta));
        assertEquals(ActivityState.RESUMED, ApplicationStatus.getStateForActivity(searchActivity));

        OmniboxTestUtils omniboxTestUtils = new OmniboxTestUtils(searchActivity);
        omniboxTestUtils.requestFocus();
        omniboxTestUtils.typeText("foobar", /* execute= */ false);
        omniboxTestUtils.waitAnimationsComplete();

        clickSuggestion("foobar", /* includePrefix= */ false);
        CriteriaHelper.pollUiThread(
                () -> ActivityState.RESUMED == ApplicationStatus.getStateForActivity(cta));
        CriteriaHelper.pollUiThread(
                () ->
                        ActivityState.DESTROYED
                                == ApplicationStatus.getStateForActivity(searchActivity));
        CriteriaHelper.pollUiThread(
                () -> cta.getLayoutManager().isLayoutVisible(LayoutType.BROWSING));
        assertTrue(
                cta.getCurrentTabModel()
                        .getCurrentTabSupplier()
                        .get()
                        .getUrl()
                        .getSpec()
                        .contains("foobar"));
        assertFalse(cta.getCurrentTabModel().isIncognitoBranded());
    }

    @Test
    @MediumTest
    public void testTypedSuggestions_OpenSearchSuggestion_Incognito() {
        List<String> urlsToOpen = Arrays.asList("/chrome/test/data/android/navigate/one.html");
        TabSwitcherSearchTestUtils.openUrls(mActivityTestRule, urlsToOpen, /* incognito= */ true);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        SearchActivity searchActivity =
                TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad(cta);
        assertEquals(ActivityState.STOPPED, ApplicationStatus.getStateForActivity(cta));
        assertEquals(ActivityState.RESUMED, ApplicationStatus.getStateForActivity(searchActivity));

        OmniboxTestUtils omniboxTestUtils = new OmniboxTestUtils(searchActivity);
        omniboxTestUtils.requestFocus();
        omniboxTestUtils.typeText("foobar", /* execute= */ false);
        omniboxTestUtils.waitAnimationsComplete();

        clickSuggestion("foobar", /* includePrefix= */ false);
        CriteriaHelper.pollUiThread(
                () -> ActivityState.RESUMED == ApplicationStatus.getStateForActivity(cta));
        CriteriaHelper.pollUiThread(
                () ->
                        ActivityState.DESTROYED
                                == ApplicationStatus.getStateForActivity(searchActivity));
        CriteriaHelper.pollUiThread(
                () -> cta.getLayoutManager().isLayoutVisible(LayoutType.BROWSING));
        assertTrue(
                cta.getCurrentTabModel()
                        .getCurrentTabSupplier()
                        .get()
                        .getUrl()
                        .getSpec()
                        .contains("foobar"));
        assertTrue(cta.getCurrentTabModel().isIncognitoBranded());
    }

    @Test
    @MediumTest
    @EnableFeatures(OmniboxFeatureList.ANDROID_HUB_SEARCH + ":enable_bookmark_provider/true")
    public void testBookmarkSuggestions() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        mActivityTestRule.loadUrl(
                mActivityTestRule.getTestServer().getURL("/chrome/test/data/android/test.html"));
        BookmarkTestUtil.waitForBookmarkModelLoaded();
        // Click star button to bookmark the current tab.
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(),
                R.id.bookmark_this_page_id);

        mActivityTestRule.loadUrl(
                mActivityTestRule
                        .getTestServer()
                        .getURL("/chrome/test/data/android/navigate/one.html"));
        enterTabSwitcher(cta);

        SearchActivity searchActivity =
                TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad(cta);
        assertEquals(ActivityState.STOPPED, ApplicationStatus.getStateForActivity(cta));
        assertEquals(ActivityState.RESUMED, ApplicationStatus.getStateForActivity(searchActivity));

        OmniboxTestUtils omniboxTestUtils = new OmniboxTestUtils(searchActivity);
        omniboxTestUtils.requestFocus();
        omniboxTestUtils.typeText("test.html", /* execute= */ false);
        omniboxTestUtils.waitAnimationsComplete();

        verifySuggestions(
                Arrays.asList("/chrome/test/data/android/test.html"), /* includePrefix= */ true);
        onView(withText("Bookmarks")).check(matches(isCompletelyDisplayed()));
    }

    @Test
    @MediumTest
    @EnableFeatures(OmniboxFeatureList.ANDROID_HUB_SEARCH + ":enable_history_provider/true")
    public void testHistorySuggestions() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        mActivityTestRule.loadUrl(
                mActivityTestRule.getTestServer().getURL("/chrome/test/data/android/test.html"));
        mActivityTestRule.loadUrl(
                mActivityTestRule
                        .getTestServer()
                        .getURL("/chrome/test/data/android/navigate/one.html"));
        enterTabSwitcher(cta);

        SearchActivity searchActivity =
                TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad(cta);
        assertEquals(ActivityState.STOPPED, ApplicationStatus.getStateForActivity(cta));
        assertEquals(ActivityState.RESUMED, ApplicationStatus.getStateForActivity(searchActivity));

        OmniboxTestUtils omniboxTestUtils = new OmniboxTestUtils(searchActivity);
        omniboxTestUtils.requestFocus();
        omniboxTestUtils.typeText("test.html", /* execute= */ false);
        omniboxTestUtils.waitAnimationsComplete();

        verifySuggestions(
                Arrays.asList("/chrome/test/data/android/test.html"), /* includePrefix= */ true);
        onView(withText("History")).check(matches(isDisplayed()));
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
        return onView(
                allOf(withId(id), withText(text), withEffectiveVisibility(Visibility.VISIBLE)));
    }

    private String adjustUrl(String url, boolean includePrefix) {
        if (includePrefix) {
            return URL_PREFIX + url;
        }
        return url;
    }
}
