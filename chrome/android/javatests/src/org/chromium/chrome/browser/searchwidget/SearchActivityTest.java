// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.text.TextUtils;
import android.view.KeyEvent;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.locale.DefaultSearchEngineDialogHelperUtils;
import org.chromium.chrome.browser.locale.DefaultSearchEnginePromoDialog;
import org.chromium.chrome.browser.locale.DefaultSearchEnginePromoDialog.DefaultSearchEnginePromoDialogObserver;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.omnibox.MatchClassificationStyle;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion.MatchClassification;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.searchwidget.SearchActivity.SearchActivityDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.MultiActivityTestRule;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.KeyUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentUrlConstants;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

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
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SearchActivityTest {
    private static final long OMNIBOX_SHOW_TIMEOUT_MS = 5000L;

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
                LocaleManager.setInstanceForTest(new LocaleManager() {
                    @Override
                    public int getSearchEnginePromoShowType() {
                        return SearchEnginePromoType.SHOW_EXISTING;
                    }

                    @Override
                    public List<TemplateUrl> getSearchEnginesForPromoDialog(int promoType) {
                        return TemplateUrlServiceFactory.get().getTemplateUrls();
                    }
                });
                super.showSearchEngineDialogIfNeeded(activity, onSearchEngineFinalized);
            } else {
                LocaleManager.setInstanceForTest(new LocaleManager() {
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

    @Rule
    public MultiActivityTestRule mTestRule = new MultiActivityTestRule();

    private TestDelegate mTestDelegate;

    @Before
    public void setUp() {
        mTestDelegate = new TestDelegate();
        SearchActivity.setDelegateForTests(mTestDelegate);
        DefaultSearchEnginePromoDialog.setObserverForTests(mTestDelegate);
    }

    @After
    public void tearDown() {
        SearchActivity.setDelegateForTests(null);
        LocaleManager.setInstanceForTest(null);
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
        setUrlBarText(searchActivity, "anything.");
        final SearchActivityLocationBarLayout locationBar =
                (SearchActivityLocationBarLayout) searchActivity.findViewById(
                        R.id.search_location_bar);
        OmniboxTestUtils.waitForOmniboxSuggestions(locationBar);
    }

    @Test
    @SmallTest
    public void testStartsBrowserAfterUrlSubmitted_aboutblank() throws Exception {
        verifyUrlLoads(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
    }

    @Test
    @SmallTest
    public void testStartsBrowserAfterUrlSubmitted_chromeUrl() throws Exception {
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
        waitForChromeTabbedActivityToStart(new Callable<Void>() {
            @Override
            public Void call() {
                // Type in a URL that should get kicked to ChromeTabbedActivity.
                setUrlBarText(searchActivity, url);
                final UrlBar urlBar = (UrlBar) searchActivity.findViewById(R.id.url_bar);
                KeyUtils.singleKeyEventView(instrumentation, urlBar, KeyEvent.KEYCODE_ENTER);
                return null;
            }
        }, url);
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
        setUrlBarText(searchActivity, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        // Start loading native, then let the activity finish initialization.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> searchActivity.startDelayedNativeInitialization());

        Assert.assertEquals(
                1, mTestDelegate.shouldDelayNativeInitializationCallback.getCallCount());
        mTestDelegate.showSearchEngineDialogIfNeededCallback.waitForCallback(0);
        mTestDelegate.onFinishDeferredInitializationCallback.waitForCallback(0);

        // Omnibox suggestions should appear now.
        final SearchActivityLocationBarLayout locationBar =
                (SearchActivityLocationBarLayout) searchActivity.findViewById(
                        R.id.search_location_bar);
        OmniboxTestUtils.waitForOmniboxSuggestions(locationBar);

        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        waitForChromeTabbedActivityToStart(new Callable<Void>() {
            @Override
            public Void call() {
                // Hitting enter should submit the URL and kick the user to the browser.
                UrlBar urlBar = (UrlBar) searchActivity.findViewById(R.id.url_bar);
                KeyUtils.singleKeyEventView(instrumentation, urlBar, KeyEvent.KEYCODE_ENTER);
                return null;
            }
        }, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
    }

    @Test
    @SmallTest
    @RetryOnFailure(message = "crbug.com/765476")
    public void testEnterUrlBeforeNativeIsLoaded() throws Exception {
        // Wait for the activity to load, but don't let it load the native library.
        mTestDelegate.shouldDelayLoadingNative = true;
        final SearchActivity searchActivity = startSearchActivity();
        mTestDelegate.shouldDelayNativeInitializationCallback.waitForCallback(0);
        Assert.assertEquals(0, mTestDelegate.showSearchEngineDialogIfNeededCallback.getCallCount());
        Assert.assertEquals(0, mTestDelegate.onFinishDeferredInitializationCallback.getCallCount());

        // Submit a URL before native is loaded.  The browser shouldn't start yet.
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        setUrlBarText(searchActivity, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        UrlBar urlBar = (UrlBar) searchActivity.findViewById(R.id.url_bar);
        KeyUtils.singleKeyEventView(instrumentation, urlBar, KeyEvent.KEYCODE_ENTER);
        Assert.assertEquals(searchActivity, ApplicationStatus.getLastTrackedFocusedActivity());
        Assert.assertFalse(searchActivity.isFinishing());

        waitForChromeTabbedActivityToStart(new Callable<Void>() {
            @Override
            public Void call() throws TimeoutException {
                // Finish initialization.  It should notice the URL is queued up and start the
                // browser.
                TestThreadUtils.runOnUiThreadBlocking(
                        () -> { searchActivity.startDelayedNativeInitialization(); });

                Assert.assertEquals(
                        1, mTestDelegate.shouldDelayNativeInitializationCallback.getCallCount());
                mTestDelegate.showSearchEngineDialogIfNeededCallback.waitForCallback(0);
                mTestDelegate.onFinishDeferredInitializationCallback.waitForCallback(0);
                return null;
            }
        }, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
    }

    @Test
    @SmallTest
    public void testZeroSuggestBeforeNativeIsLoaded() {
        LocaleManager.setInstanceForTest(new LocaleManager() {
            @Override
            public boolean needToCheckForSearchEnginePromo() {
                return false;
            }
        });

        // Cache some mock results to show.
        List<MatchClassification> classifications = new ArrayList<>();
        classifications.add(new MatchClassification(0, MatchClassificationStyle.NONE));
        OmniboxSuggestion mockSuggestion = new OmniboxSuggestion(0, true, 0, 0,
                "https://google.com", classifications, "https://google.com", classifications, null,
                "", "https://google.com", null, null, false, false);
        OmniboxSuggestion mockSuggestion2 = new OmniboxSuggestion(0, true, 0, 0,
                "https://android.com", classifications, "https://android.com", classifications,
                null, "", "https://android.com", null, null, false, false);
        List<OmniboxSuggestion> list = new ArrayList<>();
        list.add(mockSuggestion);
        list.add(mockSuggestion2);
        OmniboxSuggestion.cacheOmniboxSuggestionListForZeroSuggest(list);

        // Wait for the activity to load, but don't let it load the native library.
        mTestDelegate.shouldDelayLoadingNative = true;
        final SearchActivity searchActivity = startSearchActivity();

        // Focus on the url bar with not text.

        setUrlBarText(searchActivity, "");
        // Omnibox suggestions should appear now.
        final SearchActivityLocationBarLayout locationBar =
                (SearchActivityLocationBarLayout) searchActivity.findViewById(
                        R.id.search_location_bar);
        OmniboxTestUtils.waitForOmniboxSuggestions(locationBar, OMNIBOX_SHOW_TIMEOUT_MS);
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

        // Set some text in the search box, then continue startup.
        setUrlBarText(searchActivity, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mTestDelegate.onSearchEngineFinalizedCallback.onResult(true));

        // Let the initialization finish completely.
        Assert.assertEquals(
                1, mTestDelegate.shouldDelayNativeInitializationCallback.getCallCount());
        Assert.assertEquals(1, mTestDelegate.showSearchEngineDialogIfNeededCallback.getCallCount());
        mTestDelegate.onFinishDeferredInitializationCallback.waitForCallback(0);

        // Omnibox suggestions should appear now.
        final SearchActivityLocationBarLayout locationBar =
                (SearchActivityLocationBarLayout) searchActivity.findViewById(
                        R.id.search_location_bar);
        OmniboxTestUtils.waitForOmniboxSuggestions(locationBar, OMNIBOX_SHOW_TIMEOUT_MS);

        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        waitForChromeTabbedActivityToStart(new Callable<Void>() {
            @Override
            public Void call() {
                // Hitting enter should submit the URL and kick the user to the browser.
                UrlBar urlBar = (UrlBar) searchActivity.findViewById(R.id.url_bar);
                KeyUtils.singleKeyEventView(instrumentation, urlBar, KeyEvent.KEYCODE_ENTER);
                return null;
            }
        }, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
    }

    @Test
    @SmallTest
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

        // Omnibox suggestions should appear now.
        final SearchActivityLocationBarLayout locationBar =
                (SearchActivityLocationBarLayout) searchActivity.findViewById(
                        R.id.search_location_bar);
        OmniboxTestUtils.waitForOmniboxSuggestions(locationBar);

        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        waitForChromeTabbedActivityToStart(new Callable<Void>() {
            @Override
            public Void call() {
                // Hitting enter should submit the URL and kick the user to the browser.
                UrlBar urlBar = (UrlBar) searchActivity.findViewById(R.id.url_bar);
                KeyUtils.singleKeyEventView(instrumentation, urlBar, KeyEvent.KEYCODE_ENTER);
                return null;
            }
        }, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
    }

    @Test
    @SmallTest
    public void testRealPromoDialogDismissWithoutSelection() throws Exception {
        // Start the Activity.  It should pause when the promo dialog appears.
        mTestDelegate.shouldShowRealSearchDialog = true;
        startSearchActivity();
        mTestDelegate.shouldDelayNativeInitializationCallback.waitForCallback(0);
        mTestDelegate.showSearchEngineDialogIfNeededCallback.waitForCallback(0);
        mTestDelegate.onPromoDialogShownCallback.waitForCallback(0);
        Assert.assertEquals(0, mTestDelegate.onFinishDeferredInitializationCallback.getCallCount());

        // Dismiss the dialog without acting on it.
        mTestDelegate.shownPromoDialog.dismiss();

        // SearchActivity should realize the failure case and prevent the user from using it.
        CriteriaHelper.pollInstrumentationThread(Criteria.equals(0, new Callable<Integer>() {
            @Override
            public Integer call() {
                return ApplicationStatus.getRunningActivities().size();
            }
        }));
        Assert.assertEquals(
                1, mTestDelegate.shouldDelayNativeInitializationCallback.getCallCount());
        Assert.assertEquals(1, mTestDelegate.showSearchEngineDialogIfNeededCallback.getCallCount());
        Assert.assertEquals(0, mTestDelegate.onFinishDeferredInitializationCallback.getCallCount());
    }

    @Test
    @SmallTest
    public void testNewIntentDiscardsQuery() {
        final SearchActivity searchActivity = startSearchActivity();
        setUrlBarText(searchActivity, "first query");
        final SearchActivityLocationBarLayout locationBar =
                (SearchActivityLocationBarLayout) searchActivity.findViewById(
                        R.id.search_location_bar);
        OmniboxTestUtils.waitForOmniboxSuggestions(locationBar, OMNIBOX_SHOW_TIMEOUT_MS);

        // Start the Activity again by firing another copy of the same Intent.
        SearchActivity restartedActivity = startSearchActivity(1);
        Assert.assertEquals(searchActivity, restartedActivity);

        // The query should be wiped.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                UrlBar urlBar = (UrlBar) searchActivity.findViewById(R.id.url_bar);
                return TextUtils.isEmpty(urlBar.getText());
            }
        });
    }

    private SearchActivity startSearchActivity() {
        return startSearchActivity(0);
    }

    private SearchActivity startSearchActivity(int expectedCallCount) {
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
        Intent intent = new Intent();
        SearchWidgetProvider.startSearchActivity(intent, false);
        Activity searchActivity = instrumentation.waitForMonitorWithTimeout(
                searchMonitor, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        Assert.assertNotNull("Activity didn't start", searchActivity);
        Assert.assertTrue("Wrong activity started", searchActivity instanceof SearchActivity);
        instrumentation.removeMonitor(searchMonitor);
        return (SearchActivity) searchActivity;
    }

    private void waitForChromeTabbedActivityToStart(Callable<Void> trigger, String expectedUrl)
            throws Exception {
        final ChromeTabbedActivity cta = ActivityUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), ChromeTabbedActivity.class, trigger);

        CriteriaHelper.pollUiThread(Criteria.equals(expectedUrl, new Callable<String>() {
            @Override
            public String call() {
                Tab tab = cta.getActivityTab();
                if (tab == null) return null;

                return tab.getUrl();
            }
        }));
    }

    @SuppressLint("SetTextI18n")
    private void setUrlBarText(final Activity activity, final String url) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                UrlBar urlBar = (UrlBar) activity.findViewById(R.id.url_bar);
                if (urlBar.isFocusable() && urlBar.hasFocus()) return true;
                urlBar.requestFocus();
                return false;
            }
        });
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            UrlBar urlBar = (UrlBar) activity.findViewById(R.id.url_bar);
            urlBar.setText(url);
        });
    }
}
