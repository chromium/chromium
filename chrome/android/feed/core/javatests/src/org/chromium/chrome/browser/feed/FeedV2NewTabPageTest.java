// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static android.content.res.Configuration.ORIENTATION_LANDSCAPE;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.ViewUtils.VIEW_NULL;
import static org.chromium.ui.test.util.ViewUtils.waitForView;

import android.content.pm.ActivityInfo;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.action.GeneralLocation;
import androidx.test.espresso.action.GeneralSwipeAction;
import androidx.test.espresso.action.Press;
import androidx.test.espresso.action.Swipe;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.espresso.matcher.ViewMatchers.Visibility;
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

import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.feed.sections.SectionHeaderListProperties;
import org.chromium.chrome.browser.feed.v2.FeedV2TestHelper;
import org.chromium.chrome.browser.feed.v2.TestFeedServer;
import org.chromium.chrome.browser.firstrun.FirstRunUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.cards.SignInPromo;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;

import java.io.IOException;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.Callable;

/**
 * Tests for {@link NewTabPage}. Other tests can be found in {@link
 * org.chromium.chrome.browser.ntp.NewTabPageTest}. TODO(crbug.com/40683883): Combine test suites.
 */
@DoNotBatch(reason = "Complex tests, need to start fresh")
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "disable-features=IPH_FeedHeaderMenu"
})
@Features.EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
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

    private boolean mIsCachePopulatedInAccountManagerFacade = true;

    private final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private final FakeAccountManagerFacade mFakeAccountManagerFacade =
            new FakeAccountManagerFacade() {
                @Override
                public Promise<List<CoreAccountInfo>> getCoreAccountInfos() {
                    // Attention. When cache is not populated, the Promise shouldn't be fulfilled.
                    if (mIsCachePopulatedInAccountManagerFacade) {
                        return super.getCoreAccountInfos();
                    }
                    return new Promise<>();
                }
            };

    @Rule
    public final SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(
                            ChromeRenderTestRule.Component.UI_BROWSER_CONTENT_SUGGESTIONS_FEED)
                    .build();

    public final SigninTestRule mSigninTestRule = new SigninTestRule(mFakeAccountManagerFacade);

    // Mock sign-in environment needs to be destroyed after ChromeActivity in case there are
    // observers registered in the AccountManagerFacade mock.
    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mSigninTestRule).around(mActivityTestRule);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ExternalAuthUtils mExternalAuthUtils;

    /** Parameter provider for enabling/disabling the signin promo card. */
    public static class SigninPromoParams implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            return Collections.singletonList(
                    new ParameterSet().value(true).name("DisableSigninPromo"));
        }
    }

    private Tab mTab;
    private NewTabPage mNtp;
    private FakeMostVisitedSites mMostVisitedSites;
    private EmbeddedTestServer mTestServer;
    private List<SiteSuggestion> mSiteSuggestions;
    private boolean mDisableSigninPromoCard;
    private TestFeedServer mFeedServer;

    @ParameterAnnotations.UseMethodParameterBefore(SigninPromoParams.class)
    public void disableSigninPromoCard(boolean disableSigninPromoCard) {
        mDisableSigninPromoCard = disableSigninPromoCard;
    }

    @Before
    public void setUp() throws Exception {
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtils);
        // Pretend Google Play services are available as it is required for the sign-in
        when(mExternalAuthUtils.isGooglePlayServicesMissing(any())).thenReturn(false);
        when(mExternalAuthUtils.canUseGooglePlayServices()).thenReturn(true);

        SignInPromo.setDisablePromoForTesting(mDisableSigninPromoCard);

        mActivityTestRule.startMainActivityWithURL("about:blank");

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
    }

    @After
    public void tearDown() {
        if (mTestServer != null) {
            mFeedServer.shutdown();
        }
    }

    private void openNewTabPage() {
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mTab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(mTab);

        Assert.assertTrue(mTab.getNativePage() instanceof NewTabPage);
        mNtp = (NewTabPage) mTab.getNativePage();

        ViewGroup mvTilesLayout = mNtp.getView().findViewById(R.id.mv_tiles_layout);
        Assert.assertEquals(mSiteSuggestions.size(), mvTilesLayout.getChildCount());
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

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    @DisabledTest(message = "https://crbug.com/1046822")
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
        waitForView(view, withId(R.id.signin_promo_view_container), VIEW_NULL);
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
    public void testSignInPromo_AccountsNotReady() {
        mIsCachePopulatedInAccountManagerFacade = false;
        openNewTabPage();
        // Check that the sign-in promo is not shown if accounts are not ready.
        onView(withId(R.id.feed_stream_recycler_view))
                .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));
        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    public void testSignInPromo_AccountsReady() {
        mIsCachePopulatedInAccountManagerFacade = true;
        openNewTabPage();
        // Check that the sign-in promo is displayed this time.
        onView(withId(R.id.feed_stream_recycler_view))
                .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    public void testSignInPromo_NotShownAfterSignIn() {
        mIsCachePopulatedInAccountManagerFacade = true;
        openNewTabPage();
        // Check that the sign-in promo is displayed.
        onView(withId(R.id.feed_stream_recycler_view))
                .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));

        mSigninTestRule.addAccountThenSignin(AccountManagerTestRule.TEST_ACCOUNT_1);

        onView(withId(R.id.feed_stream_recycler_view))
                .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));
        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    public void testSignInPromoWhenDefaultAccountCannotShowHistorySyncWithoutMinorRestrictions() {
        mSigninTestRule.addAccount(AccountManagerTestRule.AADC_MINOR_ACCOUNT);
        mIsCachePopulatedInAccountManagerFacade = true;

        openNewTabPage();
        onView(withId(R.id.feed_stream_recycler_view))
                .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));

        // Check that the sign-in promo is displayed.
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    @ParameterAnnotations.UseMethodParameter(SigninPromoParams.class)
    public void testArticleSectionHeaderWithMenu(boolean disableSigninPromoCard) throws Exception {
        openNewTabPage();
        // Scroll to the article section header in case it is not visible.
        onView(withId(R.id.feed_stream_recycler_view))
                .perform(RecyclerViewActions.scrollToPosition(ARTICLE_SECTION_HEADER_POSITION));
        waitForView((ViewGroup) mNtp.getView(), allOf(withId(R.id.header_title), isDisplayed()));

        View sectionHeaderView = mNtp.getCoordinatorForTesting().getSectionHeaderViewForTesting();
        TextView headerStatusView = sectionHeaderView.findViewById(R.id.header_title);

        // Assert that the feed is expanded and that the header title text is correct.
        Assert.assertTrue(
                mNtp.getCoordinatorForTesting()
                        .getSectionHeaderModelForTest()
                        .get(SectionHeaderListProperties.IS_SECTION_ENABLED_KEY));
        Assert.assertEquals(
                sectionHeaderView.getContext().getString(R.string.ntp_discover_on),
                headerStatusView.getText());

        // Toggle header on the current tab.
        toggleHeader(false);

        // Assert that the feed is collapsed and that the header title text is correct.
        Assert.assertFalse(
                mNtp.getCoordinatorForTesting()
                        .getSectionHeaderModelForTest()
                        .get(SectionHeaderListProperties.IS_SECTION_ENABLED_KEY));
        Assert.assertEquals(
                sectionHeaderView.getContext().getString(R.string.ntp_discover_off),
                headerStatusView.getText());
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction({DeviceFormFactor.PHONE})
    @DisableFeatures({ChromeFeatureList.LOGO_POLISH})
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

        mRenderTestRule.render(recyclerView, "feedContent_landscape_with_scrollable_mvt_v2");
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
                cta.getResources()
                        .getDimensionPixelSize(org.chromium.chrome.R.dimen.ntp_search_box_height),
                cta.findViewById(org.chromium.chrome.R.id.search_box).getLayoutParams().height);

        // Drag the Feed header title to scroll the toolbar to the top.
        int toY =
                -getFakeboxTop(mNtp)
                        + cta.getResources()
                                .getDimensionPixelSize(
                                        org.chromium.chrome.R.dimen.modern_toolbar_background_size);
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
                    cta.getResources()
                            .getDimension(org.chromium.chrome.R.dimen.ntp_search_box_height),
                    0.5);
        }
    }

    /**
     * @return The position of the top of the fakebox relative to the window.
     */
    private int getFakeboxTop(final NewTabPage ntp) {
        return ThreadUtils.runOnUiThreadBlocking(
                new Callable<Integer>() {
                    @Override
                    public Integer call() {
                        final View fakebox = ntp.getView().findViewById(R.id.search_box);
                        int[] location = new int[2];
                        fakebox.getLocationInWindow(location);
                        return location[1];
                    }
                });
    }

    /**
     * Toggles the header and checks whether the header has the right status.
     * @param expanded Whether the header should be expanded.
     */
    private void toggleHeader(boolean expanded) {
        onView(allOf(instanceOf(RecyclerView.class), withId(R.id.feed_stream_recycler_view)))
                .perform(RecyclerViewActions.scrollToPosition(ARTICLE_SECTION_HEADER_POSITION));
        onView(withId(R.id.header_menu)).perform(click());

        onView(withText(expanded ? R.string.ntp_turn_on_feed : R.string.ntp_turn_off_feed))
                .perform(click());

        // There must be one and only one view with "Discover on/off" text being displayed.
        onView(
                        allOf(
                                withText(
                                        expanded
                                                ? R.string.ntp_discover_on
                                                : R.string.ntp_discover_off),
                                withEffectiveVisibility(Visibility.VISIBLE)))
                .check(matches(isDisplayed()));
    }

    RecyclerView getRecyclerView() {
        return (RecyclerView) getRootView().findViewById(R.id.feed_stream_recycler_view);
    }

    private View getRootView() {
        return mActivityTestRule.getActivity().getWindow().getDecorView().getRootView();
    }
}
