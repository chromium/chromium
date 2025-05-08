// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

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

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.ui.test.util.DeviceRestriction;

/** Public transit tests for the Hub's history pane. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@EnableFeatures(ChromeFeatureList.BOOKMARK_PANE_ANDROID)
public class BookmarkPaneTest {
    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    private WebPageStation mStartingPage;

    @Before
    public void setUp() {
        mStartingPage = mCtaTestRule.startOnBlankPage();
    }

    @After
    public void tearDown() {
        ChromeTabbedActivity cta = mCtaTestRule.getActivity();
        runOnUiThreadBlocking(
                () -> clearBookmarks(cta.getProfileProviderSupplier().get().getOriginalProfile()));
    }

    @Test
    @MediumTest
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void testBookmarkIsDisplayed() {
        ChromeTabbedActivity cta = mCtaTestRule.getActivity();
        String urlOne =
                mCtaTestRule.getTestServer().getURL("/chrome/test/data/android/navigate/one.html");
        mStartingPage.loadWebPageProgrammatically(urlOne);
        BookmarkTestUtil.waitForBookmarkModelLoaded();
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                mCtaTestRule.getActivity(),
                R.id.bookmark_this_page_id);

        enterTabSwitcher(cta);
        enterBookmarkPane();

        onViewWaiting(withText("Mobile bookmarks")).perform(click());
        onViewWaiting(withText("One")).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void testBookmarkSearchMatch() {
        ChromeTabbedActivity cta = mCtaTestRule.getActivity();
        String urlOne =
                mCtaTestRule.getTestServer().getURL("/chrome/test/data/android/navigate/one.html");
        mStartingPage.loadWebPageProgrammatically(urlOne);
        BookmarkTestUtil.waitForBookmarkModelLoaded();
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                mCtaTestRule.getActivity(),
                R.id.bookmark_this_page_id);

        enterTabSwitcher(cta);
        enterBookmarkPane();

        // Search for "One" in the history search box.
        onView(withId(R.id.row_search_text)).perform(click());
        onView(withId(R.id.row_search_text)).perform(replaceText("One"));

        // Verify that "One" is displayed as a match.
        onViewWaiting(allOf(withText("One"), withId(R.id.title))).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void testBookmarkClickOpens() {
        ChromeTabbedActivity cta = mCtaTestRule.getActivity();
        String urlOne =
                mCtaTestRule.getTestServer().getURL("/chrome/test/data/android/navigate/one.html");
        WebPageStation pageOne = mStartingPage.loadWebPageProgrammatically(urlOne);
        BookmarkTestUtil.waitForBookmarkModelLoaded();
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                mCtaTestRule.getActivity(),
                R.id.bookmark_this_page_id);

        String urlTwo =
                mCtaTestRule.getTestServer().getURL("/chrome/test/data/android/navigate/two.html");
        pageOne.loadWebPageProgrammatically(urlTwo);

        enterTabSwitcher(cta);
        enterBookmarkPane();

        onViewWaiting(withText("Mobile bookmarks")).perform(click());
        onViewWaiting(withText("One")).perform(click());

        CriteriaHelper.pollUiThread(
                () -> urlOne.equals(cta.getTabModelSelector().getCurrentTab().getUrl().getSpec()));
    }

    private void enterBookmarkPane() {
        onView(
                        allOf(
                                isDescendantOfA(withId(R.id.pane_switcher)),
                                withContentDescription(containsString("Bookmarks"))))
                .perform(click());
    }

    private void clearBookmarks(Profile profile) {
        BookmarkModel.getForProfile(profile).removeAllUserBookmarks();
    }
}
