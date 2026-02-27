// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static android.content.res.Configuration.ORIENTATION_LANDSCAPE;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import static org.chromium.base.test.transit.ViewFinder.waitForNoView;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeNtpUrl;
import static org.chromium.ui.test.util.ViewUtils.waitForView;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.content.pm.ActivityInfo;
import android.os.Build;
import android.text.format.DateUtils;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.action.GeneralLocation;
import androidx.test.espresso.action.GeneralSwipeAction;
import androidx.test.espresso.action.Press;
import androidx.test.espresso.action.Swipe;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.feed.v2.FeedV2TestHelper;
import org.chromium.chrome.browser.feed.v2.TestFeedServer;
import org.chromium.chrome.browser.firstrun.FirstRunUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.gesturenav.GestureNavigationUtils;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.NtpSmoothTransitionDelegate;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.TilesLinearLayout;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone;
import org.chromium.chrome.browser.ui.signin.signin_promo.NtpSigninPromoDelegate;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Coordinates;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;

import java.io.IOException;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/**
 * Tests for {@link NewTabPage}. Other tests can be found in {@link
 * org.chromium.chrome.browser.ntp.NewTabPageTest}. TODO(crbug.com/40683883): Combine test suites.
 */
@DoNotBatch(reason = "Complex tests, need to start fresh")
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "disable-features=IPH_FeedHeaderMenu"
})
public class FeedV2NewTabPageTest {
    private static final int ARTICLE_SECTION_HEADER_POSITION = 1;
    private static final int SIGNIN_PROMO_POSITION = 2;
    private static final int MIN_ITEMS_AFTER_LOAD = 10;

    // Espresso ViewAction that performs a swipe from center to left across the vertical center
    // of the view. Used instead of ViewAction.swipeLeft which swipes from right edge to
    // avoid conflict with gesture navigation UI which consumes the edge swipe.
    private static final ViewAction SWIPE_LEFT =
            new GeneralSwipeAction(
                    Swipe.FAST, GeneralLocation.CENTER, GeneralLocation.CENTER_LEFT, Press.FINGER);

    private final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule
    public final SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(
                            ChromeRenderTestRule.Component.UI_BROWSER_CONTENT_SUGGESTIONS_FEED)
                    .setRevision(2)
                    .build();

    public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    // Mock sign-in environment needs to be destroyed after ChromeActivity in case there are
    // observers registered in the AccountManagerFacade mock.
    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mSigninTestRule).around(mActivityTestRule);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ExternalAuthUtils mExternalAuthUtils;

    private Tab mTab;
    private NewTabPage mNtp;
    private FakeMostVisitedSites mMostVisitedSites;
    private EmbeddedTestServer mTestServer;
    private List<SiteSuggestion> mSiteSuggestions;
    private TestFeedServer mFeedServer;

    @Before
    public void setUp() throws Exception {
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtils);
        // Pretend Google Play services are available as it is required for the sign-in
        when(mExternalAuthUtils.isGooglePlayServicesMissing(any())).thenReturn(false);
        when(mExternalAuthUtils.canUseGooglePlayServices()).thenReturn(true);

        mActivityTestRule.startOnBlankPage();

        // EULA must be accepted, and internet connectivity is required, or the Feed will not
        // attempt to load.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    NetworkChangeNotifier.forceConnectivityState(true);
                    FirstRunUtils.setEulaAccepted();
                });

        mFeedServer = new TestFeedServer();
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());

        mSiteSuggestions = NewTabPageTestUtils.createFakeSiteSuggestions(mTestServer);
        mMostVisitedSites = new FakeMostVisitedSites();
        mMostVisitedSites.setTileSuggestions(mSiteSuggestions);
        mSuggestionsDeps.getFactory().mostVisitedSites = mMostVisitedSites;
        GestureNavigationUtils.setMinRequiredPhysicalRamMbForTesting(0);
    }

    @After
    public void tearDown() {
        if (mTestServer != null) {
            mFeedServer.shutdown();
        }
    }

    private void openNewTabPage() {
        mActivityTestRule.loadUrlInNewTab(getOriginalNativeNtpUrl());
        mTab = mActivityTestRule.getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(mTab);

        Assert.assertTrue(mTab.getNativePage() instanceof NewTabPage);
        mNtp = (NewTabPage) mTab.getNativePage();

        TilesLinearLayout mvTilesLayout = mNtp.getView().findViewById(R.id.mv_tiles_layout);
        Assert.assertEquals(mSiteSuggestions.size(), mvTilesLayout.getTileCount());
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    public void testLoadFeedContent() {
        openNewTabPage();

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            FeedV2TestHelper.getFeedUserActionsHistogramValues(),
                            Matchers.hasEntry("kOpenedFeedSurface", 1));
                    Criteria.checkThat(
                            FeedV2TestHelper.getLoadStreamStatusInitialValues(),
                            Matchers.hasEntry("kLoadedFromNetwork", 1));
                });
        FeedV2TestHelper.waitForRecyclerItems(MIN_ITEMS_AFTER_LOAD, getRecyclerView());
    }

    /**
     * Test that the feed has been scrolled to target position at the moment NTP finishes fading in
     * when navigating back from webpage.
     */
    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    @CommandLineFlags.Add({
        "force-prefers-no-reduced-motion",
        // Resampling can make scroll offsets non-deterministic so turn it off.
        "disable-features=ResamplingScrollEvents",
        "hide-scrollbars"
    })
    @DisableIf.Build(
            sdk_is_greater_than = Build.VERSION_CODES.Q,
            message = "crbug.com/1276402 crbug.com/345352689")
    public void testNavigateBackToNTPWithFeeds() throws TimeoutException, InterruptedException {
        openNewTabPage();

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            FeedV2TestHelper.getFeedUserActionsHistogramValues(),
                            Matchers.hasEntry("kOpenedFeedSurface", 1));
                    Criteria.checkThat(
                            FeedV2TestHelper.getLoadStreamStatusInitialValues(),
                            Matchers.hasEntry("kLoadedFromNetwork", 1));
                });
        FeedV2TestHelper.waitForRecyclerItems(MIN_ITEMS_AFTER_LOAD, getRecyclerView());
        WebContents webContents = mActivityTestRule.getWebContents();

        onView(withId(R.id.feed_stream_recycler_view))
                .perform(RecyclerViewActions.scrollToPosition(MIN_ITEMS_AFTER_LOAD));

        mActivityTestRule.loadUrl(mTestServer.getURL("/chrome/test/data/android/blue.html"));

        WebContentsUtils.waitForCopyableViewInWebContents(webContents);

        float widthPx =
                webContents.getWidth() * Coordinates.createFor(webContents).getDeviceScaleFactor();

        // Drag far enough to cause the back gesture to invoke.
        float fromEdgeStart = 5.0f;
        float dragDistance = widthPx - 10.0f;

        // from left edge EDGE_LEFT
        float fromX = fromEdgeStart;
        float toX = fromEdgeStart + dragDistance;

        TouchCommon.performWallClockDrag(
                mActivityTestRule.getActivity(),
                fromX,
                toX,
                /* fromY= */ 400.0f,
                /* toY= */ 400.0f,
                /* duration= */ 2000,
                /* dispatchIntervalMs= */ 60,
                /* preventFling= */ true);

        CriteriaHelper.pollInstrumentationThread(() -> mTab.getNativePage() != null);
        NewTabPage ntp = (NewTabPage) mTab.getNativePage();

        CriteriaHelper.pollInstrumentationThread(
                () -> ntp.getSmoothTransitionDelegateForTesting() != null);
        CallbackHelper callbackHelper = new CallbackHelper();
        ((NtpSmoothTransitionDelegate) ntp.getSmoothTransitionDelegateForTesting())
                .getAnimatorForTesting()
                .addListener(
                        new AnimatorListenerAdapter() {
                            @Override
                            public void onAnimationEnd(Animator animation) {
                                Assert.assertEquals(
                                        "Feed has been scrolled to target position when NTP"
                                                + " finished faded out",
                                        RecyclerView.SCROLL_STATE_IDLE,
                                        getRecyclerView().getScrollState());
                                Assert.assertEquals(
                                        "Feed has been scrolled to target position when NTP"
                                                + " finished faded out",
                                        Integer.valueOf(
                                                FeedSurfaceProvider.RestoringState.RESTORED),
                                        ntp.getCoordinatorForTesting()
                                                .getRestoringStateSupplier()
                                                .get());
                                callbackHelper.notifyCalled();
                            }
                        });
        callbackHelper.waitForNext();
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    @DisabledTest(message = "https://crbug.com/1046822")
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testSignInPromo_DismissBySwipe() {
        openNewTabPage();
        boolean dismissed =
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, false);
        if (dismissed) {
            ChromeSharedPreferences.getInstance()
                    .writeBoolean(ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, false);
        }

        // Verify that sign-in promo is displayed initially.
        onView(withId(R.id.feed_stream_recycler_view))
                .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));

        // Swipe away the sign-in promo.
        onView(withId(R.id.feed_stream_recycler_view))
                .perform(
                        RecyclerViewActions.actionOnItemAtPosition(
                                SIGNIN_PROMO_POSITION, SWIPE_LEFT));

        ViewGroup view = (ViewGroup) mNtp.getCoordinatorForTesting().getRecyclerView();
        waitForNoView(withId(R.id.signin_promo_view_container));
        waitForView(view, allOf(withId(R.id.header_title), isDisplayed()));

        // Verify that sign-in promo is gone, but new tab page layout and header are displayed.
        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());
        onView(withId(R.id.header_title)).check(matches(isDisplayed()));
        onView(withId(R.id.ntp_content)).check(matches(isDisplayed()));

        // Reset state.
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, dismissed);
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testSignInPromo_AccountsNotReady() {
        try (var unused = mSigninTestRule.blockGetAccountsUpdate()) {
            openNewTabPage();
            // Check that the sign-in promo is not shown if accounts are not ready.
            onView(withId(R.id.feed_stream_recycler_view))
                    .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));
            onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());
        }
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testSignInPromo_AccountsReady() {
        openNewTabPage();
        // Check that the sign-in promo is displayed this time.
        onView(withId(R.id.feed_stream_recycler_view))
                .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testSignInPromo_NotShownAfterSignIn() {
        openNewTabPage();
        // Check that the sign-in promo is displayed.
        onView(withId(R.id.feed_stream_recycler_view))
                .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));

        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        onView(withId(R.id.feed_stream_recycler_view))
                .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));
        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testSignInPromoDisplayedWithAADCMinorAccount() {
        mSigninTestRule.addAccount(TestAccounts.AADC_MINOR_ACCOUNT);

        openNewTabPage();
        onView(withId(R.id.feed_stream_recycler_view))
                .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));

        // Check that the sign-in promo is displayed.
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    @EnableFeatures({
        "EnableSeamlessSignin"
                + ":seamless-signin-promo-type/compact"
                + "/seamless-signin-string-type/continueButton"
    })
    public void testSignInPromo_shownIfTimeElapsedSinceFirstShownIsLessThanFirstShownLimit() {
        // Show the promo for the first time.
        openNewTabPage();
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));

        // Advance time, but not beyond the first time shown limit.
        mFakeTimeTestRule.advanceMillis(
                (NtpSigninPromoDelegate.NTP_SYNC_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS - 1)
                        * DateUtils.HOUR_IN_MILLIS);

        // Open a new tab, the promo should still be shown.
        openNewTabPage();
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    @EnableFeatures({
        "EnableSeamlessSignin"
                + ":seamless-signin-promo-type/compact"
                + "/seamless-signin-string-type/continueButton"
    })
    public void
            testSignInPromo_shownIfTimeElapsedSinceFirstShownExceedsFirstShownLimitAndResetThreshold() {
        // Show the promo for the first time.
        openNewTabPage();
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));

        // Advance time beyond the the first time shown limit and the last time shown reset period.
        mFakeTimeTestRule.advanceMillis(
                (NtpSigninPromoDelegate.NTP_SYNC_PROMO_RESET_AFTER_DAYS * DateUtils.DAY_IN_MILLIS));
        // Open a new tab, the promo should be shown.
        openNewTabPage();
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction({DeviceFormFactor.PHONE})
    public void testLoadFeedContent_Landscape() throws IOException {
        ChromeTabbedActivity chromeActivity = mActivityTestRule.getActivity();
        chromeActivity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            chromeActivity.getResources().getConfiguration().orientation,
                            is(ORIENTATION_LANDSCAPE));
                });

        openNewTabPage();

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            FeedV2TestHelper.getFeedUserActionsHistogramValues(),
                            Matchers.hasEntry("kOpenedFeedSurface", 1));
                    Criteria.checkThat(
                            FeedV2TestHelper.getLoadStreamStatusInitialValues(),
                            Matchers.hasEntry("kLoadedFromNetwork", 1));
                });

        RecyclerView recyclerView = getRecyclerView();
        FeedV2TestHelper.waitForRecyclerItems(MIN_ITEMS_AFTER_LOAD, recyclerView);

        mRenderTestRule.render(recyclerView, "feedContent_landscape_with_scrollable_mvt_v5");
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    @Restriction({DeviceFormFactor.PHONE})
    @DisabledTest(message = "crbug.com/1467377")
    public void testFakeOmniboxOnNtp() throws IOException {
        openNewTabPage();

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        assertEquals(
                cta.getResources().getDimensionPixelSize(R.dimen.ntp_search_box_height),
                cta.findViewById(R.id.search_box).getLayoutParams().height);

        // Drag the Feed header title to scroll the toolbar to the top.
        int toY =
                -getFakeboxTop(mNtp)
                        + cta.getResources()
                                .getDimensionPixelSize(R.dimen.modern_toolbar_background_size);
        TestTouchUtils.dragCompleteView(
                InstrumentationRegistry.getInstrumentation(),
                cta.findViewById(R.id.header_title),
                0,
                0,
                0,
                toY,
                /* stepCount= */ 10);

        if (cta.findViewById(R.id.search_box).getAlpha() == 1) {
            ToolbarPhone toolbar = cta.findViewById(R.id.toolbar);
            // There might be a rounding issue for some devices.
            assertEquals(
                    toolbar.getLocationBarBackgroundHeightForTesting(),
                    cta.getResources().getDimension(R.dimen.ntp_search_box_height),
                    0.5);
        }
    }

    /**
     * @return The position of the top of the fakebox relative to the window.
     */
    private int getFakeboxTop(final NewTabPage ntp) {
        return ThreadUtils.runOnUiThreadBlocking(
                new Callable<>() {
                    @Override
                    public Integer call() {
                        final View fakebox = ntp.getView().findViewById(R.id.search_box);
                        int[] location = new int[2];
                        fakebox.getLocationInWindow(location);
                        return location[1];
                    }
                });
    }

    RecyclerView getRecyclerView() {
        return (RecyclerView) getRootView().findViewById(R.id.feed_stream_recycler_view);
    }

    private View getRootView() {
        return mActivityTestRule.getActivity().getWindow().getDecorView().getRootView();
    }
}
