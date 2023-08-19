// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.Matchers.instanceOf;

import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.INSTANT_START_TEST_BASE_PARAMS;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.espresso.matcher.BoundedMatcher;
import androidx.test.filters.LargeTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.core.AllOf;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;
import org.chromium.ui.test.util.ViewUtils;

/**
 * Integration tests of Feed placeholder on Instant Start which requires 2-stage initialization for
 * Clank startup.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.
    Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
@EnableFeatures({ChromeFeatureList.START_SURFACE_RETURN_TIME + "<Study,",
    ChromeFeatureList.START_SURFACE_ANDROID + "<Study", ChromeFeatureList.INSTANT_START})
@Restriction({Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE,
    UiRestriction.RESTRICTION_TYPE_PHONE})
public class InstantStartFeedTest {
    // clang-format on
    private static final int ARTICLE_SECTION_HEADER_POSITION = 0;

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() {
        ReturnToChromeUtil.setSkipInitializationCheckForTesting(true);
    }

    @After
    public void tearDown() {
        if (mActivityTestRule.getActivity() != null) {
            ActivityTestUtils.clearActivityOrientation(mActivityTestRule.getActivity());
        }
    }

    @Test
    @SmallTest
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        INSTANT_START_TEST_BASE_PARAMS})
    public void testFeedPlaceholderFromColdStart() {
        // clang-format on
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        Assert.assertFalse(mActivityTestRule.getActivity().isTablet());
        Assert.assertTrue(ChromeFeatureList.sInstantStart.isEnabled());

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        // Feed placeholder should be shown from cold start with Instant Start on.
        onView(withId(org.chromium.chrome.test.R.id.placeholders_layout))
                .check(matches(isDisplayed()));
        Assert.assertFalse(LibraryLoader.getInstance().isInitialized());

        StartSurfaceTestUtils.startAndWaitNativeInitialization(mActivityTestRule);
        // Feed background should be non-transparent finally.
        ViewUtils.onViewWaiting(
                AllOf.allOf(withId(org.chromium.chrome.test.R.id.feed_stream_recycler_view),
                        matchesBackgroundAlpha(255)));

        StartSurfaceCoordinator startSurfaceCoordinator =
                StartSurfaceTestUtils.getStartSurfaceFromUIThread(cta);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(
                    startSurfaceCoordinator.getMediatorForTesting().shouldShowFeedPlaceholder());
        });
    }

    @Test
    @LargeTest
    @CommandLineFlags.Add({INSTANT_START_TEST_BASE_PARAMS})
    public void testCachedFeedVisibility() {
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        mActivityTestRule.waitForActivityNativeInitializationComplete();
        // FEED_ARTICLES_LIST_VISIBLE should equal to ARTICLES_LIST_VISIBLE.
        CriteriaHelper.pollUiThread(()
                                            -> UserPrefs.get(Profile.getLastUsedRegularProfile())
                                                       .getBoolean(Pref.ARTICLES_LIST_VISIBLE)
                        == ReturnToChromeUtil.getFeedArticlesVisibility());

        // Hide articles and verify that FEED_ARTICLES_LIST_VISIBLE and ARTICLES_LIST_VISIBLE are
        // both false.
        toggleHeader(false);
        CriteriaHelper.pollUiThread(() -> !ReturnToChromeUtil.getFeedArticlesVisibility());
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertEquals(UserPrefs.get(Profile.getLastUsedRegularProfile())
                                                       .getBoolean(Pref.ARTICLES_LIST_VISIBLE),
                                ReturnToChromeUtil.getFeedArticlesVisibility()));

        // Show articles and verify that FEED_ARTICLES_LIST_VISIBLE and ARTICLES_LIST_VISIBLE are
        // both true.
        toggleHeader(true);
        CriteriaHelper.pollUiThread(ReturnToChromeUtil::getFeedArticlesVisibility);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertEquals(UserPrefs.get(Profile.getLastUsedRegularProfile())
                                                       .getBoolean(Pref.ARTICLES_LIST_VISIBLE),
                                ReturnToChromeUtil.getFeedArticlesVisibility()));
    }

    @Test
    @SmallTest
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        INSTANT_START_TEST_BASE_PARAMS})
    public void testHideFeedPlaceholder() {
        // clang-format on
        StartSurfaceConfiguration.setFeedVisibilityForTesting(false);
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);

        // When cached Feed articles' visibility is invisible, placeholder should be invisible too.
        onView(withId(org.chromium.chrome.test.R.id.placeholders_layout)).check(doesNotExist());
    }

    @Test
    @SmallTest
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        INSTANT_START_TEST_BASE_PARAMS})
    public void testShowFeedPlaceholder() {
        // clang-format on
        StartSurfaceConfiguration.setFeedVisibilityForTesting(true);
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);

        // When cached Feed articles' visibility is visible, placeholder should be visible too.
        onView(withId(org.chromium.chrome.test.R.id.placeholders_layout))
                .check(matches(isDisplayed()));
    }

    /**
     * Toggles the header and checks whether the header has the right status.
     *
     * @param expanded Whether the header should be expanded.
     */
    private void toggleHeader(boolean expanded) {
        onView(allOf(instanceOf(RecyclerView.class),
                       withId(org.chromium.chrome.test.R.id.feed_stream_recycler_view)))
                .perform(RecyclerViewActions.scrollToPosition(ARTICLE_SECTION_HEADER_POSITION));
        onView(withId(org.chromium.chrome.test.R.id.header_menu)).perform(click());

        onView(withText(expanded ? org.chromium.chrome.test.R.string.ntp_turn_on_feed
                                 : org.chromium.chrome.test.R.string.ntp_turn_off_feed))
                .perform(click());

        onView(withText(expanded ? org.chromium.chrome.test.R.string.ntp_discover_on
                                 : org.chromium.chrome.test.R.string.ntp_discover_off))
                .check(matches(isDisplayed()));
    }

    private static Matcher<View> matchesBackgroundAlpha(final int expectedAlpha) {
        return new BoundedMatcher<View, View>(View.class) {
            String mMessage;
            int mActualAlpha;

            @Override
            protected boolean matchesSafely(View item) {
                if (item.getBackground() == null) {
                    mMessage = item.getId() + " does not have a background";
                    return false;
                }
                mActualAlpha = item.getBackground().getAlpha();
                return mActualAlpha == expectedAlpha;
            }
            @Override
            public void describeTo(final Description description) {
                if (expectedAlpha != mActualAlpha) {
                    mMessage = "Background alpha did not match: Expected " + expectedAlpha + " was "
                            + mActualAlpha;
                }
                description.appendText(mMessage);
            }
        };
    }
}
