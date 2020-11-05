// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v1;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isRoot;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;

import static org.chromium.chrome.test.util.ViewUtils.VIEW_NULL;
import static org.chromium.chrome.test.util.ViewUtils.waitForView;

import android.support.test.InstrumentationRegistry;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.action.GeneralLocation;
import androidx.test.espresso.action.GeneralSwipeAction;
import androidx.test.espresso.action.Press;
import androidx.test.espresso.action.Swipe;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.cards.SignInPromo;
import org.chromium.chrome.browser.ntp.snippets.SectionHeader;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.ViewUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.Collections;
import java.util.List;

/**
 * Tests for {@link NewTabPage}. Other tests can be found in
 * {@link org.chromium.chrome.browser.ntp.NewTabPageTest}.
 * TODO(https://crbug.com/1069183): Combine test suites.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "disable-features=IPH_FeedHeaderMenu"})
@Features.EnableFeatures(ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS)
@Features.DisableFeatures({ChromeFeatureList.REPORT_FEED_USER_ACTIONS,
        ChromeFeatureList.QUERY_TILES, ChromeFeatureList.ENHANCED_PROTECTION_PROMO_CARD})
public class FeedNewTabPageTest {
    private static final int ARTICLE_SECTION_HEADER_POSITION = 1;
    private static final int SIGNIN_PROMO_POSITION = 2;

    // Espresso ViewAction that performs a swipe from center to left across the vertical center
    // of the view. Used instead of ViewAction.swipeLeft which swipes from right edge to
    // avoid conflict with gesture navigation UI which consumes the edge swipe.
    private static final ViewAction SWIPE_LEFT = new GeneralSwipeAction(
            Swipe.FAST, GeneralLocation.CENTER, GeneralLocation.CENTER_LEFT, Press.FINGER);

    private boolean mIsCachePopulatedInAccountManagerFacade = true;

    @Rule
    public final SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    private final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(new FakeAccountManagerFacade(null) {
                @Override
                public boolean isCachePopulated() {
                    // Attention. When isCachePopulated() returns false,
                    // runAfterCacheIsPopulated(...) shouldn't run. If this becomes a problem,
                    // we can override runAfterCacheIsPopulated(...) as well.
                    return mIsCachePopulatedInAccountManagerFacade;
                }
            });

    // Mock sign-in environment needs to be destroyed after ChromeActivity in case there are
    // observers registered in the AccountManagerFacade mock.
    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mAccountManagerTestRule).around(mActivityTestRule);

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
    private ViewGroup mTileGridLayout;
    private FakeMostVisitedSites mMostVisitedSites;
    private EmbeddedTestServer mTestServer;
    private List<SiteSuggestion> mSiteSuggestions;
    private boolean mDisableSigninPromoCard;

    @ParameterAnnotations.UseMethodParameterBefore(SigninPromoParams.class)
    public void disableSigninPromoCard(boolean disableSigninPromoCard) {
        mDisableSigninPromoCard = disableSigninPromoCard;
    }

    @Before
    public void setUp() {
        SignInPromo.setDisablePromoForTests(mDisableSigninPromoCard);
        mActivityTestRule.startMainActivityWithURL("about:blank");

        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mSiteSuggestions = NewTabPageTestUtils.createFakeSiteSuggestions(mTestServer);
        mMostVisitedSites = new FakeMostVisitedSites();
        mMostVisitedSites.setTileSuggestions(mSiteSuggestions);
        mSuggestionsDeps.getFactory().mostVisitedSites = mMostVisitedSites;
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
        FeedProcessScopeFactory.setTestNetworkClient(null);
    }

    private void openNewTabPage() {
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mTab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(mTab);

        Assert.assertTrue(mTab.getNativePage() instanceof NewTabPage);
        mNtp = (NewTabPage) mTab.getNativePage();
        mTileGridLayout = mNtp.getView().findViewById(R.id.tile_grid_layout);
        Assert.assertEquals(mSiteSuggestions.size(), mTileGridLayout.getChildCount());
    }

    private void waitForPopup(Matcher<View> matcher) {
        View mainDecorView = mActivityTestRule.getActivity().getWindow().getDecorView();
        onView(isRoot())
                .inRoot(withDecorView(not(is(mainDecorView))))
                .check(waitForView(matcher, ViewUtils.VIEW_VISIBLE));
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    @Features.DisableFeatures(ChromeFeatureList.INTEREST_FEED_V2)
    @DisabledTest(message = "Flaky -- crbug.com/1136923")
    public void testSignInPromo() {
        openNewTabPage();
        SignInPromo.SigninObserver signinObserver = mNtp.getCoordinatorForTesting()
                                                            .getMediatorForTesting()
                                                            .getSignInPromoForTesting()
                                                            .getSigninObserverForTesting();
        RecyclerView recyclerView =
                (RecyclerView) mNtp.getCoordinatorForTesting().getStreamForTesting().getView();

        // Prioritize RecyclerView's focusability so that the sign-in promo button and the action
        // button don't get focused initially to avoid flakiness.
        int descendantFocusability = recyclerView.getDescendantFocusability();
        TestThreadUtils.runOnUiThreadBlocking((() -> {
            recyclerView.setDescendantFocusability(ViewGroup.FOCUS_BEFORE_DESCENDANTS);
            recyclerView.requestFocus();
        }));

        // Simulate sign in, scroll to the position where sign-in promo could be placed, and verify
        // that sign-in promo is not shown.
        TestThreadUtils.runOnUiThreadBlocking(signinObserver::onSignedIn);
        RecyclerViewTestUtils.waitForStableRecyclerView(recyclerView);
        onView(instanceOf(RecyclerView.class))
                .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));
        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());

        // Simulate sign out, scroll to the position where sign-in promo could be placed, and verify
        // that sign-in promo is shown.
        TestThreadUtils.runOnUiThreadBlocking(signinObserver::onSignedOut);
        RecyclerViewTestUtils.waitForStableRecyclerView(recyclerView);
        onView(instanceOf(RecyclerView.class))
                .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));

        // Hide articles and verify that the sign-in promo is not shown.
        toggleHeader(recyclerView, false);
        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());

        // Show articles and verify that the sign-in promo is shown.
        toggleHeader(recyclerView, true);
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));

        // Reset states.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mNtp.getCoordinatorForTesting().getMediatorForTesting().destroyForTesting();
            recyclerView.setDescendantFocusability(descendantFocusability);
        });
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    @DisabledTest(message = "https://crbug.com/1046822")
    public void testSignInPromo_DismissBySwipe() {
        openNewTabPage();
        boolean dismissed = SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, false);
        if (dismissed) {
            SharedPreferencesManager.getInstance().writeBoolean(
                    ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, false);
        }

        // Verify that sign-in promo is displayed initially.
        onView(instanceOf(RecyclerView.class))
                .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));

        // Swipe away the sign-in promo.
        onView(instanceOf(RecyclerView.class))
                .perform(RecyclerViewActions.actionOnItemAtPosition(
                        SIGNIN_PROMO_POSITION, SWIPE_LEFT));

        ViewGroup view =
                (ViewGroup) mNtp.getCoordinatorForTesting().getStreamForTesting().getView();
        waitForView(view, withId(R.id.signin_promo_view_container), VIEW_NULL);
        waitForView(view, allOf(withId(R.id.header_title), isDisplayed()));

        // Verify that sign-in promo is gone, but new tab page layout and header are displayed.
        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());
        onView(withId(R.id.header_title)).check(matches(isDisplayed()));
        onView(withId(R.id.ntp_content)).check(matches(isDisplayed()));

        // Reset state.
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, dismissed);
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    public void testSignInPromo_AccountsNotReady() {
        mIsCachePopulatedInAccountManagerFacade = false;
        openNewTabPage();
        // Check that the sign-in promo is not shown if accounts are not ready.
        onView(instanceOf(RecyclerView.class))
                .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));
        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());

        mIsCachePopulatedInAccountManagerFacade = true;
        TestThreadUtils.runOnUiThreadBlocking(mTab::reload);
        ChromeTabUtils.waitForTabPageLoaded(mTab, ChromeTabUtils.getUrlStringOnUiThread(mTab));

        // Check that the sign-in promo is displayed this time.
        onView(instanceOf(RecyclerView.class))
                .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.REPORT_FEED_USER_ACTIONS)
    @Features.DisableFeatures(ChromeFeatureList.INTEREST_FEED_V2)
    @Feature({"NewTabPage", "FeedNewTabPage"})
    @ParameterAnnotations.UseMethodParameter(SigninPromoParams.class)
    public void testArticleSectionHeaderWithMenu(boolean disableSigninPromoCard) throws Exception {
        openNewTabPage();
        // Scroll to the article section header in case it is not visible.
        onView(instanceOf(RecyclerView.class))
                .perform(RecyclerViewActions.scrollToPosition(ARTICLE_SECTION_HEADER_POSITION));
        waitForView((ViewGroup) mNtp.getView(), allOf(withId(R.id.header_title), isDisplayed()));

        View sectionHeaderView = mNtp.getCoordinatorForTesting().getSectionHeaderViewForTesting();
        TextView headerStatusView = sectionHeaderView.findViewById(R.id.header_title);

        // Assert that the feed is expanded and that the header title text is correct.
        Assert.assertTrue(mNtp.getCoordinatorForTesting()
                                  .getMediatorForTesting()
                                  .getSectionHeaderForTesting()
                                  .isExpanded());
        Assert.assertEquals(sectionHeaderView.getContext().getString(R.string.ntp_discover_on),
                headerStatusView.getText());

        // Toggle header on the current tab.
        onView(withId(R.id.header_menu)).perform(click());
        View mainDecorView = mActivityTestRule.getActivity().getWindow().getDecorView();
        waitForPopup(withText(R.string.ntp_turn_off_feed));
        onView(withText(R.string.ntp_turn_off_feed))
                .inRoot(withDecorView(not(is(mainDecorView))))
                .perform(click());

        // Assert that the feed is collapsed and that the header title text is correct.
        Assert.assertFalse(mNtp.getCoordinatorForTesting()
                                   .getMediatorForTesting()
                                   .getSectionHeaderForTesting()
                                   .isExpanded());
        Assert.assertEquals(sectionHeaderView.getContext().getString(R.string.ntp_discover_off),
                headerStatusView.getText());
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    @DisabledTest(message = "https://crbug.com/914068")
    public void testArticleSectionHeader() throws Exception {
        openNewTabPage();
        final int expectedCountWhenCollapsed = 2;
        final int expectedCountWhenExpanded = 4; // 3 header views and the empty view.

        // Open a new tab.
        Tab tab1 = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL);
        NewTabPage ntp1 = (NewTabPage) tab1.getNativePage();
        SectionHeader firstHeader = ntp1.getCoordinatorForTesting()
                                            .getMediatorForTesting()
                                            .getSectionHeaderForTesting();
        RecyclerView.Adapter adapter1 =
                ((RecyclerView) ntp1.getCoordinatorForTesting().getStreamForTesting().getView())
                        .getAdapter();

        // Check header is expanded.
        Assert.assertTrue(firstHeader.isExpandable() && firstHeader.isExpanded());
        Assert.assertEquals(expectedCountWhenExpanded, adapter1.getItemCount());
        Assert.assertTrue(getPreferenceForArticleSectionHeader());

        // Toggle header on the current tab.
        toggleHeader(
                (ViewGroup) ntp1.getCoordinatorForTesting().getStreamForTesting().getView(), false);

        // Check header is collapsed.
        Assert.assertTrue(firstHeader.isExpandable() && !firstHeader.isExpanded());
        Assert.assertEquals(expectedCountWhenCollapsed, adapter1.getItemCount());
        Assert.assertFalse(getPreferenceForArticleSectionHeader());

        // Open a second new tab.
        Tab tab2 = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL);
        NewTabPage ntp2 = (NewTabPage) tab2.getNativePage();
        SectionHeader secondHeader = ntp2.getCoordinatorForTesting()
                                             .getMediatorForTesting()
                                             .getSectionHeaderForTesting();
        RecyclerView.Adapter adapter2 =
                ((RecyclerView) ntp2.getCoordinatorForTesting().getStreamForTesting().getView())
                        .getAdapter();

        // Check header on the second tab is collapsed.
        Assert.assertTrue(secondHeader.isExpandable() && !secondHeader.isExpanded());
        Assert.assertEquals(expectedCountWhenCollapsed, adapter2.getItemCount());
        Assert.assertFalse(getPreferenceForArticleSectionHeader());

        // Toggle header on the second tab.
        toggleHeader(
                (ViewGroup) ntp2.getCoordinatorForTesting().getStreamForTesting().getView(), true);

        // Check header on the second tab is expanded.
        Assert.assertTrue(secondHeader.isExpandable() && secondHeader.isExpanded());
        Assert.assertEquals(expectedCountWhenExpanded, adapter2.getItemCount());
        Assert.assertTrue(getPreferenceForArticleSectionHeader());

        // Go back to the first tab and wait for a stable recycler view.
        ChromeTabUtils.switchTabInCurrentTabModel(mActivityTestRule.getActivity(), tab1.getId());

        // Check header on the first tab is expanded.
        Assert.assertTrue(firstHeader.isExpandable() && firstHeader.isExpanded());
        Assert.assertEquals(expectedCountWhenExpanded, adapter1.getItemCount());
        Assert.assertTrue(getPreferenceForArticleSectionHeader());
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    @DisabledTest(message = "crbug.com/1064388")
    public void testFeedDisabledByPolicy() throws Exception {
        openNewTabPage();
        final boolean pref = TestThreadUtils.runOnUiThreadBlocking(
                () -> getPrefService().getBoolean(Pref.ENABLE_SNIPPETS));

        // Policy is disabled. Verify the NTP root view contains only the Stream view as child.
        ViewGroup rootView = (ViewGroup) mNtp.getView();
        ViewUtils.waitForStableView(rootView);
        Assert.assertNotNull(mNtp.getCoordinatorForTesting().getStreamForTesting());
        Assert.assertNull(mNtp.getCoordinatorForTesting().getScrollViewForPolicy());
        Assert.assertEquals(1, rootView.getChildCount());
        Assert.assertEquals(mNtp.getCoordinatorForTesting().getStreamForTesting().getView(),
                rootView.getChildAt(0));

        // Simulate that policy is enabled. Verify the NTP root view contains only the view for
        // policy as child.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> getPrefService().setBoolean(Pref.ENABLE_SNIPPETS, false));
        Assert.assertNotNull(mNtp.getCoordinatorForTesting().getScrollViewForPolicy());
        Assert.assertNull(mNtp.getCoordinatorForTesting().getStreamForTesting());
        Assert.assertEquals(1, rootView.getChildCount());
        Assert.assertEquals(
                mNtp.getCoordinatorForTesting().getScrollViewForPolicy(), rootView.getChildAt(0));

        // Open a new tab while policy is still enabled.
        Tab tab2 = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL);
        NewTabPage ntp2 = (NewTabPage) tab2.getNativePage();
        ViewGroup rootView2 = (ViewGroup) ntp2.getView();

        // Verify that NTP root view contains only the view for policy as child.
        Assert.assertNotNull(ntp2.getCoordinatorForTesting().getScrollViewForPolicy());
        Assert.assertNull(ntp2.getCoordinatorForTesting().getStream());
        Assert.assertEquals(1, rootView2.getChildCount());
        Assert.assertEquals(
                ntp2.getCoordinatorForTesting().getScrollViewForPolicy(), rootView2.getChildAt(0));

        // Simulate that policy is disabled. Verify the NTP root view is the view for policy. We
        // don't re-enable the Feed until the next restart.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> getPrefService().setBoolean(Pref.ENABLE_SNIPPETS, true));
        Assert.assertNotNull(ntp2.getCoordinatorForTesting().getScrollViewForPolicy());
        Assert.assertNull(ntp2.getCoordinatorForTesting().getStream());
        Assert.assertEquals(1, rootView2.getChildCount());
        Assert.assertEquals(
                ntp2.getCoordinatorForTesting().getScrollViewForPolicy(), rootView2.getChildAt(0));

        // Switch to the old tab. Verify the NTP root view is the view for policy.
        ChromeTabUtils.switchTabInCurrentTabModel(mActivityTestRule.getActivity(), mTab.getId());
        Assert.assertNotNull(mNtp.getCoordinatorForTesting().getScrollViewForPolicy());
        Assert.assertNull(mNtp.getCoordinatorForTesting().getStream());
        Assert.assertEquals(1, rootView.getChildCount());
        Assert.assertEquals(
                mNtp.getCoordinatorForTesting().getScrollViewForPolicy(), rootView.getChildAt(0));

        // Reset state.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> getPrefService().setBoolean(Pref.ENABLE_SNIPPETS, pref));
    }

    /**
     * Toggles the header and checks whether the header has the right status.
     * @param rootView The {@link ViewGroup} that contains the header view.
     * @param expanded Whether the header should be expanded.
     */
    private void toggleHeader(ViewGroup rootView, boolean expanded) {
        onView(instanceOf(RecyclerView.class))
                .perform(RecyclerViewActions.scrollToPosition(ARTICLE_SECTION_HEADER_POSITION),
                        RecyclerViewActions.actionOnItemAtPosition(
                                ARTICLE_SECTION_HEADER_POSITION, click()));
        waitForView(rootView,
                allOf(withId(R.id.header_status),
                        withText(expanded ? R.string.hide_content : R.string.show_content)));
    }

    private boolean getPreferenceForArticleSectionHeader() throws Exception {
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> getPrefService().getBoolean(Pref.ARTICLES_LIST_VISIBLE));
    }

    private PrefService getPrefService() {
        return UserPrefs.get(Profile.getLastUsedRegularProfile());
    }
}
