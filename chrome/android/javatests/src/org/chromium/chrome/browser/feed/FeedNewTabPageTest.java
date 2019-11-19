// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.instanceOf;

import static org.chromium.chrome.test.util.ViewUtils.VIEW_NULL;
import static org.chromium.chrome.test.util.ViewUtils.waitForView;

import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.ViewAction;
import android.support.test.espresso.action.GeneralLocation;
import android.support.test.espresso.action.GeneralSwipeAction;
import android.support.test.espresso.action.Press;
import android.support.test.espresso.action.Swipe;
import android.support.test.espresso.contrib.RecyclerViewActions;
import android.support.test.filters.MediumTest;
import android.support.v7.widget.RecyclerView;
import android.view.ViewGroup;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ntp.cards.SignInPromo;
import org.chromium.chrome.browser.ntp.snippets.SectionHeader;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.ViewUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.components.signin.test.util.AccountManagerTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.List;

/**
 * Tests for {@link FeedNewTabPage} specifically. Other tests can be found in
 * {@link org.chromium.chrome.browser.ntp.NewTabPageTest}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@Features.EnableFeatures(ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS)
public class FeedNewTabPageTest {
    private static final int ARTICLE_SECTION_HEADER_POSITION = 1;
    private static final int SIGNIN_PROMO_POSITION = 2;

    // Espresso ViewAction that performs a swipe from center to left across the vertical center
    // of the view. Used instead of ViewAction.swipeLeft which swipes from right edge to
    // avoid conflict with gesture navigation UI which consumes the edge swipe.
    private static final ViewAction SWIPE_LEFT = new GeneralSwipeAction(
            Swipe.FAST, GeneralLocation.CENTER, GeneralLocation.CENTER_LEFT, Press.FINGER);

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    @Rule
    public AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    private Tab mTab;
    private FeedNewTabPage mNtp;
    private ViewGroup mTileGridLayout;
    private FakeMostVisitedSites mMostVisitedSites;
    private EmbeddedTestServer mTestServer;
    private List<SiteSuggestion> mSiteSuggestions;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityWithURL("about:blank");

        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mSiteSuggestions = NewTabPageTestUtils.createFakeSiteSuggestions(mTestServer);
        mMostVisitedSites = new FakeMostVisitedSites();
        mMostVisitedSites.setTileSuggestions(mSiteSuggestions);
        mSuggestionsDeps.getFactory().mostVisitedSites = mMostVisitedSites;

        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mTab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(mTab);

        Assert.assertTrue(mTab.getNativePage() instanceof FeedNewTabPage);
        mNtp = (FeedNewTabPage) mTab.getNativePage();
        mTileGridLayout = mNtp.getView().findViewById(R.id.tile_grid_layout);
        Assert.assertEquals(mSiteSuggestions.size(), mTileGridLayout.getChildCount());
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
        FeedProcessScopeFactory.setTestNetworkClient(null);
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    public void testSignInPromo() {
        SignInPromo.SigninObserver signinObserver = mNtp.getMediatorForTesting()
                                                            .getSignInPromoForTesting()
                                                            .getSigninObserverForTesting();
        RecyclerView recyclerView =
                (RecyclerView) mNtp.getCoordinatorForTesting().getStream().getView();

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
        TestThreadUtils.runOnUiThreadBlocking(
                () -> recyclerView.setDescendantFocusability(descendantFocusability));
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    public void testSignInPromo_DismissBySwipe() {
        boolean dismissed = SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.NTP_SIGNIN_PROMO_DISMISSED, false);
        if (dismissed) {
            SharedPreferencesManager.getInstance().writeBoolean(
                    ChromePreferenceKeys.NTP_SIGNIN_PROMO_DISMISSED, false);
        }

        // Verify that sign-in promo is displayed initially.
        onView(instanceOf(RecyclerView.class))
                .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));

        // Swipe away the sign-in promo.
        onView(instanceOf(RecyclerView.class))
                .perform(RecyclerViewActions.actionOnItemAtPosition(
                        SIGNIN_PROMO_POSITION, SWIPE_LEFT));

        ViewGroup view = (ViewGroup) mNtp.getCoordinatorForTesting().getStream().getView();
        waitForView(view, withId(R.id.signin_promo_view_container), VIEW_NULL);
        waitForView(view, allOf(withId(R.id.header_title), isDisplayed()));

        // Verify that sign-in promo is gone, but new tab page layout and header are displayed.
        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());
        onView(withId(R.id.header_title)).check(matches(isDisplayed()));
        onView(withId(R.id.ntp_content)).check(matches(isDisplayed()));

        // Reset state.
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.NTP_SIGNIN_PROMO_DISMISSED, dismissed);
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    @FlakyTest(message = "https://crbug.com/996716")
    @AccountManagerTestRule.BlockGetAccounts
    public void testSignInPromo_AccountsNotReady() {
        // Check that the sign-in promo is not shown if accounts are not ready.
        onView(instanceOf(RecyclerView.class))
                .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));
        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());

        // Wait for accounts cache population to finish and reload ntp.
        mAccountManagerTestRule.unblockGetAccountsAndWaitForAccountsPopulated();
        TestThreadUtils.runOnUiThreadBlocking(() -> mTab.reload());
        NewTabPageTestUtils.waitForNtpLoaded(mTab);

        // Check that the sign-in promo is displayed this time.
        onView(instanceOf(RecyclerView.class))
                .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    @DisabledTest(message = "https://crbug.com/914068")
    public void testArticleSectionHeader() throws Exception {
        final int expectedCountWhenCollapsed = 2;
        final int expectedCountWhenExpanded = 4; // 3 header views and the empty view.

        // Open a new tab.
        Tab tab1 = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL);
        FeedNewTabPage ntp1 = (FeedNewTabPage) tab1.getNativePage();
        SectionHeader firstHeader = ntp1.getMediatorForTesting().getSectionHeaderForTesting();
        RecyclerView.Adapter adapter1 =
                ((RecyclerView) ntp1.getCoordinatorForTesting().getStream().getView()).getAdapter();

        // Check header is expanded.
        Assert.assertTrue(firstHeader.isExpandable() && firstHeader.isExpanded());
        Assert.assertEquals(expectedCountWhenExpanded, adapter1.getItemCount());
        Assert.assertTrue(getPreferenceForArticleSectionHeader());

        // Toggle header on the current tab.
        toggleHeader((ViewGroup) ntp1.getCoordinatorForTesting().getStream().getView(), false);

        // Check header is collapsed.
        Assert.assertTrue(firstHeader.isExpandable() && !firstHeader.isExpanded());
        Assert.assertEquals(expectedCountWhenCollapsed, adapter1.getItemCount());
        Assert.assertFalse(getPreferenceForArticleSectionHeader());

        // Open a second new tab.
        Tab tab2 = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL);
        FeedNewTabPage ntp2 = (FeedNewTabPage) tab2.getNativePage();
        SectionHeader secondHeader = ntp2.getMediatorForTesting().getSectionHeaderForTesting();
        RecyclerView.Adapter adapter2 =
                ((RecyclerView) ntp2.getCoordinatorForTesting().getStream().getView()).getAdapter();

        // Check header on the second tab is collapsed.
        Assert.assertTrue(secondHeader.isExpandable() && !secondHeader.isExpanded());
        Assert.assertEquals(expectedCountWhenCollapsed, adapter2.getItemCount());
        Assert.assertFalse(getPreferenceForArticleSectionHeader());

        // Toggle header on the second tab.
        toggleHeader((ViewGroup) ntp2.getCoordinatorForTesting().getStream().getView(), true);

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
    public void testFeedDisabledByPolicy() throws Exception {
        final boolean pref = TestThreadUtils.runOnUiThreadBlocking(
                () -> PrefServiceBridge.getInstance().getBoolean(
                        Pref.NTP_ARTICLES_SECTION_ENABLED));

        // Policy is disabled. Verify the NTP root view contains only the Stream view as child.
        ViewGroup rootView = (ViewGroup) mNtp.getView();
        ViewUtils.waitForStableView(rootView);
        Assert.assertNotNull(mNtp.getCoordinatorForTesting().getStream());
        Assert.assertNull(mNtp.getCoordinatorForTesting().getScrollViewForPolicy());
        Assert.assertEquals(1, rootView.getChildCount());
        Assert.assertEquals(
                mNtp.getCoordinatorForTesting().getStream().getView(), rootView.getChildAt(0));

        // Simulate that policy is enabled. Verify the NTP root view contains only the view for
        // policy as child.
        TestThreadUtils.runOnUiThreadBlocking(() -> PrefServiceBridge.getInstance().setBoolean(
                Pref.NTP_ARTICLES_SECTION_ENABLED, false));
        Assert.assertNotNull(mNtp.getCoordinatorForTesting().getScrollViewForPolicy());
        Assert.assertNull(mNtp.getCoordinatorForTesting().getStream());
        Assert.assertEquals(1, rootView.getChildCount());
        Assert.assertEquals(
                mNtp.getCoordinatorForTesting().getScrollViewForPolicy(), rootView.getChildAt(0));

        // Open a new tab while policy is still enabled.
        Tab tab2 = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL);
        FeedNewTabPage ntp2 = (FeedNewTabPage) tab2.getNativePage();
        ViewGroup rootView2 = (ViewGroup) ntp2.getView();

        // Verify that NTP root view contains only the view for policy as child.
        Assert.assertNotNull(ntp2.getCoordinatorForTesting().getScrollViewForPolicy());
        Assert.assertNull(ntp2.getCoordinatorForTesting().getStream());
        Assert.assertEquals(1, rootView2.getChildCount());
        Assert.assertEquals(
                ntp2.getCoordinatorForTesting().getScrollViewForPolicy(), rootView2.getChildAt(0));

        // Simulate that policy is disabled. Verify the NTP root view is the view for policy. We
        // don't re-enable the Feed until the next restart.
        TestThreadUtils.runOnUiThreadBlocking(() -> PrefServiceBridge.getInstance().setBoolean(
                Pref.NTP_ARTICLES_SECTION_ENABLED, true));
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
        TestThreadUtils.runOnUiThreadBlocking(() -> PrefServiceBridge.getInstance().setBoolean(
                Pref.NTP_ARTICLES_SECTION_ENABLED, pref));
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
                        withText(expanded ? R.string.hide : R.string.show)));
    }

    private boolean getPreferenceForArticleSectionHeader() throws Exception {
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> PrefServiceBridge.getInstance().getBoolean(Pref.NTP_ARTICLES_LIST_VISIBLE));
    }
}
