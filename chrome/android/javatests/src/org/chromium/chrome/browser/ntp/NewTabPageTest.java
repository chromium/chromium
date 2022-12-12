// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.ViewUtils.waitForView;

import android.content.ComponentCallbacks2;
import android.graphics.Canvas;
import android.support.test.InstrumentationRegistry;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.FeatureList;
import org.chromium.base.GarbageCollectionTestUtils;
import org.chromium.base.MemoryPressureListener;
import org.chromium.base.memory.MemoryPressureCallback;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.feed.FeedReliabilityLogger;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

import java.io.IOException;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
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
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "disable-features=IPH_FeedHeaderMenu"})
@Features.DisableFeatures({ChromeFeatureList.QUERY_TILES, ChromeFeatureList.VIDEO_TUTORIALS})
public class NewTabPageTest {
    /**
     * Parameter set controlling whether scrollable mvt is enabled.
     */
    public static class MVTParams implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            return Arrays.asList(new ParameterSet().value(true).name("EnableScrollableMVTOnNTP"),
                    new ParameterSet().value(false).name("DisableScrollableMVTOnNTP"));
        }
    }

    private static final int ARTICLE_SECTION_HEADER_POSITION = 1;
    private static final int SIGNIN_PROMO_POSITION = 2;

    private static final int RENDER_TEST_REVISION = 5;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    @Rule
    public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();
    @Rule
    public SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(RENDER_TEST_REVISION)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_NEW_TAB_PAGE)
                    .build();
    @Mock
    OmniboxStub mOmniboxStub;
    @Mock
    VoiceRecognitionHandler mVoiceRecognitionHandler;
    @Mock
    FeedReliabilityLogger mFeedReliabilityLogger;
    @Mock
    private TemplateUrlService mTemplateUrlService;

    private static final String TEST_PAGE = "/chrome/test/data/android/navigate/simple.html";
    private static final String TEST_FEED =
            UrlUtils.getIsolatedTestFilePath("/chrome/test/data/android/feed/hello_world.gcl.bin");

    private Tab mTab;
    private NewTabPage mNtp;
    private View mFakebox;
    private ViewGroup mMvTilesLayout;
    private FakeMostVisitedSites mMostVisitedSites;
    private EmbeddedTestServer mTestServer;
    private List<SiteSuggestion> mSiteSuggestions;
    private OmniboxTestUtils mOmnibox;
    private boolean mEnableScrollableMVT;

    @ParameterAnnotations.UseMethodParameterBefore(MVTParams.class)
    public void setIsScrollableMVTEnabledForTest(boolean isScrollableMVTEnabled) {
        mEnableScrollableMVT = isScrollableMVTEnabled;
        FeatureList.TestValues testValuesOverride = new FeatureList.TestValues();
        testValuesOverride.addFeatureFlagOverride(
                ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_ANDROID, isScrollableMVTEnabled);
        FeatureList.setTestValues(testValuesOverride);
    }

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityWithURL("about:blank");
        Assume.assumeFalse(mActivityTestRule.getActivity().isTablet() && mEnableScrollableMVT);

        mOmnibox = new OmniboxTestUtils(mActivityTestRule.getActivity());

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
        mMvTilesLayout = mNtp.getView().findViewById(R.id.mv_tiles_layout);
        Assert.assertEquals(mSiteSuggestions.size(), mMvTilesLayout.getChildCount());
        mNtp.getCoordinatorForTesting().setReliabilityLoggerForTesting(mFeedReliabilityLogger);
    }

    @After
    public void tearDown() {
        if (mTestServer != null) {
            mTestServer.stopAndDestroyServer();
        }
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage", "FeedNewTabPage", "RenderTest"})
    @ParameterAnnotations.UseMethodParameter(MVTParams.class)
    // Disable sign-in to suppress sync promo, as it's unrelated to this render test.
    @Policies.Add(@Policies.Item(key = "BrowserSignin", string = "0"))
    public void testRender_FocusFakeBoxT(boolean isScrollableMVTEnabled) throws Exception {
        ScrimCoordinator scrimCoordinator = mActivityTestRule.getActivity()
                                                    .getRootUiCoordinatorForTesting()
                                                    .getScrimCoordinatorForTesting();
        scrimCoordinator.disableAnimationForTesting(true);
        onView(withId(R.id.search_box)).perform(click());
        ChromeRenderTestRule.sanitize(mNtp.getView().getRootView());
        mRenderTestRule.render(mNtp.getView().getRootView(),
                "focus_fake_box"
                        + (mEnableScrollableMVT ? "_with_scrollable_mvt"
                                                : "_with_non_scrollable_mvt"));
        scrimCoordinator.disableAnimationForTesting(false);
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage", "FeedNewTabPage", "RenderTest"})
    public void testRender_ArticleSectionHeader() throws Exception {
        // Scroll to the article section header in case it is not visible.
        onView(withId(R.id.feed_stream_recycler_view))
                .perform(RecyclerViewActions.scrollToPosition(ARTICLE_SECTION_HEADER_POSITION));
        waitForView((ViewGroup) mNtp.getView(), allOf(withId(R.id.header_title), isDisplayed()));
        View view = mNtp.getCoordinatorForTesting().getSectionHeaderViewForTesting();
        // Check header is expanded.
        mRenderTestRule.render(view, "expandable_header_expanded");

        // Toggle header on the current tab.
        onView(withId(R.id.feed_stream_recycler_view))
                .perform(RecyclerViewActions.scrollToPosition(ARTICLE_SECTION_HEADER_POSITION));
        waitForView((ViewGroup) mNtp.getView(), allOf(withId(R.id.header_title), isDisplayed()));
        onView(withId(R.id.header_title)).perform(click());
        // Check header is collapsed.
        mRenderTestRule.render(view, "expandable_header_collapsed");
    }

    /**
     * Tests that clicking on the fakebox causes it to animate upwards and focus the omnibox, and
     * defocusing the omnibox causes the fakebox to animate back down.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    public void testFocusFakebox() {
        int initialFakeboxTop = getFakeboxTop(mNtp);

        TouchCommon.singleClickView(mFakebox);

        waitForFakeboxFocusAnimationComplete(mNtp);
        mOmnibox.checkFocus(true);
        if (!mActivityTestRule.getActivity().isTablet()) {
            int afterFocusFakeboxTop = getFakeboxTop(mNtp);
            Assert.assertTrue(afterFocusFakeboxTop < initialFakeboxTop);
        }

        mOmnibox.clearFocus();
        waitForFakeboxTopPosition(mNtp, initialFakeboxTop);
    }

    /**
     * Tests that clicking on the fakebox causes it to focus the omnibox and allows typing and
     * navigating to a URL.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    @DisabledTest(message = "https://crbug.com/1033654")
    public void testSearchFromFakebox() {
        TouchCommon.singleClickView(mFakebox);
        waitForFakeboxFocusAnimationComplete(mNtp);
        final UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        mOmnibox.requestFocus();
        mOmnibox.typeText(UrlConstants.VERSION_URL, false);
        mOmnibox.checkSuggestionsShown();
        mOmnibox.sendKey(KeyEvent.KEYCODE_ENTER);
    }

    /**
     * Tests clicking on a most visited item.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    @ParameterAnnotations.UseMethodParameter(MVTParams.class)
    public void testClickMostVisitedItem(boolean isScrollableMVTEnabled) {
        Assert.assertNotNull(mMvTilesLayout);
        ChromeTabUtils.waitForTabPageLoaded(
                mTab, mSiteSuggestions.get(0).url.getSpec(), new Runnable() {
                    @Override
                    public void run() {
                        View mostVisitedItem = mMvTilesLayout.getChildAt(0);
                        TouchCommon.singleClickView(mostVisitedItem);
                    }
                });
        Assert.assertEquals(mSiteSuggestions.get(0).url, ChromeTabUtils.getUrlOnUiThread(mTab));
    }

    /**
     * Tests opening a most visited item in a new tab.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    @ParameterAnnotations.UseMethodParameter(MVTParams.class)
    @DisabledTest(message = "Flaky - crbug.com/543138")
    public void testOpenMostVisitedItemInNewTab(boolean isScrollableMVTEnabled)
            throws ExecutionException {
        Assert.assertNotNull(mMvTilesLayout);
        ChromeTabUtils.invokeContextMenuAndOpenInANewTab(mActivityTestRule,
                mMvTilesLayout.getChildAt(0), ContextMenuManager.ContextMenuItemId.OPEN_IN_NEW_TAB,
                false, mSiteSuggestions.get(0).url.getSpec());
    }

    /**
     * Tests opening a most visited item in a new incognito tab.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    @ParameterAnnotations.UseMethodParameter(MVTParams.class)
    public void testOpenMostVisitedItemInIncognitoTab(boolean isScrollableMVTEnabled)
            throws ExecutionException {
        Assert.assertNotNull(mMvTilesLayout);
        ChromeTabUtils.invokeContextMenuAndOpenInANewTab(mActivityTestRule,
                mMvTilesLayout.getChildAt(0),
                ContextMenuManager.ContextMenuItemId.OPEN_IN_INCOGNITO_TAB, true,
                mSiteSuggestions.get(0).url.getSpec());
    }

    /**
     * Tests deleting a most visited item.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    @ParameterAnnotations.UseMethodParameter(MVTParams.class)
    @DisabledTest(message = "crbug.com/1036500")
    public void testRemoveMostVisitedItem(boolean isScrollableMVTEnabled)
            throws ExecutionException {
        Assert.assertNotNull(mMvTilesLayout);
        SiteSuggestion testSite = mSiteSuggestions.get(0);
        View mostVisitedItem = mMvTilesLayout.getChildAt(0);
        ArrayList<View> views = new ArrayList<>();
        mMvTilesLayout.findViewsWithText(views, testSite.title, View.FIND_VIEWS_WITH_TEXT);
        Assert.assertEquals(1, views.size());

        TestTouchUtils.performLongClickOnMainSync(
                InstrumentationRegistry.getInstrumentation(), mostVisitedItem);
        Assert.assertTrue(InstrumentationRegistry.getInstrumentation().invokeContextMenuAction(
                mActivityTestRule.getActivity(), ContextMenuManager.ContextMenuItemId.REMOVE, 0));

        Assert.assertTrue(mMostVisitedSites.isUrlBlocklisted(testSite.url));
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    public void testUrlFocusAnimationsDisabledOnLoad() {
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
    public void testUrlFocusAnimationsEnabledOnFailedLoad() throws Exception {
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
            mOmnibox.checkFocus(true);
            mOmnibox.typeText(testPageUrl, false);
            mOmnibox.checkSuggestionsShown();

            final CallbackHelper loadedCallback = new CallbackHelper();
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                mTab.addObserver(new EmptyTabObserver() {
                    @Override
                    public void onPageLoadFinished(Tab tab, GURL url) {
                        loadedCallback.notifyCalled();
                        tab.removeObserver(this);
                    }
                });
            });

            mOmnibox.sendKey(KeyEvent.KEYCODE_ENTER);
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
    public void testSetSearchProviderInfo() throws Throwable {
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        mActivityTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                NewTabPageLayout ntpLayout = mNtp.getNewTabPageLayout();
                View logoView = ntpLayout.findViewById(R.id.search_provider_logo);
                Assert.assertEquals(View.VISIBLE, logoView.getVisibility());

                ntpLayout.setSearchProviderInfo(/* hasLogo = */ false, /* isGoogle */ true);
                // Mock to notify the template URL service observer.
                when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(false);
                when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
                ntpLayout.getLogoCoordinatorForTesting().onTemplateURLServiceChangedForTesting();
                Assert.assertEquals(View.GONE, logoView.getVisibility());

                ntpLayout.setSearchProviderInfo(/* hasLogo = */ true, /* isGoogle */ true);
                // Mock to notify the template URL service observer.
                when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(true);
                ntpLayout.getLogoCoordinatorForTesting().onTemplateURLServiceChangedForTesting();
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
    @ParameterAnnotations.UseMethodParameter(MVTParams.class)
    public void testPlaceholder(boolean isScrollableMVTEnabled) {
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(true);

        final NewTabPageLayout ntpLayout = mNtp.getNewTabPageLayout();
        final View logoView = ntpLayout.findViewById(R.id.search_provider_logo);
        final View searchBoxView = ntpLayout.findViewById(R.id.search_box);

        // Initially, the logo is visible, the search box is visible, there is one tile suggestion,
        // and the placeholder has not been inflated yet.
        Assert.assertEquals(View.VISIBLE, logoView.getVisibility());
        Assert.assertEquals(View.VISIBLE, searchBoxView.getVisibility());
        Assert.assertEquals(8, mMvTilesLayout.getChildCount());
        Assert.assertNull(mNtp.getView().findViewById(R.id.tile_grid_placeholder));

        // When the search provider has no logo and there are no tile suggestions, the placeholder
        // is shown.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(false);
            when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
            ntpLayout.setSearchProviderInfo(/* hasLogo = */ false, /* isGoogle */ true);
            // Mock to notify the template URL service observer.
            ntpLayout.getLogoCoordinatorForTesting().onTemplateURLServiceChangedForTesting();

            Assert.assertEquals(View.GONE, logoView.getVisibility());
            Assert.assertEquals(View.GONE, searchBoxView.getVisibility());

            mMostVisitedSites.setTileSuggestions(new String[] {});
            ntpLayout.onSwitchToForeground(); // Force tile refresh.
            // Mock to notify the template URL service observer.
            ntpLayout.getMostVisitedTilesCoordinatorForTesting()
                    .onTemplateURLServiceChangedForTesting();
        });
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    "The tile grid was not updated.", mMvTilesLayout.getChildCount(), is(0));
        });
        Assert.assertNotNull(mNtp.getView().findViewById(R.id.tile_grid_placeholder));
        Assert.assertEquals(View.VISIBLE,
                mNtp.getView().findViewById(R.id.tile_grid_placeholder).getVisibility());

        // Once the search provider has a logo again, the logo and search box are shown again and
        // the placeholder is hidden.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(true);
            when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
            ntpLayout.setSearchProviderInfo(/* hasLogo = */ true, /* isGoogle */ true);
            // Mock to notify the template URL service observer.
            ntpLayout.getLogoCoordinatorForTesting().onTemplateURLServiceChangedForTesting();

            Assert.assertEquals(View.VISIBLE, logoView.getVisibility());
            Assert.assertEquals(View.VISIBLE, searchBoxView.getVisibility());

            // Mock to notify the template URL service observer.
            ntpLayout.getMostVisitedTilesCoordinatorForTesting()
                    .onTemplateURLServiceChangedForTesting();
            Assert.assertEquals(View.GONE,
                    mNtp.getView().findViewById(R.id.tile_grid_placeholder).getVisibility());
        });
    }

    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    public void testMemoryPressure() throws Exception {
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

    @Test
    @DisabledTest(message = "Test is flaky. crbug.com/1077724")
    @SmallTest
    @Feature("NewTabPage")
    public void testNewTabPageCanBeGarbageCollected() throws IOException {
        WeakReference<NewTabPage> ntpRef = new WeakReference<>(mNtp);

        mActivityTestRule.loadUrl("about:blank");

        mNtp = null;
        mMostVisitedSites = null;
        mSuggestionsDeps.getFactory().mostVisitedSites = null;
        mFakebox = null;
        mMvTilesLayout = null;
        mTab = null;

        Assert.assertTrue(GarbageCollectionTestUtils.canBeGarbageCollected(ntpRef));
    }

    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    public void testSettingOmniboxStubAddsVoiceObserver() throws IOException {
        when(mOmniboxStub.getVoiceRecognitionHandler()).thenReturn(mVoiceRecognitionHandler);
        when(mVoiceRecognitionHandler.isVoiceSearchEnabled()).thenReturn(true);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mNtp.setOmniboxStub(mOmniboxStub);
            verify(mVoiceRecognitionHandler).addObserver(eq(mNtp));
            View micButton = mNtp.getView().findViewById(R.id.voice_search_button);
            assertEquals(View.VISIBLE, micButton.getVisibility());

            when(mVoiceRecognitionHandler.isVoiceSearchEnabled()).thenReturn(false);
            mNtp.onVoiceAvailabilityImpacted();
            assertEquals(View.GONE, micButton.getVisibility());
        });
    }

    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    public void testSettingOmniboxStubAddsUrlFocusChangeListener() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mNtp.setOmniboxStub(mOmniboxStub);
            verify(mOmniboxStub).addUrlFocusChangeListener(eq(mFeedReliabilityLogger));
        });
    }

    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    public void testFeedReliabilityLoggingFocusOmnibox() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mNtp.getNewTabPageManagerForTesting().focusSearchBox(
                    /*beginVoiceSearch=*/false, /*pastedText=*/"");
            verify(mFeedReliabilityLogger).onOmniboxFocused();
        });
    }

    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    public void testFeedReliabilityLoggingVoiceSearch() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mNtp.getNewTabPageManagerForTesting().focusSearchBox(
                    /*beginVoiceSearch=*/true, /*pastedText=*/"");
            verify(mFeedReliabilityLogger).onVoiceSearch();
        });
    }

    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    public void testFeedReliabilityLoggingHideWithBack() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeTabbedActivity activity = (ChromeTabbedActivity) mActivityTestRule.getActivity();
            activity.handleBackPressed();
            verify(mFeedReliabilityLogger).onNavigateBack();
        });
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
        CriteriaHelper.pollInstrumentationThread(
                () -> Criteria.checkThat(getUrlFocusAnimationsDisabled(), is(disabled)));
    }

    private void waitForTabLoading() {
        CriteriaHelper.pollUiThread(() -> mTab.isLoading());
    }

    private void waitForFakeboxFocusAnimationComplete(NewTabPage ntp) {
        // Tablet doesn't animate fakebox but simply focuses Omnibox upon click.
        // Skip the check on animation.
        if (mActivityTestRule.getActivity().isTablet()) return;
        waitForUrlFocusPercent(ntp, 1f);
    }

    private void waitForUrlFocusPercent(final NewTabPage ntp, float percent) {
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    ntp.getNewTabPageLayout().getUrlFocusChangeAnimationPercent(), is(percent));
        });
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
        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(getFakeboxTop(ntp), is(position)));
    }
}
