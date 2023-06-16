// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
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
import androidx.test.runner.lifecycle.Stage;

import org.hamcrest.Matchers;
import org.junit.After;
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
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.locale.LocaleManagerDelegate;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteControllerProvider;
import org.chromium.chrome.browser.omnibox.suggestions.CachedZeroSuggestionsManager;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.DefaultSearchEngineDialogHelperUtils;
import org.chromium.chrome.browser.search_engines.DefaultSearchEnginePromoDialog;
import org.chromium.chrome.browser.search_engines.DefaultSearchEnginePromoDialog.DefaultSearchEnginePromoDialogObserver;
import org.chromium.chrome.browser.search_engines.SearchEnginePromoType;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.searchwidget.SearchActivity.SearchActivityDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityConstants;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.MultiActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.test.util.UiDisableIf;
import org.chromium.url.GURL;

import java.util.List;
import java.util.concurrent.Callable;

/**
 * Tests the {@link SearchActivity}.
 *
 * TODO(dfalcantara): Add tests for:
 *                    + Performing a search query.
 *
 *                    + Performing a search query while the SearchActivity is alive and the
 *                      default search engine is changed outside the SearchActivity.
 *
 *                    + Add microphone tests somehow (vague query + confident query).
 */
@Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO}) // Search widget not supported on auto.
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Test start up behaviors.")
public class SearchActivityTest {
    private static final String TEST_PNG_IMAGE_FILE_EXTENSION = ".png";

    private static class TestDelegate
            extends SearchActivityDelegate implements DefaultSearchEnginePromoDialogObserver {
        public final CallbackHelper shouldDelayNativeInitializationCallback = new CallbackHelper();
        public final CallbackHelper showSearchEngineDialogIfNeededCallback = new CallbackHelper();
        public final CallbackHelper onFinishDeferredInitializationCallback = new CallbackHelper();
        public final CallbackHelper onPromoDialogShownCallback = new CallbackHelper();

        public boolean shouldDelayLoadingNative;
        public boolean shouldDelayDeferredInitialization;
        public boolean shouldShowRealSearchDialog;

        public DefaultSearchEnginePromoDialog shownPromoDialog;
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
                TestThreadUtils.runOnUiThreadBlocking(() -> {
                    LocaleManager.getInstance().setDelegateForTest(new LocaleManagerDelegate() {
                        @Override
                        public int getSearchEnginePromoShowType() {
                            return SearchEnginePromoType.SHOW_EXISTING;
                        }

                        @Override
                        public List<TemplateUrl> getSearchEnginesForPromoDialog(int promoType) {
                            return TemplateUrlServiceFactory
                                    .getForProfile(Profile.getLastUsedRegularProfile())
                                    .getTemplateUrls();
                        }
                    });
                });
                super.showSearchEngineDialogIfNeeded(activity, onSearchEngineFinalized);
            } else {
                LocaleManager.getInstance().setDelegateForTest(new LocaleManagerDelegate() {
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

        @Override
        public void onDialogShown(DefaultSearchEnginePromoDialog dialog) {
            shownPromoDialog = dialog;
            onPromoDialogShownCallback.notifyCalled();
        }
    }

    public @Rule MultiActivityTestRule mTestRule = new MultiActivityTestRule();
    public @Rule ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();
    public @Rule JniMocker mJniMocker = new JniMocker();
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock AutocompleteController mAutocompleteController;
    private @Mock VoiceRecognitionHandler mHandler;

    private TestDelegate mTestDelegate;
    private OmniboxTestUtils mOmnibox;
    private AutocompleteController.OnSuggestionsReceivedListener mOnSuggestionsReceivedListener;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(true).when(mHandler).isVoiceSearchEnabled();

        AutocompleteControllerProvider.setControllerForTesting(mAutocompleteController);
        doAnswer(inv
                -> mOnSuggestionsReceivedListener =
                           (AutocompleteController.OnSuggestionsReceivedListener)
                                   inv.getArguments()[0])
                .when(mAutocompleteController)
                .addOnSuggestionsReceivedListener(any());

        doReturn(buildDummyAutocompleteMatch(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL))
                .when(mAutocompleteController)
                .classify(any(), anyBoolean());

        mTestDelegate = new TestDelegate();
        SearchActivity.setDelegateForTests(mTestDelegate);
        DefaultSearchEnginePromoDialog.setObserverForTests2(mTestDelegate);
    }

    @After
    public void tearDown() {
        SearchActivity.setDelegateForTests(null);
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
                List.of(buildDummyAutocompleteMatch("https://www.google.com"),
                        buildDummyAutocompleteMatch("https://android.com")),
                null);
    }

    @Test
    @SmallTest
    public void testOmniboxSuggestionContainerAppears() throws Exception {
        SearchActivity searchActivity = startSearchActivity();

        // Wait for the Activity to fully load.
        mTestDelegate.shouldDelayNativeInitializationCallback.waitForCallback(0);
        mTestDelegate.showSearchEngineDialogIfNeededCallback.waitForCallback(0);
        mTestDelegate.onFinishDeferredInitializationCallback.waitForCallback(0);

        // Type in anything.  It should force the suggestions to appear.
        mOmnibox.requestFocus();
        verify(mAutocompleteController, times(1))
                .startZeroSuggest(eq(""), any(/* DSE URL*/),
                        eq(PageClassification.ANDROID_SEARCH_WIDGET_VALUE), eq(""));

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mOnSuggestionsReceivedListener.onSuggestionsReceived(
                                buildDummyAutocompleteResult(), "inline text", true));
        mOmnibox.checkSuggestionsShown();
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.BACK_GESTURE_REFACTOR})
    public void testBackPressFinishActivity() throws Exception {
        SearchActivity searchActivity = startSearchActivity();

        // Wait for the Activity to fully load.
        mTestDelegate.shouldDelayNativeInitializationCallback.waitForCallback(0);
        mTestDelegate.showSearchEngineDialogIfNeededCallback.waitForCallback(0);
        mTestDelegate.onFinishDeferredInitializationCallback.waitForCallback(0);

        // Type in anything.  It should force the suggestions to appear.
        mOmnibox.requestFocus();
        searchActivity.handleBackKeyPressed();

        ApplicationTestUtils.waitForActivityState(
                "Back press should finish the activity", searchActivity, Stage.DESTROYED);
    }

    /**
     * Same with {@link #testBackPressFinishActivity()}, but with predictive back gesture enabled.
     */
    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.BACK_GESTURE_REFACTOR})
    public void testBackPressFinishActivity_BackRefactored() throws Exception {
        SearchActivity searchActivity = startSearchActivity();

        // Wait for the Activity to fully load.
        mTestDelegate.shouldDelayNativeInitializationCallback.waitForCallback(0);
        mTestDelegate.showSearchEngineDialogIfNeededCallback.waitForCallback(0);
        mTestDelegate.onFinishDeferredInitializationCallback.waitForCallback(0);

        // Type in anything.  It should force the suggestions to appear.
        mOmnibox.requestFocus();
        searchActivity.getOnBackPressedDispatcher().onBackPressed();

        ApplicationTestUtils.waitForActivityState(
                "Back press should finish the activity", searchActivity, Stage.DESTROYED);
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
                .classify(any(), anyBoolean());
        verifyUrlLoads("chrome://flags/");
    }

    private void verifyUrlLoads(final String url) throws Exception {
        SearchActivity searchActivity = startSearchActivity();

        // Wait for the Activity to fully load.
        mTestDelegate.shouldDelayNativeInitializationCallback.waitForCallback(0);
        mTestDelegate.showSearchEngineDialogIfNeededCallback.waitForCallback(0);
        mTestDelegate.onFinishDeferredInitializationCallback.waitForCallback(0);

        // Monitor for ChromeTabbedActivity.
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        waitForChromeTabbedActivityToStart(() -> {
            mOmnibox.requestFocus();
            mOmnibox.typeText(url, true);
            return null;
        }, url);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM,
                        LaunchCauseMetrics.LaunchCause.HOME_SCREEN_WIDGET));
    }

    @Test
    @SmallTest
    public void testVoiceSearchBeforeNativeIsLoaded() throws Exception {
        // Wait for the activity to load, but don't let it load the native library.
        mTestDelegate.shouldDelayLoadingNative = true;
        final SearchActivity searchActivity = startSearchActivity(0, /*isVoiceSearch=*/true);
        final SearchActivityLocationBarLayout locationBar =
                (SearchActivityLocationBarLayout) searchActivity.findViewById(
                        R.id.search_location_bar);

        LocationBarCoordinator locationBarCoordinator =
                searchActivity.getLocationBarCoordinatorForTesting();
        locationBarCoordinator.setVoiceRecognitionHandlerForTesting(mHandler);
        locationBar.beginQuery(SearchType.VOICE, /* optionalText= */ null, mHandler, null);
        verify(mHandler, times(0))
                .startVoiceRecognition(
                        VoiceRecognitionHandler.VoiceInteractionSource.SEARCH_WIDGET);

        mTestDelegate.shouldDelayNativeInitializationCallback.waitForCallback(0);
        Assert.assertEquals(0, mTestDelegate.showSearchEngineDialogIfNeededCallback.getCallCount());
        Assert.assertEquals(0, mTestDelegate.onFinishDeferredInitializationCallback.getCallCount());

        // Start loading native, then let the activity finish initialization.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> searchActivity.startDelayedNativeInitialization());

        Assert.assertEquals(
                1, mTestDelegate.shouldDelayNativeInitializationCallback.getCallCount());
        mTestDelegate.showSearchEngineDialogIfNeededCallback.waitForCallback(0);
        mTestDelegate.onFinishDeferredInitializationCallback.waitForCallback(0);

        verify(mHandler).startVoiceRecognition(
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
        TestThreadUtils.runOnUiThreadBlocking(
                () -> searchActivity.startDelayedNativeInitialization());

        verifyNoMoreInteractions(mAutocompleteController);

        Assert.assertEquals(
                1, mTestDelegate.shouldDelayNativeInitializationCallback.getCallCount());
        mTestDelegate.showSearchEngineDialogIfNeededCallback.waitForCallback(0);
        mTestDelegate.onFinishDeferredInitializationCallback.waitForCallback(0);

        // Suggestions requests are always delayed. Rather than check for the request itself
        // confirm that any prior requests have been canceled.
        verify(mAutocompleteController, times(1)).resetSession();

        waitForChromeTabbedActivityToStart(() -> {
            mOmnibox.sendKey(KeyEvent.KEYCODE_ENTER);
            return null;
        }, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
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

        waitForChromeTabbedActivityToStart(() -> {
            // Finish initialization.  It should notice the URL is queued up and start the
            // browser.
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> { searchActivity.startDelayedNativeInitialization(); });

            Assert.assertEquals(
                    1, mTestDelegate.shouldDelayNativeInitializationCallback.getCallCount());
            mTestDelegate.showSearchEngineDialogIfNeededCallback.waitForCallback(0);
            mTestDelegate.onFinishDeferredInitializationCallback.waitForCallback(0);
            return null;
        }, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
    }

    @Test
    @SmallTest
    public void testZeroSuggestBeforeNativeIsLoaded() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            LocaleManager.getInstance().setDelegateForTest(new LocaleManagerDelegate() {
                @Override
                public boolean needToCheckForSearchEnginePromo() {
                    return false;
                }
            });
        });

        CachedZeroSuggestionsManager.saveToCache(buildDummyAutocompleteResult());

        // Wait for the activity to load, but don't let it load the native library.
        mTestDelegate.shouldDelayLoadingNative = true;
        final SearchActivity searchActivity = startSearchActivity();

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
        final SearchActivity searchActivity = startSearchActivity();
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

        TestThreadUtils.runOnUiThreadBlocking(
                mTestDelegate.onSearchEngineFinalizedCallback.bind(true));

        // Let the initialization finish completely.
        Assert.assertEquals(
                1, mTestDelegate.shouldDelayNativeInitializationCallback.getCallCount());
        Assert.assertEquals(1, mTestDelegate.showSearchEngineDialogIfNeededCallback.getCallCount());
        mTestDelegate.onFinishDeferredInitializationCallback.waitForCallback(0);

        // Omnibox suggestions should be requested now.
        verify(mAutocompleteController, times(1))
                .startZeroSuggest(eq(""), any(/* DSE URL */),
                        eq(PageClassification.ANDROID_SEARCH_WIDGET_VALUE), any());
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/1133547")
    public void testRealPromoDialogInterruption() throws Exception {
        // Start the Activity.  It should pause when the promo dialog appears.
        mTestDelegate.shouldShowRealSearchDialog = true;
        final SearchActivity searchActivity = startSearchActivity();
        mTestDelegate.shouldDelayNativeInitializationCallback.waitForCallback(0);
        mTestDelegate.showSearchEngineDialogIfNeededCallback.waitForCallback(0);
        mTestDelegate.onPromoDialogShownCallback.waitForCallback(0);
        Assert.assertEquals(0, mTestDelegate.onFinishDeferredInitializationCallback.getCallCount());

        // Set some text in the search box, then select the first engine to continue startup.
        setUrlBarText(searchActivity, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        DefaultSearchEngineDialogHelperUtils.clickOnFirstEngine(
                mTestDelegate.shownPromoDialog.findViewById(android.R.id.content));

        // Let the initialization finish completely.
        Assert.assertEquals(
                1, mTestDelegate.shouldDelayNativeInitializationCallback.getCallCount());
        Assert.assertEquals(1, mTestDelegate.showSearchEngineDialogIfNeededCallback.getCallCount());
        mTestDelegate.onFinishDeferredInitializationCallback.waitForCallback(0);

        mOmnibox.checkSuggestionsShown();
        mOmnibox.sendKey(KeyEvent.KEYCODE_ENTER);

        waitForChromeTabbedActivityToStart(() -> {
            mOmnibox.sendKey(KeyEvent.KEYCODE_ENTER);
            return null;
        }, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1440967")
    public void testRealPromoDialogDismissWithoutSelection() throws Exception {
        // Start the Activity.  It should pause when the promo dialog appears.
        mTestDelegate.shouldShowRealSearchDialog = true;
        SearchActivity activity = startSearchActivity();
        mTestDelegate.shouldDelayNativeInitializationCallback.waitForCallback(0);
        mTestDelegate.showSearchEngineDialogIfNeededCallback.waitForCallback(0);
        mTestDelegate.onPromoDialogShownCallback.waitForCallback(0);
        Assert.assertEquals(0, mTestDelegate.onFinishDeferredInitializationCallback.getCallCount());

        // Dismiss the dialog without acting on it.
        TestThreadUtils.runOnUiThreadBlocking(() -> mTestDelegate.shownPromoDialog.dismiss());

        // SearchActivity should realize the failure case and prevent the user from using it.
        CriteriaHelper.pollUiThread(() -> {
            List<Activity> activities = ApplicationStatus.getRunningActivities();
            if (activities.isEmpty()) return;

            Criteria.checkThat(activities, Matchers.hasSize(1));
            Criteria.checkThat(activities.get(0), Matchers.is(activity));
            Criteria.checkThat(activity.isFinishing(), Matchers.is(true));
        });
        Assert.assertEquals(
                1, mTestDelegate.shouldDelayNativeInitializationCallback.getCallCount());
        Assert.assertEquals(1, mTestDelegate.showSearchEngineDialogIfNeededCallback.getCallCount());
        Assert.assertEquals(0, mTestDelegate.onFinishDeferredInitializationCallback.getCallCount());
    }

    @Test
    @SmallTest
    public void testNewIntentDiscardsQuery() {
        final SearchActivity searchActivity = startSearchActivity();
        // Note: we should not need to request focus here.
        mOmnibox.requestFocus();
        mOmnibox.typeText("first query", false);

        // Start the Activity again by firing another copy of the same Intent.
        SearchActivity restartedActivity = startSearchActivity(1, /*isVoiceSearch=*/false);
        Assert.assertEquals(searchActivity, restartedActivity);

        mOmnibox.checkText(Matchers.isEmptyString(), null);
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
        UrlBar urlBar = (UrlBar) searchActivity.findViewById(R.id.url_bar);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            locationBarCoordinator.onUrlChangedForTesting();
            Assert.assertTrue(urlBar.getText().toString().isEmpty());
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            locationBarCoordinator.clearOmniboxFocus();
            locationBarCoordinator.onUrlChangedForTesting();
            Assert.assertTrue(urlBar.getText().toString().isEmpty());
        });
    }

    @Test
    @SmallTest
    public void testSearchTypes_knownValidValues() {
        Assert.assertEquals(SearchType.TEXT,
                SearchActivity.getSearchType(SearchActivityConstants.ACTION_START_TEXT_SEARCH));
        Assert.assertEquals(SearchType.VOICE,
                SearchActivity.getSearchType(SearchActivityConstants.ACTION_START_VOICE_SEARCH));
        Assert.assertEquals(SearchType.LENS,
                SearchActivity.getSearchType(SearchActivityConstants.ACTION_START_LENS_SEARCH));
    }

    @Test
    @SmallTest
    public void testSearchTypes_invalidValuesFallBackToTextSearch() {
        Assert.assertEquals(SearchType.TEXT, SearchActivity.getSearchType("Aaaaaaa"));
        Assert.assertEquals(SearchType.TEXT, SearchActivity.getSearchType(null));
        Assert.assertEquals(SearchType.TEXT,
                SearchActivity.getSearchType(
                        SearchActivityConstants.ACTION_START_VOICE_SEARCH + "x"));
        Assert.assertEquals(SearchType.TEXT,
                SearchActivity.getSearchType(
                        SearchActivityConstants.ACTION_START_LENS_SEARCH + "1"));
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE})
    public void testupdateAnchorViewLayout_NoEffectIfFlagDisabled() {
        SearchActivity searchActivity = startSearchActivity();
        View anchorView = searchActivity.getAnchorViewForTesting();
        var layoutParams = anchorView.getLayoutParams();

        int expectedHeight = searchActivity.getResources().getDimensionPixelSize(
                R.dimen.toolbar_height_no_shadow);
        int expectedBottomPadding = 0;

        Assert.assertEquals(expectedHeight, layoutParams.height);
        Assert.assertEquals(expectedBottomPadding, anchorView.getPaddingBottom());
    }

    @Test
    @SmallTest
    @DisableIf.Device(type = {UiDisableIf.TABLET}) // The active color is only apply to the phone.
    @EnableFeatures({ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
            "enable-features=" + ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE + "<Study",
            "force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:modernize_visual_update_active_color_on_omnibox/false"})
    public void
    testupdateAnchorViewLayout_ActiveColorOff() {
        SearchActivity searchActivity = startSearchActivity();
        View anchorView = searchActivity.getAnchorViewForTesting();
        var layoutParams = anchorView.getLayoutParams();

        int expectedHeight = searchActivity.getResources().getDimensionPixelSize(
                                     R.dimen.toolbar_height_no_shadow)
                + searchActivity.getResources().getDimensionPixelSize(
                        R.dimen.toolbar_url_focus_height_increase_no_active_color);
        int expectedBottomPadding = searchActivity.getResources().getDimensionPixelSize(
                R.dimen.toolbar_url_focus_bottom_padding);

        Assert.assertEquals(expectedHeight, layoutParams.height);
        Assert.assertEquals(expectedBottomPadding, anchorView.getPaddingBottom());
    }

    @Test
    @SmallTest
    @DisableIf.Device(type = {UiDisableIf.TABLET}) // The active color is only apply to the phone.
    @EnableFeatures({ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE})
    @CommandLineFlags.
    Add({"enable-features=" + ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE + "<Study",
            "force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:modernize_visual_update_active_color_on_omnibox/true"})
    public void
    testupdateAnchorViewLayout_ActiveColorOn() {
        SearchActivity searchActivity = startSearchActivity();
        View anchorView = searchActivity.getAnchorViewForTesting();
        var layoutParams = anchorView.getLayoutParams();

        int expectedHeight = searchActivity.getResources().getDimensionPixelSize(
                                     R.dimen.toolbar_height_no_shadow)
                + searchActivity.getResources().getDimensionPixelSize(
                        R.dimen.toolbar_url_focus_height_increase_active_color);
        int expectedBottomPadding = 0;

        Assert.assertEquals(expectedHeight, layoutParams.height);
        Assert.assertEquals(expectedBottomPadding, anchorView.getPaddingBottom());
    }

    private SearchActivity startSearchActivity() {
        return startSearchActivity(0, /*isVoiceSearch=*/false);
    }

    private SearchActivity startSearchActivity(int expectedCallCount, boolean isVoiceSearch) {
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        ActivityMonitor searchMonitor =
                new ActivityMonitor(SearchActivity.class.getName(), null, false);
        instrumentation.addMonitor(searchMonitor);

        // The SearchActivity shouldn't have started yet.
        Assert.assertEquals(expectedCallCount,
                mTestDelegate.shouldDelayNativeInitializationCallback.getCallCount());
        Assert.assertEquals(expectedCallCount,
                mTestDelegate.showSearchEngineDialogIfNeededCallback.getCallCount());
        Assert.assertEquals(expectedCallCount,
                mTestDelegate.onFinishDeferredInitializationCallback.getCallCount());

        // Fire the Intent to start up the SearchActivity.
        try {
            SearchWidgetProvider.createIntent(instrumentation.getContext(), isVoiceSearch).send();
        } catch (PendingIntent.CanceledException e) {
            Assert.assertTrue("Intent canceled", false);
        }
        Activity searchActivity = instrumentation.waitForMonitorWithTimeout(
                searchMonitor, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        Assert.assertNotNull("Activity didn't start", searchActivity);
        Assert.assertTrue("Wrong activity started", searchActivity instanceof SearchActivity);
        instrumentation.removeMonitor(searchMonitor);
        mOmnibox = new OmniboxTestUtils(searchActivity);
        return (SearchActivity) searchActivity;
    }

    private void waitForChromeTabbedActivityToStart(Callable<Void> trigger, String expectedUrl)
            throws Exception {
        final ChromeTabbedActivity cta = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), ChromeTabbedActivity.class, trigger);

        CriteriaHelper.pollUiThread(() -> {
            Tab tab = cta.getActivityTab();
            Criteria.checkThat(tab, Matchers.notNullValue());
            Criteria.checkThat(tab.getUrl().getSpec(), Matchers.is(expectedUrl));
        });
        mActivityTestRule.setActivity(cta);
    }

    @SuppressLint("SetTextI18n")
    private void setUrlBarText(final Activity activity, final String url) {
        CriteriaHelper.pollUiThread(() -> {
            UrlBar urlBar = (UrlBar) activity.findViewById(R.id.url_bar);
            try {
                Criteria.checkThat("UrlBar not focusable", urlBar.isFocusable(), Matchers.is(true));
                Criteria.checkThat(
                        "UrlBar does not have focus", urlBar.hasFocus(), Matchers.is(true));
            } catch (CriteriaNotSatisfiedException ex) {
                urlBar.requestFocus();
                throw ex;
            }
        });
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            UrlBar urlBar = (UrlBar) activity.findViewById(R.id.url_bar);
            urlBar.setText(url);
        });
    }
}
