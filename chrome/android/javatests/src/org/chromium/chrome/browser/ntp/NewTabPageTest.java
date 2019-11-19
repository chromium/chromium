// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.instanceOf;

import static org.chromium.chrome.test.util.ViewUtils.waitForView;

import android.content.ComponentCallbacks2;
import android.graphics.Canvas;
import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.contrib.RecyclerViewActions;
import android.support.test.filters.LargeTest;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.support.v7.widget.RecyclerView;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.MemoryPressureListener;
import org.chromium.base.memory.MemoryPressureCallback;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.feed.FeedNewTabPage;
import org.chromium.chrome.browser.feed.FeedProcessScopeFactory;
import org.chromium.chrome.browser.feed.TestNetworkClient;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.ntp.cards.NewTabPageAdapter;
import org.chromium.chrome.browser.ntp.cards.NewTabPageRecyclerView;
import org.chromium.chrome.browser.ntp.cards.SignInPromo;
import org.chromium.chrome.browser.ntp.cards.SuggestionsSection;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SectionHeader;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.KeyUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.policy.test.annotations.Policies;
import org.chromium.ui.base.PageTransition;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/**
 * Tests for the native android New Tab Page.
 *
 * TODO(https://crbug.com/906151): Add new goldens and enable ExploreSites.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@Features.DisableFeatures(ChromeFeatureList.EXPLORE_SITES)
@RetryOnFailure
public class NewTabPageTest {
    private static final int ARTICLE_SECTION_HEADER_POSITION = 1;
    private static final int SIGNIN_PROMO_POSITION = 2;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    @Rule
    public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    @Rule
    public RenderTestRule mRenderTestRule =
            new RenderTestRule("chrome/test/data/android/render_tests");

    /** Parameter provider for enabling/disabling "Interest Feed Content Suggestions". */
    public static class InterestFeedParams implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            // Don't run tests for the dummy version of the FeedNewTabPage because content
            // suggestions dependencies may not be initialized.
            if (FeedNewTabPage.isDummy()) {
                return Collections.singletonList(
                        new ParameterSet().value(false).name("DisableInterestFeed"));
            } else {
                return Arrays.asList(new ParameterSet().value(false).name("DisableInterestFeed"),
                        new ParameterSet().value(true).name("EnableInterestFeed"));
            }
        }
    }

    private static final String TEST_PAGE = "/chrome/test/data/android/navigate/simple.html";
    private static final String TEST_FEED =
            UrlUtils.getIsolatedTestFilePath("/chrome/test/data/android/feed/hello_world.gcl.bin");

    // Anything not parameterized runs with Feed disabled.
    private boolean mInterestFeedEnabled;
    private Tab mTab;
    private NewTabPage mNtp;
    private View mFakebox;
    private ViewGroup mTileGridLayout;
    private FakeMostVisitedSites mMostVisitedSites;
    private EmbeddedTestServer mTestServer;
    private List<SiteSuggestion> mSiteSuggestions;

    @ParameterAnnotations.UseMethodParameterBefore(InterestFeedParams.class)
    public void setupInterestFeed(boolean interestFeedEnabled) {
        mInterestFeedEnabled = interestFeedEnabled;
    }

    @Before
    public void setUp() throws Exception {
        if (mInterestFeedEnabled) {
            Features.getInstance().enable(ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS);
            TestNetworkClient client = new TestNetworkClient();
            client.setNetworkResponseFile(TEST_FEED);
            FeedProcessScopeFactory.setTestNetworkClient(client);
        } else {
            Features.getInstance().disable(ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS);
        }
        mActivityTestRule.startMainActivityWithURL("about:blank");

        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());

        mSiteSuggestions = NewTabPageTestUtils.createFakeSiteSuggestions(mTestServer);
        mMostVisitedSites = new FakeMostVisitedSites();
        mMostVisitedSites.setTileSuggestions(mSiteSuggestions);
        mSuggestionsDeps.getFactory().mostVisitedSites = mMostVisitedSites;

        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mTab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(mTab);

        Assert.assertTrue(mTab.getNativePage() instanceof NewTabPage);
        mNtp = (NewTabPage) mTab.getNativePage();
        mFakebox = mNtp.getView().findViewById(R.id.search_box);
        mTileGridLayout = mNtp.getView().findViewById(R.id.tile_grid_layout);
        Assert.assertEquals(mSiteSuggestions.size(), mTileGridLayout.getChildCount());
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
        if (mInterestFeedEnabled) {
            FeedProcessScopeFactory.setTestNetworkClient(null);
        }
    }

    @Test
    @DisabledTest(message = "https://crbug.com/813589")
    @MediumTest
    @Feature({"NewTabPage", "RenderTest"})
    public void testRender() throws IOException {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        RenderTestRule.sanitize(mNtp.getView());
        mRenderTestRule.render(mTileGridLayout, "most_visited");
        mRenderTestRule.render(mFakebox, "fakebox");
        mRenderTestRule.render(mNtp.getView().getRootView(), "new_tab_page");

        RecyclerViewTestUtils.scrollToBottom(mNtp.getNewTabPageView().getRecyclerView());
        mRenderTestRule.render(mNtp.getView().getRootView(), "new_tab_page_scrolled");
    }

    @Test
    @DisabledTest(message = "https://crbug.com/888129")
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage", "RenderTest"})
    @ParameterAnnotations.UseMethodParameter(InterestFeedParams.class)
    public void testRender_FocusFakeBox(boolean interestFeedEnabled) throws Exception {
        ScrimView scrimView = mActivityTestRule.getActivity().getScrim();
        scrimView.disableAnimationForTesting(true);
        onView(withId(R.id.search_box)).perform(click());
        RenderTestRule.sanitize(mNtp.getView().getRootView());
        mRenderTestRule.render(mNtp.getView().getRootView(), "focus_fake_box");
        scrimView.disableAnimationForTesting(false);
    }

    @DisabledTest(message = "https://crbug.com/898165")
    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage", "RenderTest"})
    @ParameterAnnotations.UseMethodParameter(InterestFeedParams.class)
    public void testRender_SignInPromo(boolean interestFeedEnabled) throws Exception {
        // Scroll to the sign in promo in case it is not visible.
        onView(instanceOf(RecyclerView.class))
                .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));
        mRenderTestRule.render(mNtp.getSignInPromoViewForTesting(), "sign_in_promo");
    }

    @DisabledTest(message = "https://crbug.com/945293")
    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage", "RenderTest"})
    @ParameterAnnotations.UseMethodParameter(InterestFeedParams.class)
    public void testRender_ArticleSectionHeader(boolean interestFeedEnabled) throws Exception {
        // Scroll to the article section header in case it is not visible.
        onView(instanceOf(RecyclerView.class))
                .perform(RecyclerViewActions.scrollToPosition(ARTICLE_SECTION_HEADER_POSITION));
        waitForView((ViewGroup) mNtp.getView(), allOf(withId(R.id.header_title), isDisplayed()));
        View view = mNtp.getSectionHeaderViewForTesting();
        // Check header is expanded.
        mRenderTestRule.render(view, "expandable_header_expanded");

        // Toggle header on the current tab.
        onView(instanceOf(RecyclerView.class))
                .perform(RecyclerViewActions.scrollToPosition(ARTICLE_SECTION_HEADER_POSITION));
        waitForView((ViewGroup) mNtp.getView(), allOf(withId(R.id.header_title), isDisplayed()));
        onView(withId(R.id.header_title)).perform(click());
        // Check header is collapsed.
        mRenderTestRule.render(view, "expandable_header_collapsed");
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    public void testThumbnailInvalidations() throws Throwable {
        mActivityTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                captureThumbnail();
                Assert.assertFalse(mNtp.shouldCaptureThumbnail());

                // Check that we invalidate the thumbnail when the Recycler View is updated.
                NewTabPageRecyclerView recyclerView = mNtp.getNewTabPageView().getRecyclerView();

                recyclerView.getAdapter().notifyDataSetChanged();
                assertThumbnailInvalidAndRecapture();

                recyclerView.getAdapter().notifyItemChanged(0);
                assertThumbnailInvalidAndRecapture();

                recyclerView.getAdapter().notifyItemInserted(0);
                assertThumbnailInvalidAndRecapture();

                recyclerView.getAdapter().notifyItemMoved(0, 1);
                assertThumbnailInvalidAndRecapture();

                recyclerView.getAdapter().notifyItemRangeChanged(0, 1);
                assertThumbnailInvalidAndRecapture();

                recyclerView.getAdapter().notifyItemRangeInserted(0, 1);
                assertThumbnailInvalidAndRecapture();

                recyclerView.getAdapter().notifyItemRangeRemoved(0, 1);
                assertThumbnailInvalidAndRecapture();

                recyclerView.getAdapter().notifyItemRemoved(0);
                assertThumbnailInvalidAndRecapture();
            }
        });
    }

    /**
     * Tests that clicking on the fakebox causes it to animate upwards and focus the omnibox, and
     * defocusing the omnibox causes the fakebox to animate back down.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    @ParameterAnnotations.UseMethodParameter(InterestFeedParams.class)
    public void testFocusFakebox(boolean interestFeedEnabled) {
        int initialFakeboxTop = getFakeboxTop(mNtp);

        TouchCommon.singleClickView(mFakebox);
        waitForFakeboxFocusAnimationComplete(mNtp);
        UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, true);
        int afterFocusFakeboxTop = getFakeboxTop(mNtp);
        Assert.assertTrue(afterFocusFakeboxTop < initialFakeboxTop);

        OmniboxTestUtils.toggleUrlBarFocus(urlBar, false);
        waitForFakeboxTopPosition(mNtp, initialFakeboxTop);
        OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, false);
    }

    /**
     * Tests that clicking on the fakebox causes it to focus the omnibox and allows typing and
     * navigating to a URL.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    @DisableIf
            .Build(sdk_is_greater_than = 22, message = "crbug.com/593007")
            @ParameterAnnotations.UseMethodParameter(InterestFeedParams.class)
            public void testSearchFromFakebox(boolean interestFeedEnabled) {
        TouchCommon.singleClickView(mFakebox);
        waitForFakeboxFocusAnimationComplete(mNtp);
        final UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, true);

        InstrumentationRegistry.getInstrumentation().sendStringSync(TEST_PAGE);
        LocationBarLayout locationBar =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        OmniboxTestUtils.waitForOmniboxSuggestions(locationBar);

        ChromeTabUtils.waitForTabPageLoaded(mTab, null, new Runnable() {
            @Override
            public void run() {
                KeyUtils.singleKeyEventView(InstrumentationRegistry.getInstrumentation(), urlBar,
                        KeyEvent.KEYCODE_ENTER);
            }
        });
    }

    /**
     * Tests clicking on a most visited item.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    @ParameterAnnotations.UseMethodParameter(InterestFeedParams.class)
    public void testClickMostVisitedItem(boolean interestFeedEnabled) {
        ChromeTabUtils.waitForTabPageLoaded(mTab, mSiteSuggestions.get(0).url, new Runnable() {
            @Override
            public void run() {
                View mostVisitedItem = mTileGridLayout.getChildAt(0);
                TouchCommon.singleClickView(mostVisitedItem);
            }
        });
        Assert.assertEquals(mSiteSuggestions.get(0).url, mTab.getUrl());
    }

    /**
     * Tests opening a most visited item in a new tab.
     */
    @Test
    @DisabledTest // Flaked on the try bot. http://crbug.com/543138
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    @ParameterAnnotations.UseMethodParameter(InterestFeedParams.class)
    public void testOpenMostVisitedItemInNewTab(boolean interestFeedEnabled)
            throws ExecutionException {
        ChromeTabUtils.invokeContextMenuAndOpenInANewTab(mActivityTestRule,
                mTileGridLayout.getChildAt(0), ContextMenuManager.ContextMenuItemId.OPEN_IN_NEW_TAB,
                false, mSiteSuggestions.get(0).url);
    }

    /**
     * Tests opening a most visited item in a new incognito tab.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    @ParameterAnnotations.UseMethodParameter(InterestFeedParams.class)
    public void testOpenMostVisitedItemInIncognitoTab(boolean interestFeedEnabled)
            throws ExecutionException {
        ChromeTabUtils.invokeContextMenuAndOpenInANewTab(mActivityTestRule,
                mTileGridLayout.getChildAt(0),
                ContextMenuManager.ContextMenuItemId.OPEN_IN_INCOGNITO_TAB, true,
                mSiteSuggestions.get(0).url);
    }

    /**
     * Tests deleting a most visited item.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    @ParameterAnnotations.UseMethodParameter(InterestFeedParams.class)
    public void testRemoveMostVisitedItem(boolean interestFeedEnabled) throws ExecutionException {
        SiteSuggestion testSite = mSiteSuggestions.get(0);
        View mostVisitedItem = mTileGridLayout.getChildAt(0);
        ArrayList<View> views = new ArrayList<>();
        mTileGridLayout.findViewsWithText(views, testSite.title, View.FIND_VIEWS_WITH_TEXT);
        Assert.assertEquals(1, views.size());

        TestTouchUtils.performLongClickOnMainSync(
                InstrumentationRegistry.getInstrumentation(), mostVisitedItem);
        Assert.assertTrue(InstrumentationRegistry.getInstrumentation().invokeContextMenuAction(
                mActivityTestRule.getActivity(), ContextMenuManager.ContextMenuItemId.REMOVE, 0));

        Assert.assertTrue(mMostVisitedSites.isUrlBlacklisted(testSite.url));
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    @ParameterAnnotations.UseMethodParameter(InterestFeedParams.class)
    public void testUrlFocusAnimationsDisabledOnLoad(boolean interestFeedEnabled) {
        Assert.assertFalse(getUrlFocusAnimationsDisabled());
        ChromeTabUtils.waitForTabPageLoaded(mTab, mTestServer.getURL(TEST_PAGE), new Runnable() {
            @Override
            public void run() {
                TestThreadUtils.runOnUiThreadBlocking(() -> {
                    int pageTransition = PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR;
                    mTab.loadUrl(new LoadUrlParams(mTestServer.getURL(TEST_PAGE), pageTransition));
                    // It should be disabled as soon as a load URL is triggered.
                    Assert.assertTrue(getUrlFocusAnimationsDisabled());
                });
            }
        });
        // Ensure it is still marked as disabled once the new page is fully loaded.
        Assert.assertTrue(getUrlFocusAnimationsDisabled());
    }

    @Test
    @LargeTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    @ParameterAnnotations.UseMethodParameter(InterestFeedParams.class)
    public void testUrlFocusAnimationsEnabledOnFailedLoad(boolean interestFeedEnabled)
            throws Exception {
        // TODO(jbudorick): switch this to EmbeddedTestServer.
        TestWebServer webServer = TestWebServer.start();
        try {
            final Semaphore delaySemaphore = new Semaphore(0);
            Runnable delayAction = new Runnable() {
                @Override
                public void run() {
                    try {
                        Assert.assertTrue(delaySemaphore.tryAcquire(10, TimeUnit.SECONDS));
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }
            };
            final String testPageUrl = webServer.setResponseWithRunnableAction(
                    "/ntp_test.html",
                    "<html><body></body></html>", null, delayAction);

            Assert.assertFalse(getUrlFocusAnimationsDisabled());

            clickFakebox();
            UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
            OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, true);
            mActivityTestRule.typeInOmnibox(testPageUrl, false);
            LocationBarLayout locationBar =
                    (LocationBarLayout) mActivityTestRule.getActivity().findViewById(
                            R.id.location_bar);
            OmniboxTestUtils.waitForOmniboxSuggestions(locationBar);

            final CallbackHelper loadedCallback = new CallbackHelper();
            mTab.addObserver(new EmptyTabObserver() {
                @Override
                public void onPageLoadFinished(Tab tab, String url) {
                    loadedCallback.notifyCalled();
                    tab.removeObserver(this);
                }
            });

            final View v = urlBar;
            KeyUtils.singleKeyEventView(
                    InstrumentationRegistry.getInstrumentation(), v, KeyEvent.KEYCODE_ENTER);

            waitForUrlFocusAnimationsDisabledState(true);
            waitForTabLoading();

            TestThreadUtils.runOnUiThreadBlocking(() -> { mTab.stopLoading(); });
            waitForUrlFocusAnimationsDisabledState(false);
            delaySemaphore.release();
            loadedCallback.waitForCallback(0);
            Assert.assertFalse(getUrlFocusAnimationsDisabled());
        } finally {
            webServer.shutdown();
        }
    }

    /**
     * Tests setting whether the search provider has a logo.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    @ParameterAnnotations.UseMethodParameter(InterestFeedParams.class)
    public void testSetSearchProviderInfo(boolean interestFeedEnabled) throws Throwable {
        mActivityTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                NewTabPageLayout ntpLayout = mNtp.getNewTabPageLayout();
                View logoView = ntpLayout.findViewById(R.id.search_provider_logo);
                Assert.assertEquals(View.VISIBLE, logoView.getVisibility());
                ntpLayout.setSearchProviderInfo(/* hasLogo = */ false, /* isGoogle */ true);
                Assert.assertEquals(View.GONE, logoView.getVisibility());
                ntpLayout.setSearchProviderInfo(/* hasLogo = */ true, /* isGoogle */ true);
                Assert.assertEquals(View.VISIBLE, logoView.getVisibility());
            }
        });
    }

    /**
     * Verifies that the placeholder is only shown when there are no tile suggestions and the search
     * provider has no logo.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    @ParameterAnnotations.UseMethodParameter(InterestFeedParams.class)
    public void testPlaceholder(boolean interestFeedEnabled) {
        final NewTabPageLayout ntpLayout = mNtp.getNewTabPageLayout();
        final View logoView = ntpLayout.findViewById(R.id.search_provider_logo);
        final View searchBoxView = ntpLayout.findViewById(R.id.search_box);

        // Initially, the logo is visible, the search box is visible, there is one tile suggestion,
        // and the placeholder has not been inflated yet.
        Assert.assertEquals(View.VISIBLE, logoView.getVisibility());
        Assert.assertEquals(View.VISIBLE, searchBoxView.getVisibility());
        Assert.assertEquals(8, mTileGridLayout.getChildCount());
        Assert.assertNull(ntpLayout.getPlaceholder());

        // When the search provider has no logo and there are no tile suggestions, the placeholder
        // is shown.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ntpLayout.setSearchProviderInfo(/* hasLogo = */ false, /* isGoogle */ true);
            Assert.assertEquals(View.GONE, logoView.getVisibility());
            Assert.assertEquals(View.GONE, searchBoxView.getVisibility());

            mMostVisitedSites.setTileSuggestions(new String[] {});

            ntpLayout.getTileGroup().onSwitchToForeground(false); // Force tile refresh.
        });
        CriteriaHelper.pollUiThread(new Criteria("The tile grid was not updated.") {
            @Override
            public boolean isSatisfied() {
                return mTileGridLayout.getChildCount() == 0;
            }
        });
        Assert.assertNotNull(ntpLayout.getPlaceholder());
        Assert.assertEquals(View.VISIBLE, ntpLayout.getPlaceholder().getVisibility());

        // Once the search provider has a logo again, the logo and search box are shown again and
        // the placeholder is hidden.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ntpLayout.setSearchProviderInfo(/* hasLogo = */ true, /* isGoogle */ true);
            Assert.assertEquals(View.VISIBLE, logoView.getVisibility());
            Assert.assertEquals(View.VISIBLE, searchBoxView.getVisibility());
            Assert.assertEquals(View.GONE, ntpLayout.getPlaceholder().getVisibility());
        });
    }

    @Test
    @SmallTest
    public void testRemoteSuggestionsEnabledByDefault() {
        Assert.assertTrue(
                mNtp.getManagerForTesting().getSuggestionsSource().areRemoteSuggestionsEnabled());
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add("disable-features=NTPArticleSuggestions")
    public void testRemoteSuggestionsEnabledWhenFeatureDisabled() {
        // Verifies crash from https://crbug.com/742056.
        Assert.assertFalse(
                mNtp.getManagerForTesting().getSuggestionsSource().areRemoteSuggestionsEnabled());
    }

    @Test
    @SmallTest
    @Policies.Add(@Policies.Item(key = "NTPContentSuggestionsEnabled", string = "false"))
    public void testRemoteSuggestionsEnabledWhenDisabledByPolicy() {
        Assert.assertFalse(
                mNtp.getManagerForTesting().getSuggestionsSource().areRemoteSuggestionsEnabled());
    }

    @Test
    @SmallTest
    @Feature({"NewTabPage"})
    public void testArticleExpandableHeaderOnMultipleTabs() throws Exception {
        // Disable the sign-in promo so the header is visible above the fold.
        SignInPromo.setDisablePromoForTests(true);

        // Open a new tab.
        SuggestionsSection firstSection = getArticleSectionOnNewTab();
        SectionHeader firstHeader = firstSection.getHeaderItemForTesting();
        int firstTabId = mActivityTestRule.getActivity().getActivityTab().getId();

        // Check header is expanded.
        Assert.assertTrue(firstHeader.isExpandable() && firstHeader.isExpanded());
        Assert.assertTrue(firstSection.getItemCount() > 1);
        Assert.assertTrue(getPreferenceForExpandableHeader());

        // Toggle header on the current tab.
        onView(withId(R.id.header_title)).perform(click());

        // Check header is collapsed.
        Assert.assertTrue(firstHeader.isExpandable() && !firstHeader.isExpanded());
        Assert.assertEquals(1, firstSection.getItemCount());
        Assert.assertFalse(getPreferenceForExpandableHeader());

        // Open a second new tab.
        SuggestionsSection secondSection = getArticleSectionOnNewTab();
        SectionHeader secondHeader = secondSection.getHeaderItemForTesting();

        // Check header on the second tab is collapsed.
        Assert.assertTrue(secondHeader.isExpandable() && !secondHeader.isExpanded());
        Assert.assertEquals(1, secondSection.getItemCount());
        Assert.assertFalse(getPreferenceForExpandableHeader());

        // Toggle header on the second tab.
        onView(withId(R.id.header_title)).perform(click());

        // Check header on the second tab is expanded.
        Assert.assertTrue(secondHeader.isExpandable() && secondHeader.isExpanded());
        Assert.assertTrue(secondSection.getItemCount() > 1);
        Assert.assertTrue(getPreferenceForExpandableHeader());

        // Go back to the first tab and check header is expanded.
        ChromeTabUtils.switchTabInCurrentTabModel(mActivityTestRule.getActivity(), firstTabId);
        Assert.assertTrue(firstHeader.isExpandable() && firstHeader.isExpanded());
        Assert.assertTrue(firstSection.getItemCount() > 1);
        Assert.assertTrue(getPreferenceForExpandableHeader());

        // Reset state.
        SignInPromo.setDisablePromoForTests(false);
    }

    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    @ParameterAnnotations.UseMethodParameter(InterestFeedParams.class)
    public void testMemoryPressure(boolean interestFeedEnabled) throws Exception {
        // TODO(twellington): This test currently just checks that sending a memory pressure
        // signal doesn't crash. Enhance the test to also check whether certain behaviors are
        // performed.
        CallbackHelper callback = new CallbackHelper();
        MemoryPressureCallback pressureCallback = pressure -> callback.notifyCalled();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            MemoryPressureListener.addCallback(pressureCallback);
            mActivityTestRule.getActivity().getApplication().onTrimMemory(
                    ComponentCallbacks2.TRIM_MEMORY_MODERATE);
        });

        callback.waitForCallback(0);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> MemoryPressureListener.removeCallback(pressureCallback));
    }

    private void assertThumbnailInvalidAndRecapture() {
        Assert.assertTrue(mNtp.shouldCaptureThumbnail());
        captureThumbnail();
        Assert.assertFalse(mNtp.shouldCaptureThumbnail());
    }

    private void captureThumbnail() {
        Canvas canvas = new Canvas();
        mNtp.captureThumbnail(canvas);
    }

    private boolean getUrlFocusAnimationsDisabled() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return mNtp.getNewTabPageLayout().urlFocusAnimationsDisabled();
            }
        });
    }

    private void waitForUrlFocusAnimationsDisabledState(boolean disabled) {
        CriteriaHelper.pollInstrumentationThread(Criteria.equals(disabled, new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return getUrlFocusAnimationsDisabled();
            }
        }));
    }

    private void waitForTabLoading() {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mTab.isLoading();
            }
        });
    }

    private void waitForFakeboxFocusAnimationComplete(NewTabPage ntp) {
        waitForUrlFocusPercent(ntp, 1f);
    }

    private void waitForUrlFocusPercent(final NewTabPage ntp, float percent) {
        CriteriaHelper.pollUiThread(Criteria.equals(percent, new Callable<Float>() {
            @Override
            public Float call() {
                return ntp.getNewTabPageLayout().getUrlFocusChangeAnimationPercent();
            }
        }));
    }

    private void clickFakebox() {
        View fakebox = mNtp.getView().findViewById(R.id.search_box);
        TouchCommon.singleClickView(fakebox);
    }

    /**
     * @return The position of the top of the fakebox relative to the window.
     */
    private int getFakeboxTop(final NewTabPage ntp) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<Integer>() {
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
     * Waits until the top of the fakebox reaches the given position.
     */
    private void waitForFakeboxTopPosition(final NewTabPage ntp, int position) {
        CriteriaHelper.pollUiThread(Criteria.equals(position, new Callable<Integer>() {
            @Override
            public Integer call() {
                return getFakeboxTop(ntp);
            }
        }));
    }

    private SuggestionsSection getArticleSectionOnNewTab() {
        Tab tab = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL);
        NewTabPage ntp = (NewTabPage) tab.getNativePage();
        NewTabPageAdapter adapter =
                (NewTabPageAdapter) ntp.getNewTabPageView().getRecyclerView().getAdapter();
        return adapter.getSectionListForTesting().getSection(KnownCategories.ARTICLES);
    }

    private boolean getPreferenceForExpandableHeader() throws Exception {
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> PrefServiceBridge.getInstance().getBoolean(Pref.NTP_ARTICLES_LIST_VISIBLE));
    }
}
