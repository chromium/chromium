// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.app.PendingIntent;
import android.view.KeyEvent;
import android.view.View;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
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
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.locale.LocaleManagerDelegate;
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
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.IntentOrigin;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.SearchType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.content_public.common.ContentUrlConstants;
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

    public @Rule ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();
    // Needed for CT connection cleanup.
    public @Rule CustomTabActivityTestRule mCustomTabActivityTestRule =
            new CustomTabActivityTestRule();
    public @Rule JniMocker mJniMocker = new JniMocker();
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock AutocompleteController.Natives mAutocompleteControllerJniMock;
    private @Mock AutocompleteController mAutocompleteController;
    private @Mock VoiceRecognitionHandler mHandler;

    private TestDelegate mTestDelegate;
    private OmniboxTestUtils mOmnibox;
    private AutocompleteController.OnSuggestionsReceivedListener mOnSuggestionsReceivedListener;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(true).when(mHandler).isVoiceSearchEnabled();

        mJniMocker.mock(AutocompleteControllerJni.TEST_HOOKS, mAutocompleteControllerJniMock);
        doReturn(mAutocompleteController).when(mAutocompleteControllerJniMock).getForProfile(any());

        doAnswer(
                        inv ->
                                mOnSuggestionsReceivedListener =
                                        (AutocompleteController.OnSuggestionsReceivedListener)
                                                inv.getArguments()[0])
                .when(mAutocompleteController)
                .addOnSuggestionsReceivedListener(any());

        doReturn(buildDummyAutocompleteMatch(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL))
                .when(mAutocompleteController)
                .classify(any());

        mTestDelegate = new TestDelegate();
        SearchActivity.setDelegateForTests(mTestDelegate);
    }

    private AutocompleteMatch buildDummyAutocompleteMatch(String url) {
        return AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                .setDisplayText(url)
                .setDescription(url)
                .setUrl(new GURL(url))
                .build();
    }

    private AutocompleteResult buildDummyAutocompleteResult() {
        return AutocompleteResult.fromCache(
                List.of(
                        buildDummyAutocompleteMatch("https://www.google.com"),
                        buildDummyAutocompleteMatch("https://android.com")),
                null);
    }

    @Test
    @SmallTest
    public void testOmniboxSuggestionContainerAppears_defaultRetainOmniboxOnFocus()
            throws Exception {
        testOmniboxSuggestionContainerAppears();
    }

    @Test
    @SmallTest
    public void testOmniboxSuggestionContainerAppears_shouldNotRetainOmniboxOnFocus()
            throws Exception {
        OmniboxFeatures.setShouldRetainOmniboxOnFocusForTesting(Boolean.FALSE);
        testOmniboxSuggestionContainerAppears();
    }

    @Test
    @SmallTest
    public void testOmniboxSuggestionContainerAppears_shouldRetainOmniboxOnFocus()
            throws Exception {
        OmniboxFeatures.setShouldRetainOmniboxOnFocusForTesting(Boolean.TRUE);
        testOmniboxSuggestionContainerAppears();
    }

    private void testOmniboxSuggestionContainerAppears() throws Exception {
        startSearchActivity();

        // Wait for the Activity to fully load.
        mTestDelegate.shouldDelayNativeInitializationCallback.waitForCallback(0);
        mTestDelegate.showSearchEngineDialogIfNeededCallback.waitForCallback(0);
        mTestDelegate.onFinishDeferredInitializationCallback.waitForCallback(0);

        // Focus empty omnibox.  It should force the suggestions to appear.
        mOmnibox.requestFocus();
        verify(mAutocompleteController)
                .startZeroSuggest(
                        eq(""),
                        any(/* DSE URL*/ ),
                        eq(PageClassification.ANDROID_SEARCH_WIDGET_VALUE),
                        eq(""),
                        /* isOnFocusContext= */ eq(false));

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mOnSuggestionsReceivedListener.onSuggestionsReceived(
                                buildDummyAutocompleteResult(), true));
        mOmnibox.checkSuggestionsShown();

        // Type in anything.
        mOmnibox.typeText("text", /* commit= */ false);
        mOmnibox.checkText(Matchers.equalTo("text"), null);

        // Clear omnibox focus. This should always clear uncommitted text and hide suggestions.
        mOmnibox.sendKey(KeyEvent.KEYCODE_ESCAPE);
        mOmnibox.checkText(Matchers.isEmptyString(), null);
        mOmnibox.checkSuggestionsShown(false);

        // Refocusing omnibox should once again force the suggestions to appear.
        mOmnibox.requestFocus();
        verify(mAutocompleteController, times(2))
                .startZeroSuggest(
                        eq(""),
                        any(/* DSE URL*/ ),
                        eq(PageClassification.ANDROID_SEARCH_WIDGET_VALUE),
                        eq(""),
                        /* isOnFocusContext= */ eq(false));

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mOnSuggestionsReceivedListener.onSuggestionsReceived(
                                buildDummyAutocompleteResult(), true));
        mOmnibox.checkSuggestionsShown();
    }

    @Test
    @SmallTest
    public void testStartsBrowserAfterUrlSubmitted_aboutblank() throws Exception {
        verifyUrlLoads(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
    }

    @Test
    @SmallTest
    public void testStartsBrowserAfterUrlSubmitted_chromeUrl() throws Exception {
        doReturn(buildDummyAutocompleteMatch("chrome://flags/"))
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
        CriteriaHelper.pollUiThread(
                () -> {
                    return WarmupManager.getInstance().hasSpareWebContents()
                            || WarmupManager.getInstance()
                                    .hasSpareTab(ProfileManager.getLastUsedRegularProfile());
                });
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
        verify(mAutocompleteController, times(1)).resetSession();

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

        CachedZeroSuggestionsManager.saveToCache(buildDummyAutocompleteResult());

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
    @DisabledTest(message = "crbug.com/346528506")
    public void testTypeBeforeDeferredInitialization() throws Exception {
        // Start the Activity.  It should pause and assume that a promo dialog has appeared.
        mTestDelegate.shouldDelayDeferredInitialization = true;
        startSearchActivity();
        mTestDelegate.shouldDelayNativeInitializationCallback.waitForCallback(0);
        mTestDelegate.showSearchEngineDialogIfNeededCallback.waitForCallback(0);
        Assert.assertNotNull(mTestDelegate.onSearchEngineFinalizedCallback);
        Assert.assertEquals(0, mTestDelegate.onFinishDeferredInitializationCallback.getCallCount());
        // Native initialization is finished, but we don't have a DSE elected yet.
        verify(mAutocompleteController, times(1)).addOnSuggestionsReceivedListener(any());

        // Set some text in the search box, then continue startup.
        mOmnibox.requestFocus();
        // Confirm specifically:
        // - no prefetch,
        // - no zero suggestions fetches,
        // - no typed suggestions fetches.
        verifyNoMoreInteractions(mAutocompleteController);

        ThreadUtils.runOnUiThreadBlocking(mTestDelegate.onSearchEngineFinalizedCallback.bind(true));

        // Let the initialization finish completely.
        Assert.assertEquals(
                1, mTestDelegate.shouldDelayNativeInitializationCallback.getCallCount());
        Assert.assertEquals(1, mTestDelegate.showSearchEngineDialogIfNeededCallback.getCallCount());
        mTestDelegate.onFinishDeferredInitializationCallback.waitForCallback(0);

        // Omnibox suggestions should be requested now.
        verify(mAutocompleteController, times(1))
                .startZeroSuggest(
                        eq(""),
                        any(/* DSE URL */ ),
                        eq(PageClassification.ANDROID_SEARCH_WIDGET_VALUE),
                        any(),
                        /* isOnFocusContext= */ eq(false));
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
                    Assert.assertTrue(urlBar.getText().toString().isEmpty());
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    locationBarCoordinator.clearOmniboxFocus();
                    locationBarCoordinator.onUrlChangedForTesting();
                    Assert.assertTrue(urlBar.getText().toString().isEmpty());
                });
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/346528506")
    public void testupdateAnchorViewLayout() {
        SearchActivity searchActivity = startSearchActivity();
        View anchorView = searchActivity.findViewById(R.id.toolbar);
        var layoutParams = anchorView.getLayoutParams();

        int focusedHeight =
                searchActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.toolbar_height_no_shadow_focused);
        int expectedHeight =
                searchActivity
                                .getResources()
                                .getDimensionPixelSize(R.dimen.toolbar_height_no_shadow)
                        + searchActivity
                                .getResources()
                                .getDimensionPixelSize(R.dimen.toolbar_url_focus_height_increase);
        int expectedBottomPadding = 0;

        Assert.assertEquals(expectedHeight, focusedHeight);
        Assert.assertEquals(expectedHeight, layoutParams.height);
        Assert.assertEquals(expectedBottomPadding, anchorView.getPaddingBottom());
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
            Assert.assertTrue("Intent canceled", false);
        }
        Activity searchActivity =
                instrumentation.waitForMonitorWithTimeout(
                        searchMonitor, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        Assert.assertNotNull("Activity didn't start", searchActivity);
        Assert.assertTrue("Wrong activity started", searchActivity instanceof SearchActivity);
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
        mActivityTestRule.setActivity(cta);
    }

    @SuppressLint("SetTextI18n")
    private void setUrlBarText(final Activity activity, final String url) {
        CriteriaHelper.pollUiThread(
                () -> {
                    UrlBar urlBar = activity.findViewById(R.id.url_bar);
                    try {
                        Criteria.checkThat(
                                "UrlBar not focusable", urlBar.isFocusable(), Matchers.is(true));
                        Criteria.checkThat(
                                "UrlBar does not have focus", urlBar.hasFocus(), Matchers.is(true));
                    } catch (CriteriaNotSatisfiedException ex) {
                        urlBar.requestFocus();
                        throw ex;
                    }
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UrlBar urlBar = activity.findViewById(R.id.url_bar);
                    urlBar.setText(url);
                });
    }
}
