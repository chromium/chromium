// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.app.PendingIntent;
import android.os.Build;
import android.view.KeyEvent;

import androidx.core.content.ContextCompat;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.locale.LocaleManagerDelegate;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteControllerJni;
import org.chromium.chrome.browser.omnibox.suggestions.CachedZeroSuggestionsManager;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.search_engines.SearchEnginePromoType;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.searchwidget.SearchActivity.SearchActivityDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.IntentOrigin;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.ResolutionType;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.SearchType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.edge_to_edge.EdgeToEdgeSystemBarColorHelper;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.url.GURL;

import java.util.List;
import java.util.concurrent.Callable;

/**
 * Tests the {@link SearchActivity}.
 *
 * <p>TODO(dfalcantara): Add tests for: + Performing a search query.
 *
 * <p>+ Performing a search query while the SearchActivity is alive and the default search engine is
 * changed outside the SearchActivity.
 *
 * <p>+ Add microphone tests somehow (vague query + confident query).
 */
@Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO}) // Search widget not supported on auto.
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// TODO(crbug.com/419289558): Re-enable color surface feature flags
@Features.DisableFeatures({
    ChromeFeatureList.ANDROID_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_UPDATE
})
@DisableIf.Build(sdk_equals = Build.VERSION_CODES.UPSIDE_DOWN_CAKE, message = "crbug.com/350393662")
@DoNotBatch(reason = "Test start up behaviors.")
public class SearchActivityTest {
    private static class TestDelegate extends SearchActivityDelegate {
        public final CallbackHelper shouldDelayNativeInitializationCallback = new CallbackHelper();
        public final CallbackHelper showSearchEngineDialogIfNeededCallback = new CallbackHelper();
        public final CallbackHelper onFinishDeferredInitializationCallback = new CallbackHelper();
        public final CallbackHelper onPromoDialogShownCallback = new CallbackHelper();

        public boolean shouldDelayLoadingNative;
        public boolean shouldDelayDeferredInitialization;
        public boolean shouldShowRealSearchDialog;

        public Callback<Boolean> onSearchEngineFinalizedCallback;

        @Override
        boolean shouldDelayNativeInitialization() {
            shouldDelayNativeInitializationCallback.notifyCalled();
            return shouldDelayLoadingNative;
        }

        @Override
        void showSearchEngineDialogIfNeeded(
                Activity activity, Callback<Boolean> onSearchEngineFinalized) {
            onSearchEngineFinalizedCallback = onSearchEngineFinalized;
            showSearchEngineDialogIfNeededCallback.notifyCalled();

            if (shouldShowRealSearchDialog) {
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            LocaleManager.getInstance()
                                    .setDelegateForTest(
                                            new LocaleManagerDelegate() {
                                                @Override
                                                public int getSearchEnginePromoShowType() {
                                                    return SearchEnginePromoType.SHOW_EXISTING;
                                                }

                                                @Override
                                                public List<TemplateUrl>
                                                        getSearchEnginesForPromoDialog(
                                                                int promoType) {
                                                    return TemplateUrlServiceFactory.getForProfile(
                                                                    ProfileManager
                                                                            .getLastUsedRegularProfile())
                                                            .getTemplateUrls();
                                                }
                                            });
                        });
                super.showSearchEngineDialogIfNeeded(activity, onSearchEngineFinalized);
            } else {
                LocaleManager.getInstance()
                        .setDelegateForTest(
                                new LocaleManagerDelegate() {
                                    @Override
                                    public boolean needToCheckForSearchEnginePromo() {
                                        return false;
                                    }
                                });
                if (!shouldDelayDeferredInitialization) onSearchEngineFinalized.onResult(true);
            }
        }

        @Override
        public void onFinishDeferredInitialization() {
            onFinishDeferredInitializationCallback.notifyCalled();
        }
    }

    public @Rule FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();
    // Needed for CT connection cleanup.
    public @Rule CustomTabActivityTestRule mCustomTabActivityTestRule =
            new CustomTabActivityTestRule();
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock AutocompleteController.Natives mAutocompleteControllerJniMock;
    private @Mock AutocompleteController mAutocompleteController;
    private @Mock VoiceRecognitionHandler mHandler;

    private TestDelegate mTestDelegate;
    private OmniboxTestUtils mOmnibox;
    private AutocompleteController.OnSuggestionsReceivedListener mOnSuggestionsReceivedListener;

    @Before
    public void setUp() {
        doReturn(true).when(mHandler).isVoiceSearchEnabled();

        AutocompleteControllerJni.setInstanceForTesting(mAutocompleteControllerJniMock);
        doReturn(mAutocompleteController).when(mAutocompleteControllerJniMock).getForProfile(any());

        doAnswer(
                        inv ->
                                mOnSuggestionsReceivedListener =
                                        (AutocompleteController.OnSuggestionsReceivedListener)
                                                inv.getArguments()[0])
                .when(mAutocompleteController)
                .addOnSuggestionsReceivedListener(any());

        doReturn(buildSimpleAutocompleteMatch(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL))
                .when(mAutocompleteController)
                .classify(any());

        mTestDelegate = new TestDelegate();
        SearchActivity.setDelegateForTests(mTestDelegate);
    }

    @After
    public void tearDown() {
        AutocompleteControllerJni.setInstanceForTesting(null);
        ThreadUtils.runOnUiThreadBlocking(
                ChromeNightModeTestUtils::tearDownNightModeAfterChromeActivityDestroyed);
    }

    private AutocompleteMatch buildSimpleAutocompleteMatch(String url) {
        return AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                .setDisplayText(url)
                .setDescription(url)
                .setUrl(new GURL(url))
                .build();
    }

    private AutocompleteResult buildSimpleAutocompleteResult() {
        return AutocompleteResult.fromCache(
                List.of(
                        buildSimpleAutocompleteMatch("https://www.google.com"),
                        buildSimpleAutocompleteMatch("https://android.com")),
                null);
    }

    @Test
    @SmallTest
    public void testStartsBrowserAfterUrlSubmitted_aboutblank() throws Exception {
        verifyUrlLoads(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
    }

    @Test
    @SmallTest
    public void testStartsBrowserAfterUrlSubmitted_chromeUrl() throws Exception {
        doReturn(buildSimpleAutocompleteMatch("chrome://flags/"))
                .when(mAutocompleteController)
                .classify(any());
        verifyUrlLoads("chrome://flags/");
    }

    private void verifyUrlLoads(final String url) throws Exception {
        startSearchActivity();

        // Wait for the Activity to fully load.
        mTestDelegate.shouldDelayNativeInitializationCallback.waitForCallback(0);
        mTestDelegate.showSearchEngineDialogIfNeededCallback.waitForCallback(0);
        mTestDelegate.onFinishDeferredInitializationCallback.waitForCallback(0);

        // Monitor for ChromeTabbedActivity.
        waitForChromeTabbedActivityToStart(
                () -> {
                    mOmnibox.requestFocus();
                    mOmnibox.typeText(url, true);
                    return null;
                },
                url);
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM,
                        LaunchCauseMetrics.LaunchCause.HOME_SCREEN_WIDGET));
    }

    @Test
    @SmallTest
    public void testVoiceSearchBeforeNativeIsLoaded() throws Exception {
        // Wait for the activity to load, but don't let it load the native library.
        mTestDelegate.shouldDelayLoadingNative = true;
        final SearchActivity searchActivity = startSearchActivity(0, /* isVoiceSearch= */ true);
        final SearchActivityLocationBarLayout locationBar =
                searchActivity.findViewById(R.id.search_location_bar);

        LocationBarCoordinator locationBarCoordinator =
                searchActivity.getLocationBarCoordinatorForTesting();
        locationBarCoordinator.setVoiceRecognitionHandlerForTesting(mHandler);
        locationBar.beginQuery(
                IntentOrigin.SEARCH_WIDGET, SearchType.VOICE, /* optionalText= */ null, null);
        verify(mHandler, times(0))
                .startVoiceRecognition(
                        VoiceRecognitionHandler.VoiceInteractionSource.SEARCH_WIDGET);

        mTestDelegate.shouldDelayNativeInitializationCallback.waitForCallback(0);
        Assert.assertEquals(0, mTestDelegate.showSearchEngineDialogIfNeededCallback.getCallCount());
        Assert.assertEquals(0, mTestDelegate.onFinishDeferredInitializationCallback.getCallCount());

        // Start loading native, then let the activity finish initialization.
        ThreadUtils.runOnUiThreadBlocking(
                () -> searchActivity.startDelayedNativeInitializationForTests());

        Assert.assertEquals(
                1, mTestDelegate.shouldDelayNativeInitializationCallback.getCallCount());
        mTestDelegate.showSearchEngineDialogIfNeededCallback.waitForCallback(0);
        mTestDelegate.onFinishDeferredInitializationCallback.waitForCallback(0);

        verify(mHandler)
                .startVoiceRecognition(
                        VoiceRecognitionHandler.VoiceInteractionSource.SEARCH_WIDGET);
    }

    @Test
    @SmallTest
    public void testTypeBeforeNativeIsLoaded() throws Exception {
        // Wait for the activity to load, but don't let it load the native library.
        mTestDelegate.shouldDelayLoadingNative = true;
        final SearchActivity searchActivity = startSearchActivity();
        mTestDelegate.shouldDelayNativeInitializationCallback.waitForCallback(0);
        Assert.assertEquals(0, mTestDelegate.showSearchEngineDialogIfNeededCallback.getCallCount());
        Assert.assertEquals(0, mTestDelegate.onFinishDeferredInitializationCallback.getCallCount());

        // Set some text in the search box (but don't hit enter).
        mOmnibox.requestFocus();
        mOmnibox.typeText(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL, false);
        verifyNoMoreInteractions(mAutocompleteController);

        // Start loading native, then let the activity finish initialization.
        ThreadUtils.runOnUiThreadBlocking(
                () -> searchActivity.startDelayedNativeInitializationForTests());

        verifyNoMoreInteractions(mAutocompleteController);

        Assert.assertEquals(
                1, mTestDelegate.shouldDelayNativeInitializationCallback.getCallCount());
        mTestDelegate.showSearchEngineDialogIfNeededCallback.waitForCallback(0);
        mTestDelegate.onFinishDeferredInitializationCallback.waitForCallback(0);

        // Suggestions requests are always delayed. Rather than check for the request itself
        // confirm that any prior requests have been canceled.
        verify(mAutocompleteController).resetSession();

        waitForChromeTabbedActivityToStart(
                () -> {
                    mOmnibox.sendKey(KeyEvent.KEYCODE_ENTER);
                    return null;
                },
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
    }

    @Test
    @SmallTest
    public void testEnterUrlBeforeNativeIsLoaded() throws Exception {
        // Wait for the activity to load, but don't let it load the native library.
        mTestDelegate.shouldDelayLoadingNative = true;
        final SearchActivity searchActivity = startSearchActivity();
        mTestDelegate.shouldDelayNativeInitializationCallback.waitForCallback(0);
        Assert.assertEquals(0, mTestDelegate.showSearchEngineDialogIfNeededCallback.getCallCount());
        Assert.assertEquals(0, mTestDelegate.onFinishDeferredInitializationCallback.getCallCount());

        // Submit a URL before native is loaded.  The browser shouldn't start yet.
        mOmnibox.requestFocus();
        mOmnibox.typeText(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL, true);
        verifyNoMoreInteractions(mAutocompleteController);
        Assert.assertEquals(searchActivity, ApplicationStatus.getLastTrackedFocusedActivity());
        Assert.assertFalse(searchActivity.isFinishing());

        waitForChromeTabbedActivityToStart(
                () -> {
                    // Finish initialization.  It should notice the URL is queued up and start the
                    // browser.
                    ThreadUtils.runOnUiThreadBlocking(
                            () -> {
                                searchActivity.startDelayedNativeInitializationForTests();
                            });

                    Assert.assertEquals(
                            1,
                            mTestDelegate.shouldDelayNativeInitializationCallback.getCallCount());
                    mTestDelegate.showSearchEngineDialogIfNeededCallback.waitForCallback(0);
                    mTestDelegate.onFinishDeferredInitializationCallback.waitForCallback(0);
                    return null;
                },
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
    }

    @Test
    @SmallTest
    public void testZeroSuggestBeforeNativeIsLoaded() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LocaleManager.getInstance()
                            .setDelegateForTest(
                                    new LocaleManagerDelegate() {
                                        @Override
                                        public boolean needToCheckForSearchEnginePromo() {
                                            return false;
                                        }
                                    });
                });

        CachedZeroSuggestionsManager.saveToCache(
                PageClassification.ANDROID_SEARCH_WIDGET_VALUE, buildSimpleAutocompleteResult());

        // Wait for the activity to load, but don't let it load the native library.
        mTestDelegate.shouldDelayLoadingNative = true;
        startSearchActivity();

        // Focus on the url bar with not text.
        mOmnibox.requestFocus();
        // Omnibox suggestions should appear now.
        mOmnibox.checkSuggestionsShown();
        verifyNoMoreInteractions(mAutocompleteController);
    }

    @Test
    @SmallTest
    public void testTypeBeforeDeferredInitialization() throws Exception {
        // Start the Activity.  It should pause and assume that a promo dialog has appeared.
        mTestDelegate.shouldDelayDeferredInitialization = true;
        startSearchActivity();
        mTestDelegate.shouldDelayNativeInitializationCallback.waitForCallback(0);
        mTestDelegate.showSearchEngineDialogIfNeededCallback.waitForCallback(0);
        Assert.assertNotNull(mTestDelegate.onSearchEngineFinalizedCallback);
        Assert.assertEquals(0, mTestDelegate.onFinishDeferredInitializationCallback.getCallCount());
        // Native initialization is finished, but we don't have a DSE elected yet.
        verify(mAutocompleteController).addOnSuggestionsReceivedListener(any());

        // Set some text in the search box, then continue startup.
        mOmnibox.requestFocus();

        verify(mAutocompleteController, never()).start(any(), anyInt(), anyBoolean());
        verify(mAutocompleteController, never()).startPrefetch(any(), any());
        verify(mAutocompleteController, never()).startZeroSuggest(any());

        ThreadUtils.runOnUiThreadBlocking(mTestDelegate.onSearchEngineFinalizedCallback.bind(true));

        // Let the initialization finish completely.
        Assert.assertEquals(
                1, mTestDelegate.shouldDelayNativeInitializationCallback.getCallCount());
        Assert.assertEquals(1, mTestDelegate.showSearchEngineDialogIfNeededCallback.getCallCount());
        mTestDelegate.onFinishDeferredInitializationCallback.waitForCallback(0);

        // Omnibox suggestions should be requested now.
        var captor = ArgumentCaptor.forClass(AutocompleteInput.class);
        verify(mAutocompleteController).startZeroSuggest(captor.capture());
        Assert.assertEquals("", captor.getValue().getUserText());
        Assert.assertEquals(
                PageClassification.ANDROID_SEARCH_WIDGET_VALUE,
                captor.getValue().getPageClassification());
    }

    @Test
    @MediumTest
    public void testSetUrl_urlBarTextEmpty() throws Exception {
        final SearchActivity searchActivity = startSearchActivity();
        mTestDelegate.shouldDelayNativeInitializationCallback.waitForCallback(0);
        mTestDelegate.showSearchEngineDialogIfNeededCallback.waitForCallback(0);
        mTestDelegate.onFinishDeferredInitializationCallback.waitForCallback(0);

        LocationBarCoordinator locationBarCoordinator =
                searchActivity.getLocationBarCoordinatorForTesting();
        UrlBar urlBar = searchActivity.findViewById(R.id.url_bar);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    locationBarCoordinator.onUrlChangedForTesting();
                    assertTrue(urlBar.getText().toString().isEmpty());
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    locationBarCoordinator.clearOmniboxFocus();
                    locationBarCoordinator.onUrlChangedForTesting();
                    assertTrue(urlBar.getText().toString().isEmpty());
                });
    }

    @Test
    @MediumTest
    public void testLaunchIncognitoSearchActivity() {
        mActivityTestRule.startOnBlankPage();
        SearchActivity searchActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SearchActivity.class,
                        () -> {
                            SearchActivityClientImpl client =
                                    new SearchActivityClientImpl(
                                            mActivityTestRule.getActivity(), IntentOrigin.HUB);
                            client.requestOmniboxForResult(
                                    client.newIntentBuilder()
                                            .setPageUrl(new GURL(UrlConstants.NTP_NON_NATIVE_URL))
                                            .setIncognito(true)
                                            .setResolutionType(ResolutionType.SEND_TO_CALLER)
                                            .build());
                        });
        assertTrue(searchActivity.getProfileSupplierForTesting().get().isOffTheRecord());
    }

    @Test
    @SmallTest
    public void statusAndNavigationBarColor_lightMode() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> ChromeNightModeTestUtils.setUpNightModeForChromeActivity(false));
        SearchActivity searchActivity = startSearchActivity();
        assertStatusAndNavigationBarColors(
                searchActivity, getExpectedOmniboxBackgroundColor(searchActivity));
    }

    @Test
    @SmallTest
    public void statusAndNavigationBarColor_darkMode() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> ChromeNightModeTestUtils.setUpNightModeForChromeActivity(true));
        SearchActivity searchActivity = startSearchActivity();
        assertStatusAndNavigationBarColors(
                searchActivity, getExpectedOmniboxBackgroundColor(searchActivity));
    }

    @Test
    @SmallTest
    public void statusAndNavigationBarColor_incognito() {
        mActivityTestRule.startOnBlankPage();
        SearchActivity searchActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SearchActivity.class,
                        () -> {
                            SearchActivityClientImpl client =
                                    new SearchActivityClientImpl(
                                            mActivityTestRule.getActivity(), IntentOrigin.HUB);
                            client.requestOmniboxForResult(
                                    client.newIntentBuilder()
                                            .setPageUrl(new GURL(UrlConstants.NTP_NON_NATIVE_URL))
                                            .setIncognito(true)
                                            .setResolutionType(ResolutionType.SEND_TO_CALLER)
                                            .build());
                        });
        assertStatusAndNavigationBarColors(
                searchActivity, searchActivity.getColor(R.color.omnibox_dropdown_bg_incognito));
    }

    private void assertStatusAndNavigationBarColors(
            SearchActivity searchActivity, int expectedColor) {
        EdgeToEdgeSystemBarColorHelper edgeToEdgeSystemBarColorHelper =
                searchActivity.getEdgeToEdgeManager().getEdgeToEdgeSystemBarColorHelper();

        // Assert status bar color.
        if (EdgeToEdgeUtils.isEdgeToEdgeEverywhereEnabled()) {
            assertColorsEqual(expectedColor, edgeToEdgeSystemBarColorHelper.getStatusBarColor());
        } else {
            assertColorsEqual(expectedColor, searchActivity.getWindow().getStatusBarColor());
        }

        // Assert navigation bar color.
        assertColorsEqual(expectedColor, edgeToEdgeSystemBarColorHelper.getNavigationBarColor());
    }

    /**
     * Returns the expected background color for the omnibox in {@code searchActivity}.
     *
     * @param searchActivity The {@link SearchActivity} to use as the context.
     * @return The expected background color for the omnibox in {@code searchActivity}.
     */
    private int getExpectedOmniboxBackgroundColor(SearchActivity searchActivity) {
        return ContextCompat.getColor(searchActivity, R.color.omnibox_suggestion_dropdown_bg);
    }

    private void assertColorsEqual(int expected, int actual) {
        String message =
                String.format("Expected %s but got %s", intToHex(expected), intToHex(actual));
        Assert.assertEquals(message, expected, actual);
    }

    private String intToHex(int color) {
        return String.format("#%06X", (0xFFFFFF & color));
    }

    private SearchActivity startSearchActivity() {
        return startSearchActivity(0, /* isVoiceSearch= */ false);
    }

    private SearchActivity startSearchActivity(int expectedCallCount, boolean isVoiceSearch) {
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        ActivityMonitor searchMonitor =
                new ActivityMonitor(SearchActivity.class.getName(), null, false);
        instrumentation.addMonitor(searchMonitor);

        // The SearchActivity shouldn't have started yet.
        Assert.assertEquals(
                expectedCallCount,
                mTestDelegate.shouldDelayNativeInitializationCallback.getCallCount());
        Assert.assertEquals(
                expectedCallCount,
                mTestDelegate.showSearchEngineDialogIfNeededCallback.getCallCount());
        Assert.assertEquals(
                expectedCallCount,
                mTestDelegate.onFinishDeferredInitializationCallback.getCallCount());

        // Fire the Intent to start up the SearchActivity.
        try {
            SearchWidgetProvider.createIntent(instrumentation.getContext(), isVoiceSearch).send();
        } catch (PendingIntent.CanceledException e) {
            assertTrue("Intent canceled", false);
        }
        Activity searchActivity =
                instrumentation.waitForMonitorWithTimeout(
                        searchMonitor, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        Assert.assertNotNull("Activity didn't start", searchActivity);
        assertTrue("Wrong activity started", searchActivity instanceof SearchActivity);
        instrumentation.removeMonitor(searchMonitor);
        mOmnibox = new OmniboxTestUtils(searchActivity);
        return (SearchActivity) searchActivity;
    }

    private void waitForChromeTabbedActivityToStart(Callable<Void> trigger, String expectedUrl)
            throws Exception {
        final ChromeTabbedActivity cta =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        ChromeTabbedActivity.class,
                        trigger);

        CriteriaHelper.pollUiThread(
                () -> {
                    Tab tab = cta.getActivityTab();
                    Criteria.checkThat(tab, Matchers.notNullValue());
                    Criteria.checkThat(tab.getUrl().getSpec(), Matchers.is(expectedUrl));
                });
        mActivityTestRule.getActivityTestRule().setActivity(cta);
    }
}
