// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;

import android.view.ViewGroup;

import androidx.test.espresso.matcher.ViewMatchers.Visibility;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.ActivityFinisher;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.ExecutionException;

/** Tests for search in the tab switcher. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.ANDROID_HUB_SEARCH)
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

        TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad();
    }

    @Test
    @MediumTest
    public void testZeroPrefixSuggestions() {
        List<String> urlsToOpen = Arrays.asList("/chrome/test/data/android/navigate/one.html");
        TabSwitcherSearchTestUtils.openUrls(mActivityTestRule, urlsToOpen, /* incognito= */ false);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        SearchActivity searchActivity =
                TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad();

        // ZPS for open tabs only shows the most recent 4 tabs.
        ViewGroup suggestions = searchActivity.findViewById(R.id.omnibox_suggestions_dropdown);
        verifySuggestions(
                suggestions,
                Arrays.asList("/chrome/test/data/android/navigate/one.html", "about:blank"),
                "Last open tabs");

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
                TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad();

        OmniboxTestUtils omniboxTestUtils = new OmniboxTestUtils(searchActivity);
        omniboxTestUtils.checkSuggestionsShown(/* shown= */ false);
    }

    @Test
    @LargeTest
    public void testZeroPrefixSuggestions_duplicateUrls() {
        mActivityTestRule.loadUrl(mTestServer.getURL("/chrome/test/data/android/test.html"));
        List<String> urlsToOpen = Arrays.asList("/chrome/test/data/android/test.html");
        TabSwitcherSearchTestUtils.openUrls(mActivityTestRule, urlsToOpen, /* incognito= */ false);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        SearchActivity searchActivity =
                TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad();

        // Tab URLs will be de-duped.
        ViewGroup suggestions = searchActivity.findViewById(R.id.omnibox_suggestions_dropdown);
        verifySuggestions(
                suggestions,
                Arrays.asList("/chrome/test/data/android/test.html"),
                "Last open tabs");
    }

    @Test
    @MediumTest
    public void testTypedSuggestions() {
        List<String> urlsToOpen = Arrays.asList("/chrome/test/data/android/navigate/one.html");
        TabSwitcherSearchTestUtils.openUrls(mActivityTestRule, urlsToOpen, /* incognito= */ false);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        SearchActivity searchActivity =
                TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad();

        OmniboxTestUtils omniboxTestUtils = new OmniboxTestUtils(searchActivity);
        omniboxTestUtils.requestFocus();
        omniboxTestUtils.typeText("one.html", /* execute= */ false);
        omniboxTestUtils.waitAnimationsComplete();

        ViewGroup suggestions = searchActivity.findViewById(R.id.omnibox_suggestions_dropdown);
        verifySuggestions(suggestions, urlsToOpen, null);
    }

    @Test
    @MediumTest
    public void testTypedSuggestions_Incognito() {
        List<String> urlsToOpen = Arrays.asList("/chrome/test/data/android/navigate/one.html");
        TabSwitcherSearchTestUtils.openUrls(mActivityTestRule, urlsToOpen, /* incognito= */ true);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        SearchActivity searchActivity =
                TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad();

        OmniboxTestUtils omniboxTestUtils = new OmniboxTestUtils(searchActivity);
        omniboxTestUtils.requestFocus();
        omniboxTestUtils.typeText("one.html", /* execute= */ false);
        omniboxTestUtils.waitAnimationsComplete();

        ViewGroup suggestions = searchActivity.findViewById(R.id.omnibox_suggestions_dropdown);
        verifySuggestions(suggestions, urlsToOpen, null);
    }

    private void verifySuggestions(
            ViewGroup suggestions, List<String> suggestionUrls, String header) {
        for (int i = 0; i < suggestionUrls.size(); i++) {
            String url = suggestionUrls.get(i);
            if (!url.startsWith("about:")) {
                url = URL_PREFIX + url;
            }
            // Line 2 is the URL, the titles vary.
            onView(
                            allOf(
                                    withId(R.id.line_2),
                                    withText(url),
                                    withEffectiveVisibility(Visibility.VISIBLE)))
                    .check(matches(isCompletelyDisplayed()));
        }
    }
}
