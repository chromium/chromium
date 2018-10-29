// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.action.ViewActions.swipeLeft;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.instanceOf;

import static org.chromium.chrome.test.util.ViewUtils.VIEW_NULL;
import static org.chromium.chrome.test.util.ViewUtils.waitForView;

import android.support.test.InstrumentationRegistry;
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

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.ntp.cards.SignInPromo;
import org.chromium.chrome.browser.ntp.snippets.SectionHeader;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.ViewUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;
import org.chromium.chrome.test.util.browser.suggestions.FakeMostVisitedSites;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
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
    private static final int SIGNIN_PROMO_POSITION = 1;
    private static final int ARTICLE_SECTION_HEADER_POSITION = 2;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    private Tab mTab;
    private FeedNewTabPage mNtp;
    private ViewGroup mTileGridLayout;
    private FakeMostVisitedSites mMostVisitedSites;
    private EmbeddedTestServer mTestServer;
    private List<SiteSuggestion> mSiteSuggestions;

    private static final String TEST_FEED =
            UrlUtils.getIsolatedTestFilePath("/chrome/test/data/android/feed/feed_large.gcl.bin");

    @Before
    public void setUp() throws Exception {
        TestNetworkClient client = new TestNetworkClient();
        client.setNetworkResponseFile(TEST_FEED);
        FeedProcessScopeFactory.setTestNetworkClient(client);

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
    public void testSignInPromo() throws Exception {
        SignInPromo.SigninObserver signinObserver = mNtp.getMediatorForTesting()
                                                            .getSignInPromoForTesting()
                                                            .getSigninObserverForTesting();
        RecyclerView recyclerView = (RecyclerView) mNtp.getStream().getView();

        // Simulate sign in, scroll to the position where sign-in promo could be placed, and verify
        // that sign-in promo is not shown.
        ThreadUtils.runOnUiThreadBlocking(signinObserver::onSignedIn);
        RecyclerViewTestUtils.waitForStableRecyclerView(recyclerView);
        onView(instanceOf(RecyclerView.class))
                .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));
        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());

        // Simulate sign out, scroll to the position where sign-in promo could be placed, and verify
        // that sign-in promo is shown.
        ThreadUtils.runOnUiThreadBlocking(signinObserver::onSignedOut);
        RecyclerViewTestUtils.waitForStableRecyclerView(recyclerView);
        onView(instanceOf(RecyclerView.class))
                .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));

        // Scroll to the article section header in case it is not visible.
        onView(instanceOf(RecyclerView.class))
                .perform(RecyclerViewActions.scrollToPosition(ARTICLE_SECTION_HEADER_POSITION));

        // Hide articles and verify that the sign-in promo is not shown.
        onView(withId(R.id.header_title)).perform(click());
        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());

        // Show articles and verify that the sign-in promo is shown.
        onView(withId(R.id.header_title)).perform(click());
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    public void testSignInPromo_DismissBySwipe() {
        boolean dismissed = ChromePreferenceManager.getInstance().readBoolean(
                ChromePreferenceManager.NTP_SIGNIN_PROMO_DISMISSED, false);
        if (dismissed) {
            ChromePreferenceManager.getInstance().writeBoolean(
                    ChromePreferenceManager.NTP_SIGNIN_PROMO_DISMISSED, false);
        }

        // Verify that sign-in promo is displayed initially.
        onView(instanceOf(RecyclerView.class))
                .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));

        // Swipe away the sign-in promo.
        onView(instanceOf(RecyclerView.class))
                .perform(RecyclerViewActions.actionOnItemAtPosition(
                        SIGNIN_PROMO_POSITION, swipeLeft()));

        ViewGroup view = (ViewGroup) mNtp.getStream().getView();
        waitForView(view, withId(R.id.signin_promo_view_container), VIEW_NULL);
        waitForView(view, allOf(withId(R.id.header_title), isDisplayed()));

        // Verify that sign-in promo is gone, but new tab page layout and header are displayed.
        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());
        onView(withId(R.id.header_title)).check(matches(isDisplayed()));
        onView(withId(R.id.ntp_content)).check(matches(isDisplayed()));

        // Reset state.
        ChromePreferenceManager.getInstance().writeBoolean(
                ChromePreferenceManager.NTP_SIGNIN_PROMO_DISMISSED, dismissed);
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    public void testArticleSectionHeader() throws Exception {
        // Disable the sign-in promo so the header is visible above the fold.
        SignInPromo.setDisablePromoForTests(true);
        final int expectedHeaderViewsCount = 2;

        // Open a new tab.
        Tab tab1 = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL);
        FeedNewTabPage ntp1 = (FeedNewTabPage) tab1.getNativePage();
        SectionHeader firstHeader = ntp1.getMediatorForTesting().getSectionHeaderForTesting();
        RecyclerView.Adapter adapter1 = ((RecyclerView) ntp1.getStream().getView()).getAdapter();

        // Check header is expanded.
        Assert.assertTrue(firstHeader.isExpandable() && firstHeader.isExpanded());
        Assert.assertTrue(adapter1.getItemCount() > expectedHeaderViewsCount);
        Assert.assertTrue(getPreferenceForArticleSectionHeader());

        // Toggle header on the current tab.
        onView(withId(R.id.header_title)).perform(click());

        // Check header is collapsed.
        Assert.assertTrue(firstHeader.isExpandable() && !firstHeader.isExpanded());
        Assert.assertEquals(expectedHeaderViewsCount, adapter1.getItemCount());
        Assert.assertFalse(getPreferenceForArticleSectionHeader());

        // Open a second new tab.
        Tab tab2 = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL);
        FeedNewTabPage ntp2 = (FeedNewTabPage) tab2.getNativePage();
        SectionHeader secondHeader = ntp2.getMediatorForTesting().getSectionHeaderForTesting();
        RecyclerView.Adapter adapter2 = ((RecyclerView) ntp2.getStream().getView()).getAdapter();

        // Check header on the second tab is collapsed.
        Assert.assertTrue(secondHeader.isExpandable() && !secondHeader.isExpanded());
        Assert.assertEquals(expectedHeaderViewsCount, adapter2.getItemCount());
        Assert.assertFalse(getPreferenceForArticleSectionHeader());

        // Toggle header on the second tab.
        onView(withId(R.id.header_title)).perform(click());

        // Check header on the second tab is expanded.
        Assert.assertTrue(secondHeader.isExpandable() && secondHeader.isExpanded());
        Assert.assertTrue(adapter2.getItemCount() > expectedHeaderViewsCount);
        Assert.assertTrue(getPreferenceForArticleSectionHeader());

        // Go back to the first tab and wait for a stable recycler view.
        ChromeTabUtils.switchTabInCurrentTabModel(mActivityTestRule.getActivity(), tab1.getId());

        // Check header on the first tab is expanded.
        Assert.assertTrue(firstHeader.isExpandable() && firstHeader.isExpanded());
        Assert.assertTrue(adapter1.getItemCount() > expectedHeaderViewsCount);
        Assert.assertTrue(getPreferenceForArticleSectionHeader());

        // Reset state.
        SignInPromo.setDisablePromoForTests(false);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/888996")
    @Feature({"FeedNewTabPage"})
    public void testFeedDisabledByPolicy() throws Exception {
        final boolean pref = ThreadUtils.runOnUiThreadBlocking(
                () -> PrefServiceBridge.getInstance().getBoolean(
                        Pref.NTP_ARTICLES_SECTION_ENABLED));

        // Policy is disabled. Verify the NTP root view contains only the Stream view as child.
        ViewGroup rootView = (ViewGroup) mNtp.getView();
        ViewUtils.waitForStableView(rootView);
        Assert.assertNotNull(mNtp.getStream());
        Assert.assertNull(mNtp.getScrollViewForPolicy());
        Assert.assertEquals(1, rootView.getChildCount());
        Assert.assertEquals(mNtp.getStream().getView(), rootView.getChildAt(0));

        // Simulate that policy is enabled. Verify the NTP root view contains only the view for
        // policy as child.
        ThreadUtils.runOnUiThreadBlocking(() -> PrefServiceBridge.getInstance().setBoolean(
                Pref.NTP_ARTICLES_SECTION_ENABLED, false));
        ViewUtils.waitForStableView(rootView);
        Assert.assertNotNull(mNtp.getScrollViewForPolicy());
        Assert.assertNull(mNtp.getStream());
        Assert.assertEquals(1, rootView.getChildCount());
        Assert.assertEquals(mNtp.getScrollViewForPolicy(), rootView.getChildAt(0));

        // Open a new tab while policy is still enabled.
        Tab tab2 = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL);
        FeedNewTabPage ntp2 = (FeedNewTabPage) tab2.getNativePage();
        ViewGroup rootView2 = (ViewGroup) ntp2.getView();

        // Verify that NTP root view contains only the view for policy as child.
        ViewUtils.waitForStableView(rootView2);
        Assert.assertNotNull(ntp2.getScrollViewForPolicy());
        Assert.assertNull(ntp2.getStream());
        Assert.assertEquals(1, rootView2.getChildCount());
        Assert.assertEquals(ntp2.getScrollViewForPolicy(), rootView2.getChildAt(0));

        // Simulate that policy is disabled. Verify the NTP root view is the view for policy. We
        // don't re-enable the Feed until the next restart.
        ThreadUtils.runOnUiThreadBlocking(() -> PrefServiceBridge.getInstance().setBoolean(
                Pref.NTP_ARTICLES_SECTION_ENABLED, true));
        ViewUtils.waitForStableView(rootView2);
        Assert.assertNotNull(ntp2.getScrollViewForPolicy());
        Assert.assertNull(ntp2.getStream());
        Assert.assertEquals(1, rootView2.getChildCount());
        Assert.assertEquals(ntp2.getScrollViewForPolicy(), rootView2.getChildAt(0));

        // Switch to the old tab. Verify the NTP root view is the view for policy.
        ChromeTabUtils.switchTabInCurrentTabModel(mActivityTestRule.getActivity(), mTab.getId());
        ViewUtils.waitForStableView(rootView);
        Assert.assertNotNull(mNtp.getScrollViewForPolicy());
        Assert.assertNull(mNtp.getStream());
        Assert.assertEquals(1, rootView.getChildCount());
        Assert.assertEquals(mNtp.getScrollViewForPolicy(), rootView.getChildAt(0));

        // Reset state.
        ThreadUtils.runOnUiThreadBlocking(() -> PrefServiceBridge.getInstance().setBoolean(
                Pref.NTP_ARTICLES_SECTION_ENABLED, pref));
    }

    private boolean getPreferenceForArticleSectionHeader() throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> PrefServiceBridge.getInstance().getBoolean(Pref.NTP_ARTICLES_LIST_VISIBLE));
    }
}
