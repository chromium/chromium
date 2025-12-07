// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.longClick;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.ViewUtils.waitForView;

import android.content.ComponentCallbacks2;
import android.content.res.Resources;
import android.graphics.Point;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.GarbageCollectionTestUtils;
import org.chromium.base.MemoryPressureListener;
import org.chromium.base.ThreadUtils;
import org.chromium.base.memory.MemoryPressureCallback;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.composeplate.ComposeplateUtils;
import org.chromium.chrome.browser.feed.FeedActionDelegate;
import org.chromium.chrome.browser.feed.FeedReliabilityLogger;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.logo.LogoBridge;
import org.chromium.chrome.browser.logo.LogoBridgeJni;
import org.chromium.chrome.browser.logo.LogoCoordinator;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.Tile;
import org.chromium.chrome.browser.suggestions.tile.TileGroup;
import org.chromium.chrome.browser.suggestions.tile.TilesLinearLayout;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.tile.TileView;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;

import java.io.IOException;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/**
 * Tests for the native android New Tab Page.
 *
 * <p>TODO(crbug.com/40602800): Add new goldens and enable ExploreSites.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "disable-features=IPH_FeedHeaderMenu"
})
public class NewTabPageTest {
    private static final int ARTICLE_SECTION_HEADER_POSITION = 1;

    private static final int RENDER_TEST_REVISION = 8;

    private static final String HISTOGRAM_NTP_MODULE_CLICK = "NewTabPage.Module.Click";
    private static final String HISTOGRAM_NTP_MODULE_LONGCLICK = "NewTabPage.Module.LongClick";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();
    @Rule public SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(RENDER_TEST_REVISION)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_NEW_TAB_PAGE)
                    .build();

    @Mock OmniboxStub mOmniboxStub;
    @Mock VoiceRecognitionHandler mVoiceRecognitionHandler;
    @Mock FeedReliabilityLogger mFeedReliabilityLogger;
    @Mock private Callback mOnVisitComplete;
    @Mock FeedActionDelegate.PageLoadObserver mPageLoadObserver;
    @Mock LogoBridge.Natives mLogoBridgeJniMock;
    @Mock private LogoBridge mLogoBridge;

    private static final String TEST_PAGE = "/chrome/test/data/android/navigate/simple.html";
    private static final String TEST_FEED =
            UrlUtils.getIsolatedTestFilePath("/chrome/test/data/android/feed/hello_world.gcl.bin");
    private static final String TEST_URL = "https://www.example.com/";

    private static final String EMAIL = "email@gmail.com";
    private static final String NAME = "Email Emailson";

    private Tab mTab;
    private TemplateUrlService mTemplateUrlService;
    private NewTabPage mNtp;
    private View mFakebox;
    private TilesLinearLayout mMvTilesLayout;
    private FakeMostVisitedSites mMostVisitedSites;
    private EmbeddedTestServer mTestServer;
    private List<SiteSuggestion> mSiteSuggestions;
    private OmniboxTestUtils mOmnibox;

    @Before
    public void setUp() throws Exception {
        ComposeplateUtils.setIsEnabledForTesting(true);
        mActivityTestRule.startOnBlankPage();
        TemplateUrlService originalService =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                TemplateUrlServiceFactory.getForProfile(
                                        ProfileManager.getLastUsedRegularProfile()));
        mTemplateUrlService = Mockito.spy(originalService);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);

        mOmnibox = new OmniboxTestUtils(mActivityTestRule.getActivity());

        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());

        mSiteSuggestions = NewTabPageTestUtils.createFakeSiteSuggestions(mTestServer);
        mMostVisitedSites = new FakeMostVisitedSites();
        mMostVisitedSites.setTileSuggestions(mSiteSuggestions);
        mSuggestionsDeps.getFactory().mostVisitedSites = mMostVisitedSites;

        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mTab = mActivityTestRule.getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(mTab);

        Assert.assertTrue(mTab.getNativePage() instanceof NewTabPage);
        mNtp = (NewTabPage) mTab.getNativePage();
        mFakebox = mNtp.getView().findViewById(R.id.search_box);
        mMvTilesLayout = mNtp.getView().findViewById(R.id.mv_tiles_layout);
        Assert.assertEquals(mSiteSuggestions.size(), mMvTilesLayout.getTileCount());
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage", "FeedNewTabPage", "RenderTest"})
    // Disable sign-in to suppress sync promo, as it's unrelated to this render test.
    @Policies.Add(@Policies.Item(key = "BrowserSignin", string = "0"))
    public void testRender_FocusFakeBox() throws Exception {
        ScrimManager scrimManager =
                mActivityTestRule.getActivity().getRootUiCoordinatorForTesting().getScrimManager();
        scrimManager.disableAnimationForTesting(true);
        onView(withId(R.id.search_box)).perform(click());
        View view = mNtp.getView().findViewById(R.id.search_box);
        ChromeRenderTestRule.sanitize(view);
        mRenderTestRule.render(view, "focus_fake_box_v3");
        scrimManager.disableAnimationForTesting(false);
    }

    /**
     * If this test fails because of new buttons being added to the new tab page toolbar
     * (immediately adjacent to the real URL bar), ensure those buttons are manually wired for Tab
     * key navigation following this crbug.com/394169187.
     */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE)
    @Feature({"NewTabPage"})
    public void testToolBar_Phone() {
        ViewGroup toolBar = mActivityTestRule.getActivity().findViewById(R.id.toolbar);
        int[] toolbarContentIds =
                new int[] {
                    R.id.home_button,
                    R.id.home_page_buttons_stub,
                    R.id.location_bar_background_view,
                    R.id.location_bar,
                    R.id.toolbar_buttons
                };
        for (int i = 0; i < toolbarContentIds.length; i++) {
            assertEquals(toolbarContentIds[i], toolBar.getChildAt(i).getId());
        }

        ViewGroup toolBarButtons = (ViewGroup) toolBar.getChildAt(toolbarContentIds.length - 1);
        assertEquals(R.id.optional_toolbar_button_container, toolBarButtons.getChildAt(0).getId());
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage", "FeedNewTabPage", "RenderTest"})
    @DisableFeatures({"FeedHeaderRemoval", "WebFeedKillSwitch"})
    public void testRender_ArticleSectionHeader() throws Exception {
        // Scroll to the article section header in case it is not visible.
        onView(withId(R.id.feed_stream_recycler_view))
                .perform(RecyclerViewActions.scrollToPosition(ARTICLE_SECTION_HEADER_POSITION));
        waitForView((ViewGroup) mNtp.getView(), allOf(withId(R.id.header_title), isDisplayed()));
        View view = mNtp.getCoordinatorForTesting().getHeaderViewForTesting();
        // Check header is expanded.
        mRenderTestRule.render(view, "expandable_header_expanded_v2");

        // Toggle header on the current tab.
        onView(withId(R.id.feed_stream_recycler_view))
                .perform(RecyclerViewActions.scrollToPosition(ARTICLE_SECTION_HEADER_POSITION));
        waitForView((ViewGroup) mNtp.getView(), allOf(withId(R.id.header_title), isDisplayed()));
        onView(withId(R.id.header_title)).perform(click());
        // Check header is collapsed.
        mRenderTestRule.render(view, "expandable_header_collapsed_v3");
    }

    /**
     * Tests that clicking on the fakebox causes it to animate upwards and focus the omnibox, and
     * defocusing the omnibox causes the fakebox to animate back down.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    @DisableIf.Build(sdk_equals = Build.VERSION_CODES.R, message = "http://crbug.com/40664848")
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
        mOmnibox.requestFocus();
        mOmnibox.typeText(UrlConstants.VERSION_URL, false);
        mOmnibox.checkSuggestionsShown();
        mOmnibox.sendKey(KeyEvent.KEYCODE_ENTER);
    }

    /** Tests clicking on a most visited item. */
    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    public void testClickMostVisitedItem() {
        Assert.assertNotNull(mMvTilesLayout);
        HistogramWatcher histogramWatcher = expectMostVisitedTilesRecordForNtpModuleClick();

        ChromeTabUtils.waitForTabPageLoaded(
                mTab,
                mSiteSuggestions.get(0).url.getSpec(),
                new Runnable() {
                    @Override
                    public void run() {
                        TileView mostVisitedItem = mMvTilesLayout.getTileAt(0);
                        TouchCommon.singleClickView(mostVisitedItem);
                    }
                });

        Assert.assertEquals(mSiteSuggestions.get(0).url, ChromeTabUtils.getUrlOnUiThread(mTab));
        histogramWatcher.assertExpected();
    }

    /** Tests opening a most visited item in a new tab. */
    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    @DisabledTest(message = "Flaky - crbug.com/543138")
    public void testOpenMostVisitedItemInNewTab() throws ExecutionException {
        Assert.assertNotNull(mMvTilesLayout);
        ChromeTabUtils.invokeContextMenuAndOpenInANewTab(
                mActivityTestRule.getActivity(),
                mMvTilesLayout.getTileAt(0),
                ContextMenuManager.ContextMenuItemId.OPEN_IN_NEW_TAB,
                false,
                mSiteSuggestions.get(0).url.getSpec());
    }

    /** Tests deleting a most visited item. */
    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    @DisabledTest(message = "crbug.com/40664852")
    public void testRemoveMostVisitedItem() throws ExecutionException {
        Assert.assertNotNull(mMvTilesLayout);
        SiteSuggestion testSite = mSiteSuggestions.get(0);
        View mostVisitedItem = mMvTilesLayout.getTileAt(0);
        ArrayList<View> views = new ArrayList<>();
        mMvTilesLayout.findViewsWithText(views, testSite.title, View.FIND_VIEWS_WITH_TEXT);
        Assert.assertEquals(1, views.size());

        TestTouchUtils.performLongClickOnMainSync(
                InstrumentationRegistry.getInstrumentation(), mostVisitedItem);
        Assert.assertTrue(
                InstrumentationRegistry.getInstrumentation()
                        .invokeContextMenuAction(
                                mActivityTestRule.getActivity(),
                                ContextMenuManager.ContextMenuItemId.REMOVE,
                                0));

        Assert.assertTrue(mMostVisitedSites.isUrlBlocklisted(testSite.url));
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    public void testUrlFocusAnimationsDisabledOnLoad() {
        Assert.assertFalse(getUrlFocusAnimationsDisabled());
        ChromeTabUtils.waitForTabPageLoaded(
                mTab,
                mTestServer.getURL(TEST_PAGE),
                new Runnable() {
                    @Override
                    public void run() {
                        ThreadUtils.runOnUiThreadBlocking(
                                () -> {
                                    int pageTransition =
                                            PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR;
                                    mTab.loadUrl(
                                            new LoadUrlParams(
                                                    mTestServer.getURL(TEST_PAGE), pageTransition));
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
            Runnable delayAction =
                    new Runnable() {
                        @Override
                        public void run() {
                            try {
                                Assert.assertTrue(delaySemaphore.tryAcquire(10, TimeUnit.SECONDS));
                            } catch (InterruptedException e) {
                                e.printStackTrace();
                            }
                        }
                    };
            final String testPageUrl =
                    webServer.setResponseWithRunnableAction(
                            "/ntp_test.html", "<html><body></body></html>", null, delayAction);

            Assert.assertFalse(getUrlFocusAnimationsDisabled());

            clickFakebox();
            mOmnibox.checkFocus(true);
            mOmnibox.typeText(testPageUrl, false);
            mOmnibox.checkSuggestionsShown();

            final CallbackHelper loadedCallback = new CallbackHelper();
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mTab.addObserver(
                                new EmptyTabObserver() {
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

            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mTab.stopLoading();
                    });
            waitForUrlFocusAnimationsDisabledState(false);
            delaySemaphore.release();
            loadedCallback.waitForCallback(0);
            Assert.assertFalse(getUrlFocusAnimationsDisabled());
        } finally {
            webServer.shutdown();
        }
    }

    /** Tests setting whether the search provider has a logo. */
    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    public void testSetSearchProviderInfo() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(
                new Runnable() {
                    @Override
                    public void run() {
                        NewTabPageLayout ntpLayout = mNtp.getNewTabPageLayout();
                        View logoView = ntpLayout.findViewById(R.id.search_provider_logo);
                        Assert.assertEquals(View.VISIBLE, logoView.getVisibility());

                        ntpLayout.setSearchProviderInfo(/* hasLogo= */ false, /* isGoogle= */ true);
                        // Mock to notify the template URL service observer.
                        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo())
                                .thenReturn(false);
                        when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
                        ntpLayout
                                .getLogoCoordinatorForTesting()
                                .onTemplateURLServiceChangedForTesting();
                        Assert.assertEquals(View.GONE, logoView.getVisibility());

                        ntpLayout.setSearchProviderInfo(/* hasLogo= */ true, /* isGoogle= */ true);
                        // Mock to notify the template URL service observer.
                        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo())
                                .thenReturn(true);
                        ntpLayout
                                .getLogoCoordinatorForTesting()
                                .onTemplateURLServiceChangedForTesting();
                        Assert.assertEquals(View.VISIBLE, logoView.getVisibility());
                    }
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    MemoryPressureListener.addCallback(pressureCallback);
                    mActivityTestRule
                            .getActivity()
                            .getApplication()
                            .onTrimMemory(ComponentCallbacks2.TRIM_MEMORY_MODERATE);
                });

        callback.waitForCallback(0);
        ThreadUtils.runOnUiThreadBlocking(
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

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
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
        mNtp.getCoordinatorForTesting().setReliabilityLoggerForTesting(mFeedReliabilityLogger);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mNtp.setOmniboxStub(mOmniboxStub);
                    verify(mOmniboxStub).addUrlFocusChangeListener(eq(mFeedReliabilityLogger));
                });
    }

    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    public void testFeedReliabilityLoggingFocusOmnibox() throws IOException {
        mNtp.getCoordinatorForTesting().setReliabilityLoggerForTesting(mFeedReliabilityLogger);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mNtp.getNewTabPageManagerForTesting()
                            .focusSearchBox(
                                    /* beginVoiceSearch= */ false,
                                    AutocompleteRequestType.SEARCH,
                                    /* pastedText= */ "");
                });
    }

    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    public void testFeedReliabilityLoggingVoiceSearch() throws IOException {
        mNtp.getCoordinatorForTesting().setReliabilityLoggerForTesting(mFeedReliabilityLogger);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mNtp.getNewTabPageManagerForTesting()
                            .focusSearchBox(
                                    /* beginVoiceSearch= */ true,
                                    AutocompleteRequestType.SEARCH,
                                    /* pastedText= */ "");
                });
    }

    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    public void testFeedReliabilityLoggingHideWithBack() throws IOException {
        mNtp.getCoordinatorForTesting().setReliabilityLoggerForTesting(mFeedReliabilityLogger);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeTabbedActivity activity =
                            (ChromeTabbedActivity) mActivityTestRule.getActivity();
                    activity.getOnBackPressedDispatcher().onBackPressed();
                    verify(mFeedReliabilityLogger).onNavigateBack();
                });
    }

    /**
     * Test whether the clicking action on MV tiles in {@link NewTabPage} is been recorded in
     * histogram correctly.
     */
    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1434807")
    public void testRecordHistogramMostVisitedItemClick_Ntp() {
        Tile tileForTest = new Tile(mSiteSuggestions.get(0), 0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TileGroup.Delegate tileGroupDelegate = mNtp.getTileGroupDelegateForTesting();

                    // Test clicking on MV tiles.
                    HistogramWatcher histogramWatcher =
                            expectMostVisitedTilesRecordForNtpModuleClick();
                    tileGroupDelegate.openMostVisitedItem(
                            WindowOpenDisposition.CURRENT_TAB, tileForTest);
                    histogramWatcher.assertExpected(
                            HISTOGRAM_NTP_MODULE_CLICK
                                    + " is not recorded correctly when click on MV tiles.");

                    // Test long press then open in new tab in group on MV tiles.
                    histogramWatcher = expectMostVisitedTilesRecordForNtpModuleClick();
                    tileGroupDelegate.openMostVisitedItemInGroup(
                            WindowOpenDisposition.NEW_BACKGROUND_TAB, tileForTest);
                    histogramWatcher.assertExpected(
                            HISTOGRAM_NTP_MODULE_CLICK
                                    + " is not recorded correctly when long press then open in new"
                                    + " tab in group on MV tiles.");

                    // Test long press then open in new tab on MV tiles.
                    histogramWatcher = expectMostVisitedTilesRecordForNtpModuleClick();
                    tileGroupDelegate.openMostVisitedItem(
                            WindowOpenDisposition.NEW_BACKGROUND_TAB, tileForTest);
                    histogramWatcher.assertExpected(
                            HISTOGRAM_NTP_MODULE_CLICK
                                    + " is not recorded correctly when long press then open in new"
                                    + " tab on MV tiles.");

                    // Test long press then open in other window on MV tiles.
                    histogramWatcher = expectNoRecordsForNtpModuleClick();
                    tileGroupDelegate.openMostVisitedItem(
                            WindowOpenDisposition.NEW_WINDOW, tileForTest);
                    histogramWatcher.assertExpected(
                            HISTOGRAM_NTP_MODULE_CLICK
                                    + " shouldn't be recorded when long press then open in other"
                                    + " window on MV tiles.");

                    // Test long press then download link on MV tiles.
                    histogramWatcher = expectMostVisitedTilesRecordForNtpModuleClick();
                    tileGroupDelegate.openMostVisitedItem(
                            WindowOpenDisposition.SAVE_TO_DISK, tileForTest);
                    histogramWatcher.assertExpected(
                            HISTOGRAM_NTP_MODULE_CLICK
                                    + " is not recorded correctly when long press then download"
                                    + " link on MV tiles.");

                    // Test long press then open in Incognito tab on MV tiles.
                    histogramWatcher = expectMostVisitedTilesRecordForNtpModuleClick();
                    tileGroupDelegate.openMostVisitedItem(
                            WindowOpenDisposition.OFF_THE_RECORD, tileForTest);
                    histogramWatcher.assertExpected(
                            HISTOGRAM_NTP_MODULE_CLICK
                                    + " is not recorded correctly when long press then open in"
                                    + " Incognito tab on MV tiles.");
                });
    }

    /**
     * Test whether the clicking action on Feeds in {@link NewTabPage} is been recorded in histogram
     * correctly.
     */
    @Test
    @SmallTest
    public void testRecordHistogramFeedClick_Ntp() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FeedActionDelegate feedActionDelegate = mNtp.getFeedActionDelegateForTesting();

                    // Test click on Feeds or long press then check about this source & topic on
                    // Feeds.
                    HistogramWatcher histogramWatcher = expectFeedRecordForNtpModuleClick();
                    feedActionDelegate.openSuggestionUrl(
                            WindowOpenDisposition.CURRENT_TAB,
                            new LoadUrlParams(TEST_URL, PageTransition.AUTO_BOOKMARK),
                            false,
                            0,
                            mPageLoadObserver,
                            mOnVisitComplete);
                    histogramWatcher.assertExpected(
                            HISTOGRAM_NTP_MODULE_CLICK
                                    + " is not recorded correctly when click on Feeds or long press"
                                    + " then check about this source & topic on Feeds.");

                    // Test long press then open in new tab on Feeds.
                    histogramWatcher = expectFeedRecordForNtpModuleClick();
                    feedActionDelegate.openSuggestionUrl(
                            WindowOpenDisposition.NEW_BACKGROUND_TAB,
                            new LoadUrlParams(TEST_URL, PageTransition.AUTO_BOOKMARK),
                            false,
                            0,
                            mPageLoadObserver,
                            mOnVisitComplete);
                    histogramWatcher.assertExpected(
                            HISTOGRAM_NTP_MODULE_CLICK
                                    + " is not recorded correctly when long press then open in new"
                                    + " tab on Feeds.");

                    // Test long press then open in incognito tab on Feeds.
                    histogramWatcher = expectFeedRecordForNtpModuleClick();
                    feedActionDelegate.openSuggestionUrl(
                            WindowOpenDisposition.OFF_THE_RECORD,
                            new LoadUrlParams(TEST_URL, PageTransition.AUTO_BOOKMARK),
                            false,
                            0,
                            mPageLoadObserver,
                            mOnVisitComplete);
                    histogramWatcher.assertExpected(
                            HISTOGRAM_NTP_MODULE_CLICK
                                    + " is not recorded correctly when long press then open in"
                                    + " incognito tab on  Feeds.");

                    // Test manage activity or manage interests on Feeds.
                    histogramWatcher = expectNoRecordsForNtpModuleClick();
                    feedActionDelegate.openUrl(
                            WindowOpenDisposition.CURRENT_TAB,
                            new LoadUrlParams(TEST_URL, PageTransition.LINK));
                    histogramWatcher.assertExpected(
                            HISTOGRAM_NTP_MODULE_CLICK
                                    + " shouldn't be recorded when manage activity or manage"
                                    + " interests on Feeds.");

                    // Test click Learn More button on Feeds.
                    histogramWatcher = expectFeedRecordForNtpModuleClick();
                    feedActionDelegate.openHelpPage();
                    histogramWatcher.assertExpected(
                            HISTOGRAM_NTP_MODULE_CLICK
                                    + " is not recorded correctly when click Learn More button on"
                                    + " Feeds.");
                });
    }

    /**
     * Test whether the clicking action on the home button in {@link NewTabPage} is been recorded in
     * histogram correctly.
     */
    @Test
    @SmallTest
    @DisableIf.Build(sdk_equals = Build.VERSION_CODES.S_V2, message = "crbug.com/40901674")
    public void testRecordHistogramHomeButtonClick_Ntp() {
        HistogramWatcher histogramWatcher = expectHomeButtonRecordForNtpModuleClick();
        onView(withId(R.id.home_button)).perform(click());
        histogramWatcher.assertExpected(
                HISTOGRAM_NTP_MODULE_CLICK
                        + " is not recorded correctly when click on the home button.");

        histogramWatcher = expectHomeButtonRecordForNtpModuleLongClick();
        onView(withId(R.id.home_button)).perform(longClick());
        onView(withText(R.string.options_homepage_edit_title)).perform(click());
        histogramWatcher.assertExpected(
                HISTOGRAM_NTP_MODULE_LONGCLICK
                        + " is not recorded correctly when we perform long click on the home button"
                        + " and navigate to home page setting.");
    }

    /**
     * Test whether the clicking action on the profile button in {@link NewTabPage} is been recorded
     * in histogram correctly.
     */
    @Test
    @SmallTest
    public void testRecordHistogramProfileButtonClick_Ntp() {
        // Identity Disc should be shown on sign-in state.
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM_NTP_MODULE_CLICK, ModuleTypeOnStartAndNtp.PROFILE_BUTTON);
        onView(withId(R.id.optional_toolbar_button)).perform(click());
        histogramWatcher.assertExpected(
                HISTOGRAM_NTP_MODULE_CLICK
                        + " is not recorded correctly when click on the profile button.");
    }

    /**
     * Test whether the clicking action on Logo in {@link NewTabPage} is been recorded in histogram
     * correctly.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage"})
    public void testRecordHistogramLogoClick_Ntp() {
        LogoBridgeJni.setInstanceForTesting(mLogoBridgeJniMock);
        NewTabPageLayout ntpLayout = mNtp.getNewTabPageLayout();
        LogoCoordinator logoCoordinator = ntpLayout.getLogoCoordinatorForTesting();
        logoCoordinator.setLogoBridgeForTesting(mLogoBridge);
        logoCoordinator.setOnLogoClickUrlForTesting(TEST_URL);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM_NTP_MODULE_CLICK, ModuleTypeOnStartAndNtp.DOODLE);
        ThreadUtils.runOnUiThreadBlocking(() -> logoCoordinator.onLogoClickedForTesting(true));
        histogramWatcher.assertExpected(
                HISTOGRAM_NTP_MODULE_CLICK
                        + " is not recorded correctly when click on Logo with doodle enabled.");
    }

    /**
     * Test whether the clicking action on the menu button in {@link NewTabPage} is been recorded in
     * histogram correctly.
     */
    @Test
    @SmallTest
    public void testRecordHistogramMenuButtonClick_Ntp() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM_NTP_MODULE_CLICK, ModuleTypeOnStartAndNtp.MENU_BUTTON);
        onView(withId(R.id.menu_button_wrapper)).perform(click());
        histogramWatcher.assertExpected(
                HISTOGRAM_NTP_MODULE_CLICK
                        + " is not recorded correctly when click on the menu button.");
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    public void testMvtContainerOnNtp() {
        verifyMostVisitedTileMargin();

        Resources res = mActivityTestRule.getActivity().getResources();
        NewTabPageLayout ntpLayout = mNtp.getNewTabPageLayout();
        TilesLinearLayout mvTilesLayout = ntpLayout.findViewById(R.id.mv_tiles_layout);

        int expectedTitleTopMargin =
                res.getDimensionPixelSize(R.dimen.tile_view_title_margin_top_modern);
        TileView suggestionsTileElement = mvTilesLayout.getTileAt(0);
        View tileTextContainer = suggestionsTileElement.findViewById(R.id.tile_text_container);
        Assert.assertEquals(
                "The top margin of the tile element's title container is wrong.",
                expectedTitleTopMargin,
                ((MarginLayoutParams) tileTextContainer.getLayoutParams()).topMargin);
    }

    /**
     * Test whether the last touch position in {@link NewTabPage} is been set correctly. This is
     * used for {@link
     * org.chromium.chrome.browser.compositor.layouts.phone.NewBackgroundTabAnimationHostView}.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage"})
    public void testLastTouchPosition() {
        // TODO(crbug.com/415303495): Update test to assert with exact values.
        Point ntpPoint = mNtp.getLastTouchPosition();
        Point defaultPoint = new Point(-1, -1);
        Assert.assertEquals(defaultPoint, ntpPoint);

        Assert.assertNotNull(mMvTilesLayout);
        View mvTile = mMvTilesLayout.getTileAt(0);

        TouchCommon.longPressView(mvTile, 0, 0);
        Assert.assertNotEquals(defaultPoint, ntpPoint);
    }

    @Test
    @SmallTest
    @Feature({"NewTabPage"})
    public void testAiModeButton() {
        NewTabPageLayout ntpLayout = mNtp.getNewTabPageLayout();
        TouchCommon.singleClickView(
                ntpLayout
                        .findViewById(
                                org.chromium.chrome.browser.composeplate.R.id.composeplate_view)
                        .findViewById(R.id.composeplate_button));
    }

    @Test
    @SmallTest
    @Feature({"NewTabPage"})
    @EnableFeatures({OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT})
    public void testAiModeButton_fusebox() {
        if (mActivityTestRule.getActivity().isTablet()) return;

        mActivityTestRule.skipWindowAndTabStateCleanup();

        NewTabPageLayout ntpLayout = mNtp.getNewTabPageLayout();
        TouchCommon.singleClickView(
                ntpLayout
                        .findViewById(
                                org.chromium.chrome.browser.composeplate.R.id.composeplate_view)
                        .findViewById(R.id.composeplate_button));
        mOmnibox.checkFocus(true);
    }

    private void verifyMostVisitedTileMargin() {
        Resources res = mActivityTestRule.getActivity().getResources();
        NewTabPageLayout ntpLayout = mNtp.getNewTabPageLayout();
        View mvTilesContainer =
                ntpLayout.findViewById(org.chromium.chrome.test.R.id.mv_tiles_container);

        int expectedMvtLateralMargin =
                res.getDimensionPixelSize(R.dimen.mvt_container_lateral_margin);
        Assert.assertEquals(
                "The left margin of the most visited tiles container is wrong.",
                expectedMvtLateralMargin,
                ((MarginLayoutParams) mvTilesContainer.getLayoutParams()).leftMargin);
        Assert.assertEquals(
                "The right margin of the most visited tiles container is wrong.",
                expectedMvtLateralMargin,
                ((MarginLayoutParams) mvTilesContainer.getLayoutParams()).rightMargin);
        Assert.assertEquals(
                "The width of the most visited tiles container is wrong.",
                expectedMvtLateralMargin * 2L,
                ntpLayout.getWidth() - mvTilesContainer.getWidth());

        int expectedMvtTopMargin = res.getDimensionPixelSize(R.dimen.mvt_container_top_margin);
        int expectedMvtBottomMargin = 0;
        Assert.assertEquals(
                "The top margin of the most visited tiles container is wrong.",
                expectedMvtTopMargin,
                ((MarginLayoutParams) mvTilesContainer.getLayoutParams()).topMargin,
                1);
        Assert.assertEquals(
                "The bottom margin of the most visited tiles container is wrong.",
                expectedMvtBottomMargin,
                ((MarginLayoutParams) mvTilesContainer.getLayoutParams()).bottomMargin);

        int expectedMvtTopPadding = res.getDimensionPixelSize(R.dimen.mvt_container_top_padding);
        int expectedMvtBottomPadding =
                res.getDimensionPixelSize(R.dimen.mvt_container_bottom_padding);
        Assert.assertEquals(
                "The top padding of the most visited tiles container is wrong.",
                expectedMvtTopPadding,
                mvTilesContainer.getPaddingTop());
        Assert.assertEquals(
                "The bottom padding of the most visited tiles container is wrong.",
                expectedMvtBottomPadding,
                mvTilesContainer.getPaddingBottom());

        Drawable mvTilesContainerBackground = mvTilesContainer.getBackground();
        Assert.assertEquals(
                "The shape of the background of the most visited tiles container is wrong.",
                GradientDrawable.RECTANGLE,
                ((GradientDrawable) mvTilesContainerBackground).getShape());
    }

    private boolean getUrlFocusAnimationsDisabled() {
        return ThreadUtils.runOnUiThreadBlocking(
                new Callable<>() {
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
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            ntp.getNewTabPageLayout().getUrlFocusChangeAnimationPercent(),
                            is(percent));
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

    /** Waits until the top of the fakebox reaches the given position. */
    private void waitForFakeboxTopPosition(final NewTabPage ntp, int position) {
        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(getFakeboxTop(ntp), is(position)));
    }

    private static HistogramWatcher expectMostVisitedTilesRecordForNtpModuleClick() {
        return HistogramWatcher.newSingleRecordWatcher(
                HISTOGRAM_NTP_MODULE_CLICK, ModuleTypeOnStartAndNtp.MOST_VISITED_TILES);
    }

    private static HistogramWatcher expectFeedRecordForNtpModuleClick() {
        return HistogramWatcher.newSingleRecordWatcher(
                HISTOGRAM_NTP_MODULE_CLICK, ModuleTypeOnStartAndNtp.FEED);
    }

    private static HistogramWatcher expectHomeButtonRecordForNtpModuleClick() {
        return HistogramWatcher.newSingleRecordWatcher(
                HISTOGRAM_NTP_MODULE_CLICK, ModuleTypeOnStartAndNtp.HOME_BUTTON);
    }

    private static HistogramWatcher expectHomeButtonRecordForNtpModuleLongClick() {
        return HistogramWatcher.newSingleRecordWatcher(
                HISTOGRAM_NTP_MODULE_LONGCLICK, ModuleTypeOnStartAndNtp.HOME_BUTTON);
    }

    private static HistogramWatcher expectNoRecordsForNtpModuleClick() {
        return HistogramWatcher.newBuilder().expectNoRecords(HISTOGRAM_NTP_MODULE_CLICK).build();
    }
}
