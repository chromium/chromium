// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.junit.Assert.assertNotNull;

import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_POLLING_INTERVAL;
import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.graphics.Point;
import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewConfiguration;

import androidx.annotation.IntDef;
import androidx.test.filters.SmallTest;

import com.google.common.collect.ImmutableMap;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureList;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.compositor.bottombar.OverlayContentProgressObserver;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.PanelState;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFakeServer.ContextualSearchTestHost;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFakeServer.FakeResolveSearch;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFakeServer.FakeSlowResolveSearch;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.KeyUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.touch_selection.SelectionEventType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.concurrent.TimeoutException;

/**
 * Tests the Contextual Search Manager using instrumentation tests.
 */
// NOTE: Disable online detection so we we'll default to online on test bots with no network.
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ContextualSearchFieldTrial.ONLINE_DETECTION_DISABLED,
        "disable-features=" + ChromeFeatureList.CONTEXTUAL_SEARCH_ML_TAP_SUPPRESSION + ","
                + ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO + ","
                + ChromeFeatureList.CONTEXTUAL_SEARCH_THIN_WEB_VIEW_IMPLEMENTATION})
@Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
@Batch(Batch.PER_CLASS)
public class ContextualSearchInstrumentationTest {
    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    /**
     * Parameter provider for enabling/disabling triggering-related Features.
     */
    public static class FeatureParamProvider implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            return Arrays.asList(new ParameterSet().value(EnabledFeature.NONE).name("default"),
                    new ParameterSet().value(EnabledFeature.LONGPRESS).name("enableLongpress"),
                    new ParameterSet()
                            .value(EnabledFeature.TRANSLATIONS)
                            .name("enableTranslations"));
        }
    }

    private static final String TAG = "CSITest";
    private static final String TEST_PAGE =
            "/chrome/test/data/android/contextualsearch/simple_test.html";
    private static final String SEARCH_NODE = "search";
    private static final int TEST_TIMEOUT = 15000;
    private static final int TEST_EXPECTED_FAILURE_TIMEOUT = 1000;

    private static final int PANEL_INTERACTION_MAX_RETRIES = 3;
    private static final int PANEL_INTERACTION_RETRY_DELAY_MS = 200;

    // TODO(donnd): get these from TemplateURL once the low-priority or Contextual Search API
    // is fully supported.
    private static final String NORMAL_PRIORITY_SEARCH_ENDPOINT = "/search?";
    private static final String LOW_PRIORITY_SEARCH_ENDPOINT = "/s?";
    private static final String LOW_PRIORITY_INVALID_SEARCH_ENDPOINT = "/s/invalid";
    private static final String CONTEXTUAL_SEARCH_PREFETCH_PARAM = "&pf=c";

    /**
     * Feature maps that we use for parameterized tests.
     */

    /**
     * This represents the current fully-launched configuration.
     */
    private static final ImmutableMap<String, Boolean> ENABLE_NONE =
            ImmutableMap.of(ChromeFeatureList.CONTEXTUAL_SEARCH_LONGPRESS_RESOLVE, false,
                    ChromeFeatureList.CONTEXTUAL_SEARCH_LITERAL_SEARCH_TAP, false,
                    ChromeFeatureList.CONTEXTUAL_SEARCH_TRANSLATIONS, false);
    /**
     * This represents the Longpress with LiteralTap configurations, a good launch candidate.
     */
    private static final ImmutableMap<String, Boolean> ENABLE_LONGPRESS =
            ImmutableMap.of(ChromeFeatureList.CONTEXTUAL_SEARCH_LONGPRESS_RESOLVE, true,
                    ChromeFeatureList.CONTEXTUAL_SEARCH_LITERAL_SEARCH_TAP, true,
                    ChromeFeatureList.CONTEXTUAL_SEARCH_TRANSLATIONS, false);
    /**
     * This represents the Translations addition to the Longpress with LiteralTap configuration.
     */
    private static final ImmutableMap<String, Boolean> ENABLE_TRANSLATIONS =
            ImmutableMap.of(ChromeFeatureList.CONTEXTUAL_SEARCH_LONGPRESS_RESOLVE, false,
                    ChromeFeatureList.CONTEXTUAL_SEARCH_LITERAL_SEARCH_TAP, true,
                    ChromeFeatureList.CONTEXTUAL_SEARCH_TRANSLATIONS, true);

    /**
     * Feature maps that we use for individual tests.
     */
    private static final ImmutableMap<String, Boolean> ENABLE_RELATED_SEARCHES = ImmutableMap.of(
            ChromeFeatureList.RELATED_SEARCHES, true, ChromeFeatureList.RELATED_SEARCHES_UI, false);
    private static final ImmutableMap<String, Boolean> ENABLE_RELATED_SEARCHES_UI = ImmutableMap.of(
            ChromeFeatureList.RELATED_SEARCHES, true, ChromeFeatureList.RELATED_SEARCHES_UI, true);
    private static final ImmutableMap<String, Boolean> ENABLE_FORCE_CAPTION =
            ImmutableMap.of(ChromeFeatureList.CONTEXTUAL_SEARCH_FORCE_CAPTION, true);

    private ActivityMonitor mActivityMonitor;
    private ContextualSearchFakeServer mFakeServer;
    private ContextualSearchManager mManager;
    private ContextualSearchPanel mPanel;
    private ContextualSearchPolicy mPolicy;
    private ContextualSearchSelectionController mSelectionController;
    private EmbeddedTestServer mTestServer;
    private ContextualSearchInstrumentationTestHost mTestHost;

    private float mDpToPx;

    // State for an individual test.
    private FakeSlowResolveSearch mLatestSlowResolveSearch;

    @IntDef({EnabledFeature.NONE, EnabledFeature.LONGPRESS, EnabledFeature.TRANSLATIONS})
    @Retention(RetentionPolicy.SOURCE)
    private @interface EnabledFeature {
        int NONE = 0;
        int LONGPRESS = 1;
        int TRANSLATIONS = 2;
    }

    // Tracks whether a long-press triggering experiment is active.
    private @EnabledFeature int mEnabledFeature;

    @ParameterAnnotations.UseMethodParameterBefore(FeatureParamProvider.class)
    public void setFeatureParameterForTest(@EnabledFeature int enabledFeature) {
        mEnabledFeature = enabledFeature;
    }

    @Before
    public void setUp() throws Exception {
        LocaleManager.setInstanceForTest(new LocaleManager() {
            @Override
            public boolean needToCheckForSearchEnginePromo() {
                return false;
            }
        });

        mTestServer = sActivityTestRule.getTestServer();

        sActivityTestRule.loadUrl(mTestServer.getURL(TEST_PAGE));

        mManager = sActivityTestRule.getActivity().getContextualSearchManager();
        mTestHost = new ContextualSearchInstrumentationTestHost();

        Assert.assertNotNull(mManager);
        mPanel = (ContextualSearchPanel) mManager.getContextualSearchPanel();
        Assert.assertNotNull(mPanel);

        mSelectionController = mManager.getSelectionController();
        mPolicy = mManager.getContextualSearchPolicy();
        mPolicy.overrideDecidedStateForTesting(true);
        mSelectionController.setPolicy(mPolicy);
        resetCounters();

        mFakeServer = new ContextualSearchFakeServer(mPolicy, mTestHost, mManager,
                mManager.getOverlayContentDelegate(), new OverlayContentProgressObserver(),
                sActivityTestRule.getActivity());

        mPanel.setOverlayPanelContentFactory(mFakeServer);
        mManager.setNetworkCommunicator(mFakeServer);
        mPolicy.setNetworkCommunicator(mFakeServer);

        registerFakeSearches();

        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addCategory(Intent.CATEGORY_BROWSABLE);
        filter.addDataScheme("market");
        mActivityMonitor = InstrumentationRegistry.getInstrumentation().addMonitor(
                filter, new Instrumentation.ActivityResult(Activity.RESULT_OK, null), true);

        mDpToPx = sActivityTestRule.getActivity().getResources().getDisplayMetrics().density;

        // Set the test Features map for all tests regardless of whether they are parameterized.
        // Non-parameterized tests typically override this setting by calling setTestFeatures
        // again.
        ImmutableMap<String, Boolean> whichFeature = null;
        switch (mEnabledFeature) {
            case EnabledFeature.NONE:
                whichFeature = ENABLE_NONE;
                break;
            case EnabledFeature.LONGPRESS:
                whichFeature = ENABLE_LONGPRESS;
                break;
            case EnabledFeature.TRANSLATIONS:
                whichFeature = ENABLE_TRANSLATIONS;
                break;
        }
        Assert.assertNotNull(
                "Did you change test Features without setting the correct Map?", whichFeature);
        FeatureList.setTestFeatures(whichFeature);
    }

    @After
    public void tearDown() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mManager.dismissContextualSearchBar();
            mPanel.closePanel(StateChangeReason.UNKNOWN, false);
        });
        InstrumentationRegistry.getInstrumentation().removeMonitor(mActivityMonitor);
        mActivityMonitor = null;
        mLatestSlowResolveSearch = null;
    }

    private class ContextualSearchInstrumentationTestHost implements ContextualSearchTestHost {
        @Override
        public void triggerNonResolve(String nodeId) throws TimeoutException {
            if (mPolicy.isLiteralSearchTapEnabled()) {
                clickWordNode(nodeId);
            } else if (!mPolicy.canResolveLongpress()) {
                longPressNode(nodeId);
            } else {
                Assert.fail(
                        "Cannot trigger a non-resolving gesture with literal tap or non-resolve!");
            }
        }

        @Override
        public void triggerResolve(String nodeId) throws TimeoutException {
            if (mPolicy.canResolveLongpress()) {
                longPressNode(nodeId);
            } else {
                // When tap can trigger a resolve, we use a tap (aka click).
                clickWordNode(nodeId);
            }
        }

        @Override
        public void waitForSelectionToBe(final String text) {
            CriteriaHelper.pollInstrumentationThread(() -> {
                Criteria.checkThat(getSelectedText(), Matchers.is(text));
            }, TEST_TIMEOUT, DEFAULT_POLLING_INTERVAL);
        }

        @Override
        public void waitForSearchTermResolutionToStart(final FakeResolveSearch search) {
            CriteriaHelper.pollInstrumentationThread(
                    ()
                            -> { return search.didStartSearchTermResolution(); },
                    "Fake Search Term Resolution never started.", TEST_TIMEOUT,
                    DEFAULT_POLLING_INTERVAL);
        }

        @Override
        public void waitForSearchTermResolutionToFinish(final FakeResolveSearch search) {
            CriteriaHelper.pollInstrumentationThread(() -> {
                return search.didFinishSearchTermResolution();
            }, "Fake Search was never ready.", TEST_TIMEOUT, DEFAULT_POLLING_INTERVAL);
        }

        @Override
        public ContextualSearchPanel getPanel() {
            return mPanel;
        }
    }

    //============================================================================================
    // Private Helpers
    // TODO(donnd): pare these down. Currently this is a WIP clone of ContextualSearchManagerTest.
    //============================================================================================

    /**
     * Gets the name of the given outcome when it's expected to be logged.
     *
     * @param feature A feature whose name we want.
     * @return The name of the outcome if the give parameter is an outcome, or {@code null} if it's
     * not.
     */
    private static final String expectedOutcomeName(
            @ContextualSearchInteractionRecorder.Feature int feature) {
        switch (feature) {
            // We don't log whether the quick action was clicked unless we actually have a
            // quick action.
            case ContextualSearchInteractionRecorder.Feature.OUTCOME_WAS_QUICK_ACTION_CLICKED:
                return null;
            default:
                return ContextualSearchRankerLoggerImpl.outcomeName(feature);
        }
    }

    /**
     * Gets the name of the given feature when it's expected to be logged.
     *
     * @param feature An outcome that might have been expected to be logged.
     * @return The name of the outcome if it's expected to be logged, or {@code null} if it's not
     * expected to be logged.
     */
    private static final String expectedFeatureName(
            @ContextualSearchInteractionRecorder.Feature int feature) {
        switch (feature) {
            // We don't log previous user impressions and CTR if not available for the
            // current user.
            case ContextualSearchInteractionRecorder.Feature.PREVIOUS_WEEK_CTR_PERCENT:
            case ContextualSearchInteractionRecorder.Feature.PREVIOUS_WEEK_IMPRESSIONS_COUNT:
            case ContextualSearchInteractionRecorder.Feature.PREVIOUS_28DAY_CTR_PERCENT:
            case ContextualSearchInteractionRecorder.Feature.PREVIOUS_28DAY_IMPRESSIONS_COUNT:
                return null;
            default:
                return ContextualSearchRankerLoggerImpl.featureName(feature);
        }
    }

    /**
     * Sets the online status and reloads the current Tab with our test URL.
     *
     * @param isOnline Whether to go online.
     */
    private void setOnlineStatusAndReload(boolean isOnline) {
        mFakeServer.setIsOnline(isOnline);
        final String testUrl = mTestServer.getURL(TEST_PAGE);
        final Tab tab = sActivityTestRule.getActivity().getActivityTab();
        TestThreadUtils.runOnUiThreadBlocking(() -> tab.reload());
        // Make sure the page is fully loaded.
        ChromeTabUtils.waitForTabPageLoaded(tab, testUrl);
    }

    private interface ThrowingRunnable {
        void run() throws TimeoutException;
    }

    // Panel interactions are flaky, see crbug.com/635661. Rather than adding a long delay to
    // each test, we can retry failures. When trying to make the panel peak, we may also have to
    // clear the selection before trying again.
    private void retryPanelBarInteractions(ThrowingRunnable r, boolean clearSelection)
            throws AssertionError, TimeoutException {
        int tries = 0;
        boolean success = false;
        while (!success) {
            tries++;
            try {
                r.run();
                success = true;
            } catch (AssertionError | TimeoutException e) {
                if (tries > PANEL_INTERACTION_MAX_RETRIES) {
                    throw e;
                } else {
                    Log.e(TAG, "Failed to peek panel bar, trying again.", e);
                    if (clearSelection) clearSelection();
                    try {
                        Thread.sleep(PANEL_INTERACTION_RETRY_DELAY_MS);
                    } catch (InterruptedException ex) {
                    }
                }
            }
        }
    }

    private void clearSelection() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            SelectionPopupController.fromWebContents(sActivityTestRule.getWebContents())
                    .clearSelection();
        });
    }

    //============================================================================================
    // Public API
    //============================================================================================

    /**
     * Simulates a long-press on the given node without waiting for the panel to respond.
     *
     * @param nodeId A string containing the node ID.
     */
    public void longPressNodeWithoutWaiting(String nodeId) throws TimeoutException {
        Tab tab = sActivityTestRule.getActivity().getActivityTab();
        DOMUtils.longPressNode(tab.getWebContents(), nodeId);
    }

    /**
     * Simulates a long-press on the given node and waits for the panel to peek.
     *
     * @param nodeId A string containing the node ID.
     */
    public void longPressNode(String nodeId) throws TimeoutException {
        retryPanelBarInteractions(() -> {
            longPressNodeWithoutWaiting(nodeId);
            waitForPanelToPeek();
        }, true);
    }

    /**
     * Simulates a resolving trigger on the given node but does not wait for the panel to peek.
     *
     * @param nodeId A string containing the node ID.
     */
    private void triggerResolve(String nodeId) throws TimeoutException {
        mTestHost.triggerResolve(nodeId);
    }

    /**
     * Simulates a non-resolve trigger on the default node and waits for the panel to peek.
     */
    private void triggerNonResolve() throws TimeoutException {
        mTestHost.triggerNonResolve(SEARCH_NODE);
    }

    /**
     * Asserts that the action bar does or does not become visible in response to a selection.
     * @param visible Whether the Action Bar must become visible or not.
     */
    private void assertWaitForSelectActionBarVisible(final boolean visible) {
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    getSelectionPopupController().isSelectActionBarShowing(), Matchers.is(visible));
        });
    }

    private SelectionPopupController getSelectionPopupController() {
        return SelectionPopupController.fromWebContents(sActivityTestRule.getWebContents());
    }

    /**
     * Long-press a node without completing the action, by keeping the touch down by not letting up.
     *
     * @param nodeId The ID of the node to touch
     * @return A time stamp to use with {@link #longPressExtendSelection}
     * @see #longPressExtendSelection
     */
    public long longPressNodeWithoutUp(String nodeId) throws TimeoutException {
        long downTime = SystemClock.uptimeMillis();
        Tab tab = sActivityTestRule.getActivity().getActivityTab();
        DOMUtils.longPressNodeWithoutUp(tab.getWebContents(), nodeId, downTime);
        waitForSelectActionBarVisible();
        waitForPanelToPeek();
        return downTime;
    }

    /**
     * Extends a Long-press selection by completing a drag action.
     *
     * @param startNodeId The ID of the node that has already been touched
     * @param endNodeId The ID of the node that the touch should be extended to
     * @param downTime A time stamp returned by {@link #longPressNodeWithoutUp}
     * @see #longPressNodeWithoutUp
     */
    public void longPressExtendSelection(String startNodeId, String endNodeId, long downTime)
            throws TimeoutException {
        // TODO(donnd): figure out why we need this one line here, and why the selection does not
        // match our expected nodes!
        longPressNodeWithoutUp("term");

        // Drag to the specified position by a DOM node id.
        int stepCount = 100;
        Tab tab = sActivityTestRule.getActivity().getActivityTab();
        DOMUtils.dragNodeTo(tab.getWebContents(), startNodeId, endNodeId, stepCount, downTime);
        DOMUtils.dragNodeEnd(tab.getWebContents(), endNodeId, downTime);

        // Make sure the selection controller knows we did a drag.
        // TODO(donnd): figure out how to reliably simulate a drag on all platforms.
        float unused = 0.0f;
        @SelectionEventType
        int dragStoppedEvent = SelectionEventType.SELECTION_HANDLE_DRAG_STOPPED;
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mSelectionController.handleSelectionEvent(dragStoppedEvent, unused, unused));

        waitForSelectActionBarVisible();
    }

    /**
     * Simulates a click on the given node.
     *
     * @param nodeId A string containing the node ID.
     */
    public void clickNode(String nodeId) throws TimeoutException {
        Tab tab = sActivityTestRule.getActivity().getActivityTab();
        DOMUtils.clickNode(tab.getWebContents(), nodeId);
    }

    /**
     * Waits for the selected text string to be the given string, and asserts.
     *
     * @param text The string to wait for the selection to become.
     */
    private void waitForSelectionToBe(final String text) {
        mTestHost.waitForSelectionToBe(text);
    }

    /**
     * Waits for the Search Term Resolution to become ready.
     *
     * @param search A given FakeResolveSearch.
     */
    private void waitForSearchTermResolutionToStart(final FakeResolveSearch search) {
        mTestHost.waitForSearchTermResolutionToStart(search);
    }

    /**
     * Waits for the Search Term Resolution to finish.
     *
     * @param search A given FakeResolveSearch.
     */
    private void waitForSearchTermResolutionToFinish(final FakeResolveSearch search) {
        mTestHost.waitForSearchTermResolutionToFinish(search);
    }

    /**
     * Waits for a Normal priority URL to be loaded, or asserts that the load never happened. This
     * is needed when we test with a live internet connection and an invalid url fails to load (as
     * expected.  See crbug.com/682953 for background.
     */
    private void waitForNormalPriorityUrlLoaded() {
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(mFakeServer.getLoadedUrl(), Matchers.notNullValue());
            Criteria.checkThat(mFakeServer.getLoadedUrl(),
                    Matchers.containsString(NORMAL_PRIORITY_SEARCH_ENDPOINT));
        }, TEST_TIMEOUT, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Runs the given Runnable in the main thread.
     *
     * @param runnable The Runnable.
     */
    public void runOnMainSync(Runnable runnable) {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(runnable);
    }

    //============================================================================================
    // Fake Searches Helpers
    //============================================================================================

    /**
     * Simulates a non-resolving search.
     *
     * @param nodeId The id of the node to be triggered.
     */
    private void simulateNonResolveSearch(String nodeId)
            throws InterruptedException, TimeoutException {
        ContextualSearchFakeServer.FakeNonResolveSearch search =
                mFakeServer.getFakeNonResolveSearch(nodeId);
        search.simulate();
        waitForPanelToPeek();
    }

    /**
     * Simulates a resolve-triggering search.
     *
     * @param nodeId The id of the node to be tapped.
     */
    private FakeResolveSearch simulateResolveSearch(String nodeId)
            throws InterruptedException, TimeoutException {
        return simulateResolvableSearchAndAssertResolveAndPreload(nodeId, true);
    }

    /**
     * Simulates a resolve-triggering gesture that may or may not actually resolve. If the gesture
     * should Resolve, the resolve and preload are asserted, and vice versa.
     *
     * @param nodeId The id of the node to be tapped.
     * @param isResolveExpected Whether a resolve is expected or not. Enforce by asserting.
     */
    private FakeResolveSearch simulateResolvableSearchAndAssertResolveAndPreload(String nodeId,
            boolean isResolveExpected) throws InterruptedException, TimeoutException {
        FakeResolveSearch search = mFakeServer.getFakeResolveSearch(nodeId);
        assertNotNull("Could not find FakeResolveSearch for node ID:" + nodeId, search);
        search.simulate();
        waitForPanelToPeek();
        if (isResolveExpected) {
            assertLoadedSearchTermMatches(search.getSearchTerm());
        } else {
            assertSearchTermNotRequested();
            assertNoSearchesLoaded();
            assertNoWebContents();
        }
        return search;
    }

    /**
     * Simulates a resolving search with slow server response.
     *
     * @param nodeId The id of the node to be triggered.
     */
    private void simulateSlowResolveSearch(String nodeId)
            throws InterruptedException, TimeoutException {
        mLatestSlowResolveSearch = mFakeServer.getFakeSlowResolveSearch(nodeId);
        assertNotNull("Could not find FakeSlowResolveSearch for node ID:" + nodeId,
                mLatestSlowResolveSearch);
        mLatestSlowResolveSearch.simulate();
        waitForPanelToPeek();
    }

    /**
     * Simulates a slow response for the most recent {@link FakeSlowResolveSearch} set up by calling
     * simulateSlowResolveSearch.
     */
    private void simulateSlowResolveFinished() throws InterruptedException, TimeoutException {
        // Allow the slow Resolution to finish, waiting for it to complete.
        mLatestSlowResolveSearch.finishResolve();
        assertLoadedSearchTermMatches(mLatestSlowResolveSearch.getSearchTerm());
    }

    /**
     * Registers all fake searches to be used in tests.
     */
    private void registerFakeSearches() {
        mFakeServer.registerFakeSearches();
    }

    //============================================================================================
    // Fake Response
    // TODO(donnd): remove these methods and use the new infrastructure instead.
    //============================================================================================

    /**
     * Posts a fake response on the Main thread.
     */
    private final class FakeResponseOnMainThread implements Runnable {
        private final ResolvedSearchTerm mResolvedSearchTerm;

        public FakeResponseOnMainThread(ResolvedSearchTerm resolvedSearchTerm) {
            mResolvedSearchTerm = resolvedSearchTerm;
        }

        @Override
        public void run() {
            mFakeServer.handleSearchTermResolutionResponse(mResolvedSearchTerm);
        }
    }

    /**
     * Fakes a server response with the parameters given and startAdjust and endAdjust equal to 0.
     * {@See ContextualSearchManager#handleSearchTermResolutionResponse}.
     */
    private void fakeResponse(boolean isNetworkUnavailable, int responseCode, String searchTerm,
            String displayText, String alternateTerm, boolean doPreventPreload) {
        fakeResponse(new ResolvedSearchTerm
                             .Builder(isNetworkUnavailable, responseCode, searchTerm, displayText,
                                     alternateTerm, doPreventPreload)
                             .build());
    }

    /**
     * Fakes a server response with the parameters given. {@See
     * ContextualSearchManager#handleSearchTermResolutionResponse}.
     */
    private void fakeResponse(ResolvedSearchTerm resolvedSearchTerm) {
        if (mFakeServer.getSearchTermRequested() != null) {
            InstrumentationRegistry.getInstrumentation().runOnMainSync(
                    new FakeResponseOnMainThread(resolvedSearchTerm));
        }
    }

    //============================================================================================
    // Content Helpers
    //============================================================================================

    /**
     * @return The Panel's WebContents.
     */
    private WebContents getPanelWebContents() {
        return mPanel.getWebContents();
    }

    /**
     * @return Whether the Panel's WebContents is visible.
     */
    private boolean isWebContentsVisible() {
        return mFakeServer.isContentVisible();
    }

    /**
     * Asserts that the Panel's WebContents is created.
     */
    private void assertWebContentsCreated() {
        Assert.assertNotNull(getPanelWebContents());
    }

    /**
     * Asserts that the Panel's WebContents is not created.
     */
    private void assertNoWebContents() {
        Assert.assertNull(getPanelWebContents());
    }

    /**
     * Asserts that the Panel's WebContents is visible.
     */
    private void assertWebContentsVisible() {
        Assert.assertTrue(isWebContentsVisible());
    }

    /**
     * Asserts that the Panel's WebContents.onShow() method was never called.
     */
    private void assertNeverCalledWebContentsOnShow() {
        Assert.assertFalse(mFakeServer.didEverCallWebContentsOnShow());
    }

    /**
     * Asserts that the Panel's WebContents is created
     */
    private void assertWebContentsCreatedButNeverMadeVisible() {
        assertWebContentsCreated();
        Assert.assertFalse(isWebContentsVisible());
        assertNeverCalledWebContentsOnShow();
    }

    /**
     * Fakes navigation of the Content View to the URL that was previously requested.
     *
     * @param isFailure whether the request resulted in a failure.
     */
    private void fakeContentViewDidNavigate(boolean isFailure) {
        String url = mFakeServer.getLoadedUrl();
        mManager.getOverlayContentDelegate().onMainFrameNavigation(url, false, isFailure, false);
    }

    //============================================================================================
    // Assertions for different states
    //============================================================================================

    void assertPeekingPanelNonResolve() {
        assertLoadedNoUrl();
    }
    void assertClosedPanelNonResolve() {}
    void assertPanelNeverOpened() {
        // Check that we recorded a histogram entry for not-seen.
    }
    void assertPeekingPanelResolve() {
        assertLoadedLowPriorityUrl();
    }
    void assertClosedPanelResolve() {}

    //============================================================================================
    // Assertions for different states
    //============================================================================================

    /**
     * Simulates a click on the given word node. Waits for the bar to peek. TODO(donnd): rename to
     * include the waitForPanelToPeek semantic, or rename clickNode to clickNodeWithoutWaiting.
     *
     * @param nodeId A string containing the node ID.
     */
    private void clickWordNode(String nodeId) throws TimeoutException {
        retryPanelBarInteractions(() -> {
            clickNode(nodeId);
            waitForPanelToPeek();
        }, true);
    }

    /**
     * Simulates a simple gesture that could trigger a resolve on the given node in the given tab.
     *
     * @param tab The tab that contains the node to trigger (must be frontmost).
     * @param nodeId A string containing the node ID.
     */
    public void triggerNode(Tab tab, String nodeId) throws TimeoutException {
        if (mPolicy.canResolveLongpress()) {
            DOMUtils.longPressNode(tab.getWebContents(), nodeId);
        } else {
            DOMUtils.clickNode(tab.getWebContents(), nodeId);
        }
    }

    /**
     * Simulates a key press.
     *
     * @param keycode The key's code.
     */
    private void pressKey(int keycode) {
        KeyUtils.singleKeyEventActivity(InstrumentationRegistry.getInstrumentation(),
                sActivityTestRule.getActivity(), keycode);
    }

    /**
     * Simulates pressing back button.
     */
    private void pressBackButton() {
        pressKey(KeyEvent.KEYCODE_BACK);
    }

    /**
     * @return The selected text.
     */
    private String getSelectedText() {
        return mSelectionController.getSelectedText();
    }

    /**
     * Asserts that the loaded search term matches the provided value.
     *
     * @param searchTerm The provided search term.
     */
    private void assertLoadedSearchTermMatches(String searchTerm) {
        boolean doesMatch = false;
        String loadedUrl = mFakeServer.getLoadedUrl();
        doesMatch = loadedUrl != null && loadedUrl.contains("q=" + searchTerm);
        String message =
                loadedUrl == null ? "but there was no loaded URL!" : "in URL: " + loadedUrl;
        Assert.assertTrue(
                "Expected to find searchTerm '" + searchTerm + "', " + message, doesMatch);
    }

    /**
     * Asserts that the given parameters are present in the most recently loaded URL.
     */
    private void assertContainsParameters(String... terms) {
        Assert.assertNotNull("Fake server didn't load a SERP URL", mFakeServer.getLoadedUrl());
        for (String term : terms) {
            Assert.assertTrue("Expected search term not found:" + term,
                    mFakeServer.getLoadedUrl().contains(term));
        }
    }

    /**
     * Asserts that a Search Term has been requested.
     */
    private void assertSearchTermRequested() {
        Assert.assertNotNull(mFakeServer.getSearchTermRequested());
    }

    /**
     * Asserts that there has not been any Search Term requested.
     */
    private void assertSearchTermNotRequested() {
        Assert.assertNull(mFakeServer.getSearchTermRequested());
    }

    /**
     * Asserts that the panel is currently closed or in an undefined state.
     */
    private void assertPanelClosedOrUndefined() {
        boolean success = false;
        if (mPanel == null) {
            success = true;
        } else {
            @PanelState
            int panelState = mPanel.getPanelState();
            success = panelState == PanelState.CLOSED || panelState == PanelState.UNDEFINED;
        }
        Assert.assertTrue(success);
    }

    /**
     * Asserts that no URL has been loaded in the Overlay Panel.
     */
    private void assertLoadedNoUrl() {
        Assert.assertTrue("Requested a search or preload when none was expected!",
                mFakeServer.getLoadedUrl() == null);
    }

    /**
     * Asserts that a low-priority URL has been loaded in the Overlay Panel.
     */
    private void assertLoadedLowPriorityUrl() {
        String message = "Expected a low priority search request URL, but got "
                + (mFakeServer.getLoadedUrl() != null ? mFakeServer.getLoadedUrl() : "null");
        Assert.assertTrue(message,
                mFakeServer.getLoadedUrl() != null
                        && mFakeServer.getLoadedUrl().contains(LOW_PRIORITY_SEARCH_ENDPOINT));
        Assert.assertTrue("Low priority request does not have the required prefetch parameter!",
                mFakeServer.getLoadedUrl() != null
                        && mFakeServer.getLoadedUrl().contains(CONTEXTUAL_SEARCH_PREFETCH_PARAM));
    }

    /**
     * Asserts that a low-priority URL that is intentionally invalid has been loaded in the Overlay
     * Panel (in order to produce an error).
     */
    private void assertLoadedLowPriorityInvalidUrl() {
        String message = "Expected a low priority invalid search request URL, but got "
                + (String.valueOf(mFakeServer.getLoadedUrl()));
        Assert.assertTrue(message,
                mFakeServer.getLoadedUrl() != null
                        && mFakeServer.getLoadedUrl().contains(
                                LOW_PRIORITY_INVALID_SEARCH_ENDPOINT));
        Assert.assertTrue("Low priority request does not have the required prefetch parameter!",
                mFakeServer.getLoadedUrl() != null
                        && mFakeServer.getLoadedUrl().contains(CONTEXTUAL_SEARCH_PREFETCH_PARAM));
    }

    /**
     * Asserts that a normal priority URL has been loaded in the Overlay Panel.
     */
    private void assertLoadedNormalPriorityUrl() {
        String message = "Expected a normal priority search request URL, but got "
                + (mFakeServer.getLoadedUrl() != null ? mFakeServer.getLoadedUrl() : "null");
        Assert.assertTrue(message,
                mFakeServer.getLoadedUrl() != null
                        && mFakeServer.getLoadedUrl().contains(NORMAL_PRIORITY_SEARCH_ENDPOINT));
        Assert.assertTrue(
                "Normal priority request should not have the prefetch parameter, but did!",
                mFakeServer.getLoadedUrl() != null
                        && !mFakeServer.getLoadedUrl().contains(CONTEXTUAL_SEARCH_PREFETCH_PARAM));
    }

    /**
     * Asserts that no URLs have been loaded in the Overlay Panel since the last {@link
     * ContextualSearchFakeServer#reset}.
     */
    private void assertNoSearchesLoaded() {
        Assert.assertEquals(0, mFakeServer.getLoadedUrlCount());
        assertLoadedNoUrl();
    }

    /**
     * Asserts that a Search Term has been requested.
     *
     * @param isExactResolve Whether the Resolve request must be exact (non-expanding).
     */
    private void assertExactResolve(boolean isExactResolve) {
        Assert.assertEquals(isExactResolve, mFakeServer.getIsExactResolve());
    }

    /**
     * Waits for the Search Panel (the Search Bar) to peek up from the bottom, and asserts that it
     * did peek.
     */
    private void waitForPanelToPeek() {
        waitForPanelToEnterState(PanelState.PEEKED);
    }

    /**
     * Waits for the Search Panel to expand, and asserts that it did expand.
     */
    private void waitForPanelToExpand() {
        waitForPanelToEnterState(PanelState.EXPANDED);
    }

    /**
     * Waits for the Search Panel to maximize, and asserts that it did maximize.
     */
    private void waitForPanelToMaximize() {
        waitForPanelToEnterState(PanelState.MAXIMIZED);
    }

    /**
     * Waits for the Search Panel to close, and asserts that it did close.
     */
    private void waitForPanelToClose() {
        waitForPanelToEnterState(PanelState.CLOSED);
    }

    /**
     * Waits for the Search Panel to enter the given {@code PanelState} and assert.
     *
     * @param state The {@link PanelState} to wait for.
     */
    private void waitForPanelToEnterState(final @PanelState int state) {
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mPanel, Matchers.notNullValue());
            Criteria.checkThat(mPanel.getPanelState(), Matchers.is(state));
            Criteria.checkThat(mPanel.isHeightAnimationRunning(), Matchers.is(false));
        });
    }

    /**
     * Asserts that the panel is still in the given state and continues to stay that way for a
     * while. Waits for a reasonable amount of time for the panel to change to a different state,
     * and verifies that it did not change state while this method is executing. Note that it's
     * quite possible for the panel to transition through some other state and back to the initial
     * state before this method is called without that being detected, because this method only
     * monitors state during its own execution.
     *
     * @param initialState The initial state of the panel at the beginning of an operation that
     *         should
     * not change the panel state.
     */
    private void assertPanelStillInState(final @PanelState int initialState)
            throws InterruptedException {
        boolean didChangeState = false;
        long startTime = SystemClock.uptimeMillis();
        while (!didChangeState
                && SystemClock.uptimeMillis() - startTime < TEST_EXPECTED_FAILURE_TIMEOUT) {
            Thread.sleep(DEFAULT_POLLING_INTERVAL);
            didChangeState = mPanel.getPanelState() != initialState;
        }
        Assert.assertFalse(didChangeState);
    }

    /**
     * Shorthand for a common sequence: 1) Waits for gesture processing, 2) Waits for the panel to
     * close, 3) Asserts that there is no selection and that the panel closed.
     */
    private void waitForGestureToClosePanelAndAssertNoSelection() {
        waitForPanelToClose();
        assertPanelClosedOrUndefined();
        Assert.assertTrue(TextUtils.isEmpty(getSelectedText()));
    }

    /**
     * Waits for the selection to be empty. Use this method any time a test repeatedly establishes
     * and dissolves a selection to ensure that the selection has been completely dissolved before
     * simulating the next selection event. This is needed because the renderer's notification of a
     * selection going away is async, and a subsequent tap may think there's a current selection
     * until it has been dissolved.
     */
    private void waitForSelectionEmpty() {
        CriteriaHelper.pollUiThread(
                () -> mSelectionController.isSelectionEmpty(), "Selection never empty.");
    }

    /**
     * Waits for the panel to close and then waits for the selection to dissolve.
     */
    private void waitForPanelToCloseAndSelectionEmpty() {
        waitForPanelToClose();
        waitForSelectionEmpty();
    }

    private void waitToPreventDoubleTapRecognition() throws InterruptedException {
        // Avoid issues with double-tap detection by ensuring sequential taps
        // aren't treated as such. Double-tapping can also select words much as
        // longpress, in turn showing the pins and preventing contextual tap
        // refinement from nearby taps. The double-tap timeout is sufficiently
        // short that this shouldn't conflict with tap refinement by the user.
        int doubleTapTimeout = ViewConfiguration.getDoubleTapTimeout();
        Thread.sleep(doubleTapTimeout);
    }

    /**
     * Generate a fling sequence from the given start/end X,Y percentages, for the given steps.
     * Works in either landscape or portrait orientation.
     */
    private void fling(float startX, float startY, float endX, float endY, int stepCount) {
        Point size = new Point();
        sActivityTestRule.getActivity().getWindowManager().getDefaultDisplay().getSize(size);
        float dragStartX = size.x * startX;
        float dragEndX = size.x * endX;
        float dragStartY = size.y * startY;
        float dragEndY = size.y * endY;
        long downTime = SystemClock.uptimeMillis();
        TouchCommon.dragStart(sActivityTestRule.getActivity(), dragStartX, dragStartY, downTime);
        TouchCommon.dragTo(sActivityTestRule.getActivity(), dragStartX, dragEndX, dragStartY,
                dragEndY, stepCount, downTime);
        TouchCommon.dragEnd(sActivityTestRule.getActivity(), dragEndX, dragEndY, downTime);
    }

    /**
     * Generate a swipe sequence from the given start/end X,Y percentages, for the given steps.
     * Works in either landscape or portrait orientation.
     */
    private void swipe(float startX, float startY, float endX, float endY, int stepCount) {
        Point size = new Point();
        sActivityTestRule.getActivity().getWindowManager().getDefaultDisplay().getSize(size);
        float dragStartX = size.x * startX;
        float dragEndX = size.x * endX;
        float dragStartY = size.y * startY;
        float dragEndY = size.y * endY;
        int halfCount = stepCount / 2;
        long downTime = SystemClock.uptimeMillis();
        TouchCommon.dragStart(sActivityTestRule.getActivity(), dragStartX, dragStartY, downTime);
        TouchCommon.dragTo(sActivityTestRule.getActivity(), dragStartX, dragEndX, dragStartY,
                dragEndY, halfCount, downTime);
        // Generate events in the stationary end position in order to simulate a "pause" in
        // the movement, therefore preventing this gesture from being interpreted as a fling.
        TouchCommon.dragTo(sActivityTestRule.getActivity(), dragEndX, dragEndX, dragEndY, dragEndY,
                halfCount, downTime);
        TouchCommon.dragEnd(sActivityTestRule.getActivity(), dragEndX, dragEndY, downTime);
    }

    /**
     * Flings the panel up to its expanded state.
     */
    private void flingPanelUp() {
        fling(0.5f, 0.95f, 0.5f, 0.55f, 1000);
    }

    /**
     * Swipes the panel down to its peeked state.
     */
    private void swipePanelDown() {
        swipe(0.5f, 0.55f, 0.5f, 0.95f, 100);
    }

    /**
     * Scrolls the base page.
     */
    private void scrollBasePage() {
        fling(0.f, 0.75f, 0.f, 0.7f, 100);
    }

    /**
     * Taps the base page near the top.
     */
    private void tapBasePageToClosePanel() {
        // TODO(donnd): This is not reliable. Find a better approach.
        // This taps on the panel in an area that will be selected if the "intelligence" node has
        // been tap-selected, and that will cause it to be long-press selected.
        // We use the far right side to prevent simulating a tap on top of an
        // existing long-press selection (the pins are a tap target). This might not work on RTL.
        // We are using y == 0.35f because otherwise it will fail for long press cases.
        // It might be better to get the position of the Panel and tap just about outside
        // the Panel. I suspect some Flaky tests are caused by this problem (ones involving
        // long press and trying to close with the bar peeking, with a long press selection
        // established).
        tapBasePage(0.95f, 0.35f);
        waitForPanelToClose();
    }

    /**
     * Taps the base page at the given x, y position.
     */
    private void tapBasePage(float x, float y) {
        View root = sActivityTestRule.getActivity().getWindow().getDecorView().getRootView();
        x *= root.getWidth();
        y *= root.getHeight();
        TouchCommon.singleClickView(root, (int) x, (int) y);
    }

    /**
     * Click various places to cause the panel to show, expand, then close.
     */
    private void clickToExpandAndClosePanel() throws TimeoutException {
        clickWordNode("states");
        tapBarToExpandAndClosePanel();
        waitForSelectionEmpty();
    }

    /**
     * Tap on the peeking Bar to expand the panel, then close it.
     */
    private void tapBarToExpandAndClosePanel() throws TimeoutException {
        tapPeekingBarToExpandAndAssert();
        closePanel();
    }

    /**
     * Generate a click in the middle of panel's bar. TODO(donnd): Replace this method with
     * panelBarClick since this appears to be unreliable.
     */
    private void clickPanelBar() {
        View root = sActivityTestRule.getActivity().getWindow().getDecorView().getRootView();
        float tapX = ((mPanel.getOffsetX() + mPanel.getWidth()) / 2f) * mDpToPx;
        float tapY = (mPanel.getOffsetY() + (mPanel.getBarContainerHeight() / 2f)) * mDpToPx;

        TouchCommon.singleClickView(root, (int) tapX, (int) tapY);
    }

    /**
     * Taps the peeking bar to expand the panel
     */
    private void tapPeekingBarToExpandAndAssert() throws TimeoutException {
        retryPanelBarInteractions(() -> {
            clickPanelBar();
            waitForPanelToExpand();
        }, false);
    }

    /**
     * Simple sequence useful for checking if a Search Request is prefetched. Resets the fake server
     * and clicks near to cause a search, then closes the panel, which takes us back to the starting
     * state except that the fake server knows if a prefetch occurred.
     */
    private void clickToTriggerPrefetch() throws Exception {
        mFakeServer.reset();
        simulateResolveSearch("search");
        closePanel();
        waitForPanelToCloseAndSelectionEmpty();
    }

    /**
     * Simple sequence to trigger, resolve, and prefetch.  Verifies a prefetch occurred.
     */
    private void triggerToResolveAndAssertPrefetch() throws Exception {
        simulateSlowResolveSearch("states");
        assertLoadedNoUrl();
        assertSearchTermRequested();
        simulateSlowResolveFinished();
    }

    /**
     * Fakes a response to the Resolve request.
     */
    private void fakeAResponse() {
        fakeResponse(false, 200, "states", "United States Intelligence", "alternate-term", false);
        waitForPanelToPeek();
        assertLoadedLowPriorityUrl();
        assertContainsParameters("states", "alternate-term");
    }

    /**
     * Resets all the counters used, by resetting all shared preferences.
     */
    private void resetCounters() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
            boolean freStatus =
                    prefs.getBoolean(ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE, false);
            prefs.edit()
                    .clear()
                    .putBoolean(ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE, freStatus)
                    .apply();
        });
    }

    /**
     * Force the Panel to handle a click on open-in-a-new-tab icon.
     */
    private void forceOpenTabIconClick() {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            mPanel.handleBarClick(mPanel.getOpenTabIconX() + mPanel.getOpenTabIconDimension() / 2,
                    mPanel.getBarHeight() / 2);
        });
    }

    /**
     * Force the Panel to close.
     */
    private void closePanel() {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> { mPanel.closePanel(StateChangeReason.UNKNOWN, false); });
    }

    /**
     * Force the Panel to maximize.
     */
    private void maximizePanel() {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> { mPanel.maximizePanel(StateChangeReason.UNKNOWN); });
    }

    /**
     * Waits for the Action Bar to be visible in response to a selection.
     */
    private void waitForSelectActionBarVisible() {
        assertWaitForSelectActionBarVisible(true);
    }

    /**
     * Gets the Ranker Logger and asserts if we can't.
     **/
    private ContextualSearchRankerLoggerImpl getRankerLogger() {
        ContextualSearchRankerLoggerImpl rankerLogger =
                (ContextualSearchRankerLoggerImpl) mManager.getRankerLogger();
        Assert.assertNotNull(rankerLogger);
        return rankerLogger;
    }

    /**
     * @return The value of the given logged feature, or {@code null} if not logged.
     */
    private Object loggedToRanker(@ContextualSearchInteractionRecorder.Feature int feature) {
        return getRankerLogger().getFeaturesLogged().get(feature);
    }

    /**
     * Asserts that all the expected features have been logged to Ranker.
     **/
    private void assertLoggedAllExpectedFeaturesToRanker() {
        for (int feature = 0; feature < ContextualSearchInteractionRecorder.Feature.NUM_ENTRIES;
                feature++) {
            if (expectedFeatureName(feature) != null) Assert.assertNotNull(loggedToRanker(feature));
        }
    }

    /**
     * Asserts that all the expected outcomes have been logged to Ranker.
     **/
    private void assertLoggedAllExpectedOutcomesToRanker() {
        for (int feature = 0; feature < ContextualSearchInteractionRecorder.Feature.NUM_ENTRIES;
                feature++) {
            if (expectedOutcomeName(feature) != null) {
                Assert.assertNotNull("Expected this outcome to be logged: " + feature,
                        getRankerLogger().getOutcomesLogged().get(feature));
            }
        }
    }

    //============================================================================================
    // Test Cases
    //============================================================================================

    /**
     * Tests a non-resolving gesture that peeks the panel followed by close panel.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testNonResolveGesture(@EnabledFeature int enabledFeature) throws Exception {
        simulateNonResolveSearch(SEARCH_NODE);
        assertPeekingPanelNonResolve();
        closePanel();
        assertClosedPanelNonResolve();
        assertPanelNeverOpened();
    }

    /**
     * Tests a resolving gesture that peeks the panel followed by close panel.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testResolveGesture(@EnabledFeature int enabledFeature) throws Exception {
        simulateResolveSearch(SEARCH_NODE);
        assertPeekingPanelResolve();
        closePanel();
        assertClosedPanelResolve();
        assertPanelNeverOpened();
    }
}
