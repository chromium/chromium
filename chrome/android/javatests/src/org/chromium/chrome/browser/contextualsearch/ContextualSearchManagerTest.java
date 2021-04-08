// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.junit.Assert.assertNotNull;

import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_POLLING_INTERVAL;
import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper.waitForSecondChromeTabbedActivity;
import static org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper.waitForTabs;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.graphics.Point;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewConfiguration;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.test.filters.LargeTest;
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
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity2;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.compositor.bottombar.OverlayContentDelegate;
import org.chromium.chrome.browser.compositor.bottombar.OverlayContentProgressObserver;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.PanelState;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchBarControl;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchImageControl;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchQuickActionControl;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFakeServer.ContextualSearchTestHost;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFakeServer.FakeResolveSearch;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFakeServer.FakeSlowResolveSearch;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchInternalStateController.InternalState;
import org.chromium.chrome.browser.contextualsearch.ResolvedSearchTerm.CardTag;
import org.chromium.chrome.browser.externalnav.ExternalNavigationDelegateImpl;
import org.chromium.chrome.browser.findinpage.FindToolbar;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.gsa.GSAContextDisplaySelection;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.FullscreenTestUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.navigation_interception.NavigationParams;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.KeyUtils;
import org.chromium.content_public.browser.test.util.TestSelectionPopupController;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.test.util.UiDisableIf;
import org.chromium.ui.test.util.UiRestriction;
import org.chromium.ui.touch_selection.SelectionEventType;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeoutException;

// TODO(donnd): Create class with limited API to encapsulate the internals of simulations.
// TODO(donnd): Separate tests into different classes grouped by type of tests. Examples:
// Gestures (Tap, Long-press), Search Term Resolution (resolves, expand selection, prevent preload,
// translation), Panel interaction (tap, fling up/down, close), Content (creation, loading,
// visibility, history, delayed load), Tab Promotion, Policy (add tests to check if policies
// affect the behavior correctly), General (remaining tests), etc.

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
public class ContextualSearchManagerTest {
    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    /** Parameter provider for enabling/disabling triggering-related Features. */
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

    private static final String TAG = "SearchManagerTest";
    private static final String TEST_PAGE =
            "/chrome/test/data/android/contextualsearch/tap_test.html";
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

    /** This represents the current fully-launched configuration. */
    private static final ImmutableMap<String, Boolean> ENABLE_NONE =
            ImmutableMap.of(ChromeFeatureList.CONTEXTUAL_SEARCH_LONGPRESS_RESOLVE, false,
                    ChromeFeatureList.CONTEXTUAL_SEARCH_LITERAL_SEARCH_TAP, false,
                    ChromeFeatureList.CONTEXTUAL_SEARCH_TRANSLATIONS, false);
    /** This represents the Longpress with LiteralTap configurations, a good launch candidate. */
    private static final ImmutableMap<String, Boolean> ENABLE_LONGPRESS =
            ImmutableMap.of(ChromeFeatureList.CONTEXTUAL_SEARCH_LONGPRESS_RESOLVE, true,
                    ChromeFeatureList.CONTEXTUAL_SEARCH_LITERAL_SEARCH_TAP, true,
                    ChromeFeatureList.CONTEXTUAL_SEARCH_TRANSLATIONS, false);
    /** This represents the Translations addition to the Longpress with LiteralTap configuration. */
    private static final ImmutableMap<String, Boolean> ENABLE_TRANSLATIONS =
            ImmutableMap.of(ChromeFeatureList.CONTEXTUAL_SEARCH_LONGPRESS_RESOLVE, false,
                    ChromeFeatureList.CONTEXTUAL_SEARCH_LITERAL_SEARCH_TAP, true,
                    ChromeFeatureList.CONTEXTUAL_SEARCH_TRANSLATIONS, true);

    /** Feature maps that we use for individual tests. */
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
    private ContextualSearchManagerTestHost mTestHost;

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
        mTestHost = new ContextualSearchManagerTestHost();

        Assert.assertNotNull(mManager);
        mPanel = (ContextualSearchPanel) mManager.getContextualSearchPanel();

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

    private class ContextualSearchManagerTestHost implements ContextualSearchTestHost {
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

    /**
     * Gets the name of the given outcome when it's expected to be logged.
     * @param feature A feature whose name we want.
     * @return The name of the outcome if the give parameter is an outcome, or {@code null} if it's
     *         not.
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
     * @param feature An outcome that might have been expected to be logged.
     * @return The name of the outcome if it's expected to be logged, or {@code null} if it's not
     *         expected to be logged.
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
     * @param nodeId A string containing the node ID.
     */
    public void longPressNodeWithoutWaiting(String nodeId) throws TimeoutException {
        Tab tab = sActivityTestRule.getActivity().getActivityTab();
        DOMUtils.longPressNode(tab.getWebContents(), nodeId);
    }

    /**
     * Simulates a long-press on the given node and waits for the panel to peek.
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
     * @param nodeId A string containing the node ID.
     */
    private void triggerResolve(String nodeId) throws TimeoutException {
        mTestHost.triggerResolve(nodeId);
    }

    /**
     * Simulates a non-resolve trigger on the given node and waits for the panel to peek.
     * @param nodeId A string containing the node ID.
     */
    private void triggerNonResolve(String nodeId) throws TimeoutException {
        mTestHost.triggerNonResolve(nodeId);
    }

    /**
     * Long-press a node without completing the action, by keeping the touch down by not letting up.
     * @param nodeId The ID of the node to touch
     * @return A time stamp to use with {@link #longPressExtendSelection}
     * @throws TimeoutException
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
     * @param startNodeId The ID of the node that has already been touched
     * @param endNodeId The ID of the node that the touch should be extended to
     * @param downTime A time stamp returned by {@link #longPressNodeWithoutUp}
     * @throws TimeoutException
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
     * @param nodeId A string containing the node ID.
     */
    public void clickNode(String nodeId) throws TimeoutException {
        Tab tab = sActivityTestRule.getActivity().getActivityTab();
        DOMUtils.clickNode(tab.getWebContents(), nodeId);
    }

    /**
     * Waits for the selected text string to be the given string, and asserts.
     * @param text The string to wait for the selection to become.
     */
    private void waitForSelectionToBe(final String text) {
        mTestHost.waitForSelectionToBe(text);
    }

    /**
     * Waits for the Search Term Resolution to become ready.
     * @param search A given FakeResolveSearch.
     */
    private void waitForSearchTermResolutionToStart(final FakeResolveSearch search) {
        mTestHost.waitForSearchTermResolutionToStart(search);
    }

    /**
     * Waits for the Search Term Resolution to finish.
     * @param search A given FakeResolveSearch.
     */
    private void waitForSearchTermResolutionToFinish(final FakeResolveSearch search) {
        mTestHost.waitForSearchTermResolutionToFinish(search);
    }

    /**
     * Waits for a Normal priority URL to be loaded, or asserts that the load never happened.
     * This is needed when we test with a live internet connection and an invalid url fails to
     * load (as expected.  See crbug.com/682953 for background.
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
     * @throws InterruptedException
     * @throws TimeoutException
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
     * @throws InterruptedException
     * @throws TimeoutException
     */
    private FakeResolveSearch simulateResolveSearch(String nodeId)
            throws InterruptedException, TimeoutException {
        return simulateResolvableSearchAndAssertResolveAndPreload(nodeId, true);
    }

    /**
     * Simulates a resolve-triggering gesture that may or may not actually resolve.
     * If the gesture should Resolve, the resolve and preload are asserted, and vice versa.
     *
     * @param nodeId The id of the node to be tapped.
     * @param isResolveExpected Whether a resolve is expected or not. Enforce by asserting.
     * @throws InterruptedException
     * @throws TimeoutException
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
     * @throws InterruptedException
     * @throws TimeoutException
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
     * Simulates a slow response for the most recent {@link FakeSlowResolveSearch} set up
     * by calling simulateSlowResolveSearch.
     * @throws TimeoutException
     * @throws InterruptedException
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
    private void fakeResponse(boolean isNetworkUnavailable, int responseCode,
            String searchTerm, String displayText, String alternateTerm, boolean doPreventPreload) {
        fakeResponse(new ResolvedSearchTerm
                             .Builder(isNetworkUnavailable, responseCode, searchTerm, displayText,
                                     alternateTerm, doPreventPreload)
                             .build());
    }

    /**
     * Fakes a server response with the parameters given.
     * {@See ContextualSearchManager#handleSearchTermResolutionResponse}.
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
     * @param isFailure whether the request resulted in a failure.
     */
    private void fakeContentViewDidNavigate(boolean isFailure) {
        String url = mFakeServer.getLoadedUrl();
        mManager.getOverlayContentDelegate().onMainFrameNavigation(url, false, isFailure, false);
    }

    /**
     * A SelectionPopupController that has some methods stubbed out for testing.
     */
    private static final class StubbedSelectionPopupController
            extends TestSelectionPopupController {
        private boolean mIsFocusedNodeEditable;

        public StubbedSelectionPopupController() {}

        public void setIsFocusedNodeEditableForTest(boolean isFocusedNodeEditable) {
            mIsFocusedNodeEditable = isFocusedNodeEditable;
        }

        @Override
        public boolean isFocusedNodeEditable() {
            return mIsFocusedNodeEditable;
        }
    }

    //============================================================================================
    // Other Helpers
    // TODO(donnd): organize into sections.
    //============================================================================================

    /**
     * Simulates a click on the given word node.
     * Waits for the bar to peek.
     * TODO(donnd): rename to include the waitForPanelToPeek semantic, or rename clickNode to
     * clickNodeWithoutWaiting.
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
     * @param searchTerm The provided search term.
     */
    private void assertLoadedSearchTermMatches(String searchTerm) {
        boolean doesMatch = false;
        String loadedUrl = mFakeServer.getLoadedUrl();
        doesMatch = loadedUrl != null && loadedUrl.contains("q=" + searchTerm);
        String message = loadedUrl == null ? "but there was no loaded URL!"
                                           : "in URL: " + loadedUrl;
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
     * Asserts that no URLs have been loaded in the Overlay Panel since the last
     * {@link ContextualSearchFakeServer#reset}.
     */
    private void assertNoSearchesLoaded() {
        Assert.assertEquals(0, mFakeServer.getLoadedUrlCount());
        assertLoadedNoUrl();
    }

    /**
     * Asserts that a Search Term has been requested.
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
     * Asserts that the panel is still in the given state and continues to stay that way
     * for a while.
     * Waits for a reasonable amount of time for the panel to change to a different state,
     * and verifies that it did not change state while this method is executing.
     * Note that it's quite possible for the panel to transition through some other state and
     * back to the initial state before this method is called without that being detected,
     * because this method only monitors state during its own execution.
     * @param initialState The initial state of the panel at the beginning of an operation that
     *        should not change the panel state.
     * @throws InterruptedException
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
     * Shorthand for a common sequence:
     * 1) Waits for gesture processing,
     * 2) Waits for the panel to close,
     * 3) Asserts that there is no selection and that the panel closed.
     */
    private void waitForGestureToClosePanelAndAssertNoSelection() {
        waitForPanelToClose();
        assertPanelClosedOrUndefined();
        Assert.assertTrue(TextUtils.isEmpty(getSelectedText()));
    }

    /**
     * Waits for the selection to be empty.
     * Use this method any time a test repeatedly establishes and dissolves a selection to ensure
     * that the selection has been completely dissolved before simulating the next selection event.
     * This is needed because the renderer's notification of a selection going away is async,
     * and a subsequent tap may think there's a current selection until it has been dissolved.
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
     * Generate a click in the middle of panel's bar.
     * TODO(donnd): Replace this method with panelBarClick since this appears to be unreliable.
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
     * Simple sequence useful for checking if a Search Request is prefetched.
     * Resets the fake server and clicks near to cause a search, then closes the panel,
     * which takes us back to the starting state except that the fake server knows
     * if a prefetch occurred.
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

    /** Gets the Ranker Logger and asserts if we can't. **/
    private ContextualSearchRankerLoggerImpl getRankerLogger() {
        ContextualSearchRankerLoggerImpl rankerLogger =
                (ContextualSearchRankerLoggerImpl) mManager.getRankerLogger();
        Assert.assertNotNull(rankerLogger);
        return rankerLogger;
    }

    /** @return The value of the given logged feature, or {@code null} if not logged. */
    private Object loggedToRanker(@ContextualSearchInteractionRecorder.Feature int feature) {
        return getRankerLogger().getFeaturesLogged().get(feature);
    }

    /** Asserts that all the expected features have been logged to Ranker. **/
    private void assertLoggedAllExpectedFeaturesToRanker() {
        for (int feature = 0; feature < ContextualSearchInteractionRecorder.Feature.NUM_ENTRIES;
                feature++) {
            if (expectedFeatureName(feature) != null) Assert.assertNotNull(loggedToRanker(feature));
        }
    }

    /** Asserts that all the expected outcomes have been logged to Ranker. **/
    private void assertLoggedAllExpectedOutcomesToRanker() {
        for (int feature = 0; feature < ContextualSearchInteractionRecorder.Feature.NUM_ENTRIES;
                feature++) {
            if (expectedOutcomeName(feature) != null) {
                Assert.assertNotNull("Expected this outcome to be logged: " + feature,
                        getRankerLogger().getOutcomesLogged().get(feature));
            }
        }
    }

    /**
     * Monitor user action UMA recording operations.
     */
    private static class UserActionMonitor extends UserActionTester {
        // TODO(donnd): merge into UserActionTester. See https://crbug.com/1103757.
        private Set<String> mUserActionPrefixes;
        private Map<String, Integer> mUserActionCounts;

        /** @param userActionPrefixes A set of plain prefix strings for user actions to monitor. */
        UserActionMonitor(Set<String> userActionPrefixes) {
            mUserActionPrefixes = userActionPrefixes;
            mUserActionCounts = new HashMap<String, Integer>();
            for (String action : mUserActionPrefixes) {
                mUserActionCounts.put(action, 0);
            }
        }

        @Override
        public void onActionRecorded(String action) {
            for (String entry : mUserActionPrefixes) {
                if (action.startsWith(entry)) {
                    mUserActionCounts.put(entry, mUserActionCounts.get(entry) + 1);
                }
            }
        }

        /**
         * Gets the count of user actions recorded for the given prefix.
         * @param actionPrefix The plain string prefix to lookup (must match a constructed entry)
         * @return The count of user actions recorded for that prefix.
         */
        int get(String actionPrefix) {
            return mUserActionCounts.get(actionPrefix);
        }
    }

    //============================================================================================
    // Test Cases
    //============================================================================================

    /**
     * Tests whether the contextual search panel hides when omnibox is clicked.
     */
    //@SmallTest
    //@Feature({"ContextualSearch"})
    @Test
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    @FlakyTest(message = "Flaked in 2017.  https://crbug.com/707529")
    public void testHidesWhenOmniboxFocused() throws Exception {
        clickWordNode("intelligence");

        Assert.assertEquals("Intelligence", mFakeServer.getSearchTermRequested());
        fakeResponse(false, 200, "Intelligence", "display-text", "alternate-term", false);
        assertContainsParameters("Intelligence", "alternate-term");
        waitForPanelToPeek();

        OmniboxTestUtils.toggleUrlBarFocus(
                (UrlBar) sActivityTestRule.getActivity().findViewById(R.id.url_bar), true);

        assertPanelClosedOrUndefined();
    }

    /**
     * Tests the doesContainAWord method.
     * TODO(donnd): Change to a unit test.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testDoesContainAWord() {
        Assert.assertTrue(mSelectionController.doesContainAWord("word"));
        Assert.assertTrue(mSelectionController.doesContainAWord("word "));
        Assert.assertFalse("Emtpy string should not be considered a word!",
                mSelectionController.doesContainAWord(""));
        Assert.assertFalse("Special symbols should not be considered a word!",
                mSelectionController.doesContainAWord("@"));
        Assert.assertFalse("White space should not be considered a word",
                mSelectionController.doesContainAWord(" "));
        Assert.assertTrue(mSelectionController.doesContainAWord("Q2"));
        Assert.assertTrue(mSelectionController.doesContainAWord("123"));
    }

    /**
     * Tests the isValidSelection method.
     * TODO(donnd): Change to a unit test.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testIsValidSelection() {
        StubbedSelectionPopupController c = new StubbedSelectionPopupController();
        Assert.assertTrue(mSelectionController.isValidSelection("valid", c));
        Assert.assertFalse(mSelectionController.isValidSelection(" ", c));
        c.setIsFocusedNodeEditableForTest(true);
        Assert.assertFalse(mSelectionController.isValidSelection("editable", c));
        c.setIsFocusedNodeEditableForTest(false);
        String numberString = "0123456789";
        Assert.assertTrue(mSelectionController.isValidSelection(numberString, c));
        StringBuilder longStringBuilder = new StringBuilder().append(numberString);
        for (int i = 0; i < 10; i++) {
            longStringBuilder.append(longStringBuilder.toString());
            if (longStringBuilder.toString().length() < 1000) {
                Assert.assertTrue(
                        mSelectionController.isValidSelection(longStringBuilder.toString(), c));
            } else {
                Assert.assertFalse(
                        mSelectionController.isValidSelection(longStringBuilder.toString(), c));
                break;
            }
        }
    }

    /**
     * Tests Ranker logging for a simple trigger that resolves.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    // Ranker is only used for Tap triggering.
    public void testResolvingSearchRankerLogging() throws Exception {
        FeatureList.setTestFeatures(ENABLE_NONE);

        simulateResolveSearch("intelligence");
        assertLoadedLowPriorityUrl();

        assertLoggedAllExpectedFeaturesToRanker();
        Assert.assertEquals(
                true, loggedToRanker(ContextualSearchInteractionRecorder.Feature.IS_LONG_WORD));
        // The panel must be closed for outcomes to be logged.
        // Close the panel by clicking far away in order to make sure the outcomes get logged by
        // the hideContextualSearchUi call to writeRankerLoggerOutcomesAndReset.
        clickWordNode("states-far");
        waitForPanelToClose();
        assertLoggedAllExpectedOutcomesToRanker();
    }

    /**
     * Tests a simple non-resolving gesture, without opening the panel.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testNonResolveTrigger(@EnabledFeature int enabledFeature) throws Exception {
        triggerNonResolve("states");

        Assert.assertNull(mFakeServer.getSearchTermRequested());
        waitForPanelToPeek();
        assertLoadedNoUrl();
        assertNoWebContents();
    }

    /**
     * Tests swiping the overlay open, after an initial trigger that activates the peeking card.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @DisableIf.Build(supported_abis_includes = "arm64-v8a", message = "crbug.com/765403")
    public void testSwipeExpand() throws Exception {
        // TODO(donnd): enable for all features.
        FeatureList.setTestFeatures(ENABLE_NONE);

        assertNoSearchesLoaded();
        triggerResolve("intelligence");
        assertNoSearchesLoaded();

        // Fake a search term resolution response.
        fakeResponse(false, 200, "Intelligence", "United States Intelligence", "alternate-term",
                false);
        assertContainsParameters("Intelligence", "alternate-term");
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        assertLoadedLowPriorityUrl();

        waitForPanelToPeek();
        flingPanelUp();
        waitForPanelToExpand();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        assertLoadedLowPriorityUrl();
    }

    /**
     * Tests swiping the overlay open, after an initial non-resolve search that activates the
     * peeking card, followed by closing the panel.
     * This test also verifies that we don't create any {@link WebContents} or load any URL
     * until the panel is opened.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testNonResolveSwipeExpand(@EnabledFeature int enabledFeature) throws Exception {
        simulateNonResolveSearch("search");
        assertNoWebContents();
        assertLoadedNoUrl();

        tapPeekingBarToExpandAndAssert();
        assertWebContentsCreated();
        assertLoadedNormalPriorityUrl();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // tap the base page to close.
        closePanel();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        assertNoWebContents();
    }

    /**
     * Tests that only a single low-priority request is issued for a trigger/open sequence.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    @FlakyTest(message = "High priority test.  See https://crbug.com/1058297")
    public void testResolveCausesOneLowPriorityRequest(@EnabledFeature int enabledFeature)
            throws Exception {
        mFakeServer.reset();
        simulateResolveSearch("states");

        // We should not make a second-request until we get a good response from the first-request.
        assertLoadedNoUrl();
        Assert.assertEquals(0, mFakeServer.getLoadedUrlCount());
        fakeResponse(false, 200, "states", "United States Intelligence", "alternate-term", false);
        assertLoadedLowPriorityUrl();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // When the second request succeeds, we should not issue a new request.
        fakeContentViewDidNavigate(false);
        assertLoadedLowPriorityUrl();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // When the bar opens, we should not make any additional request.
        tapPeekingBarToExpandAndAssert();
        assertLoadedLowPriorityUrl();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        assertLoadedLowPriorityUrl();
    }

    /**
     * Tests that a failover for a prefetch request is issued after the panel is opened.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testPrefetchFailoverRequestMadeAfterOpen(@EnabledFeature int enabledFeature)
            throws Exception {
        mFakeServer.reset();
        triggerResolve("states");

        // We should not make a SERP request until we get a good response from the resolve request.
        assertLoadedNoUrl();
        Assert.assertEquals(0, mFakeServer.getLoadedUrlCount());
        fakeResponse(false, 200, "states", "United States Intelligence", "alternate-term", false);
        assertLoadedLowPriorityUrl();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // When the second request fails, we should not automatically issue a new request.
        fakeContentViewDidNavigate(true);
        assertLoadedLowPriorityUrl();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Once the bar opens, we make a new request at normal priority.
        tapPeekingBarToExpandAndAssert();
        assertLoadedNormalPriorityUrl();
        Assert.assertEquals(2, mFakeServer.getLoadedUrlCount());
    }

    /**
     * Tests that a live request that fails (for an invalid URL) does a failover to a
     * normal priority request once the user triggers the failover by opening the panel.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testLivePrefetchFailoverRequestMadeAfterOpen(@EnabledFeature int enabledFeature)
            throws Exception {
        // Test fails with out-of-process network service. crbug.com/1071721
        if (!ChromeFeatureList.isEnabled("NetworkServiceInProcess")) return;

        mFakeServer.reset();
        mFakeServer.setLowPriorityPathInvalid();
        mFakeServer.setActuallyLoadALiveSerp();
        simulateResolveSearch("search");
        assertLoadedLowPriorityInvalidUrl();
        Assert.assertTrue(mFakeServer.didAttemptLoadInvalidUrl());

        // we should not automatically issue a new request.
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Fake a navigation error if offline.
        // When connected to the Internet this error may already have happened due to actually
        // trying to load the invalid URL.  But on test bots that are not online we need to
        // fake that a navigation happened with an error. See crbug.com/682953 for details.
        if (!mManager.isOnline()) {
            boolean isFailure = true;
            fakeContentViewDidNavigate(isFailure);
        }

        // Once the bar opens, we make a new request at normal priority.
        tapPeekingBarToExpandAndAssert();
        waitForNormalPriorityUrlLoaded();
        Assert.assertEquals(2, mFakeServer.getLoadedUrlCount());
    }

    /**
     * Tests a simple triggering getsture with disable-preload set.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testResolveDisablePreload(@EnabledFeature int enabledFeature) throws Exception {
        simulateSlowResolveSearch("intelligence");

        assertSearchTermRequested();
        boolean doPreventPreload = true;
        fakeResponse(
                false, 200, "Intelligence", "display-text", "alternate-term", doPreventPreload);
        assertLoadedNoUrl();
        waitForPanelToPeek();
        assertLoadedNoUrl();
    }

    /**
     * Tests that long-press selects text, and a subsequent tap will unselect text.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testLongPressGestureSelects(@EnabledFeature int enabledFeature) throws Exception {
        longPressNode("intelligence");
        Assert.assertEquals("Intelligence", getSelectedText());
        waitForPanelToPeek();
        assertLoadedNoUrl();  // No load after long-press until opening panel.
        clickNode("question-mark");
        waitForPanelToCloseAndSelectionEmpty();
        Assert.assertTrue(TextUtils.isEmpty(getSelectedText()));
        assertLoadedNoUrl();
    }

    /**
     * Tests that a Resolve gesture selects the expected text.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testResolveGestureSelects(@EnabledFeature int enabledFeature) throws Exception {
        simulateResolveSearch("intelligence");
        Assert.assertEquals("Intelligence", getSelectedText());
        assertLoadedLowPriorityUrl();
        clickNode("question-mark");
        waitForPanelToClose();
        Assert.assertTrue(getSelectedText() == null || getSelectedText().isEmpty());
    }

    //============================================================================================
    // Tap=gesture Tests
    //============================================================================================

    /**
     * Tests that a Tap gesture on a special character does not select or show the panel.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapGestureOnSpecialCharacterDoesntSelect() throws Exception {
        FeatureList.setTestFeatures(ENABLE_NONE);

        clickNode("question-mark");
        Assert.assertNull(getSelectedText());
        assertPanelClosedOrUndefined();
        assertLoadedNoUrl();
    }

    /**
     * Tests that a Tap gesture followed by scrolling clears the selection.
     */
    @Test
    @DisableIf.
    Build(sdk_is_greater_than = Build.VERSION_CODES.LOLLIPOP, message = "crbug.com/841017")
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapGestureFollowedByScrollClearsSelection() throws Exception {
        FeatureList.setTestFeatures(ENABLE_NONE);

        clickWordNode("intelligence");
        fakeResponse(false, 200, "Intelligence", "Intelligence", "alternate-term", false);
        assertContainsParameters("Intelligence", "alternate-term");
        waitForPanelToPeek();
        assertLoadedLowPriorityUrl();
        scrollBasePage();
        assertPanelClosedOrUndefined();
        Assert.assertTrue(TextUtils.isEmpty(mSelectionController.getSelectedText()));
    }

    /**
     * Tests that a Tap gesture followed by tapping an invalid character doesn't select.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapGestureFollowedByInvalidTextTapCloses() throws Exception {
        FeatureList.setTestFeatures(ENABLE_NONE);

        clickWordNode("states-far");
        waitForPanelToPeek();
        clickNode("question-mark");
        waitForPanelToClose();
        Assert.assertNull(mSelectionController.getSelectedText());
    }

    /**
     * Tests that a Tap gesture followed by tapping a non-text character doesn't select.
     * @SmallTest
     * @Feature({"ContextualSearch"})
     * crbug.com/665633
     */
    @Test
    @DisabledTest
    public void testTapGestureFollowedByNonTextTap() throws Exception {
        FeatureList.setTestFeatures(ENABLE_NONE);

        clickWordNode("states-far");
        waitForPanelToPeek();
        clickNode("button");
        waitForPanelToCloseAndSelectionEmpty();
    }

    /**
     * Tests that a Tap gesture far away toggles selecting text.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapGestureFarAwayTogglesSelecting() throws Exception {
        FeatureList.setTestFeatures(ENABLE_NONE);

        clickWordNode("states");
        Assert.assertEquals("States", getSelectedText());
        waitForPanelToPeek();
        clickNode("states-far");
        waitForPanelToClose();
        Assert.assertNull(getSelectedText());
        clickNode("states-far");
        waitForPanelToPeek();
        Assert.assertEquals("States", getSelectedText());
    }

    /**
     * Tests a "retap" -- that sequential Tap gestures nearby keep selecting.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @DisabledTest(message = "https://crbug.com/1075895")
    public void testTapGesturesNearbyKeepSelecting() throws Exception {
        FeatureList.setTestFeatures(ENABLE_NONE);

        clickWordNode("states");
        Assert.assertEquals("States", getSelectedText());
        waitForPanelToPeek();
        assertLoggedAllExpectedFeaturesToRanker();
        // Avoid issues with double-tap detection by ensuring sequential taps
        // aren't treated as such. Double-tapping can also select words much as
        // longpress, in turn showing the pins and preventing contextual tap
        // refinement from nearby taps. The double-tap timeout is sufficiently
        // short that this shouldn't conflict with tap refinement by the user.
        Thread.sleep(ViewConfiguration.getDoubleTapTimeout());
        // Because sequential taps never hide the bar, we we can't wait for it to peek.
        // Instead we use clickNode (which doesn't wait) instead of clickWordNode and wait
        // for the selection to change.
        clickNode("states-near");
        waitForSelectionToBe("StatesNear");
        assertLoggedAllExpectedOutcomesToRanker();
        assertLoggedAllExpectedFeaturesToRanker();
        Thread.sleep(ViewConfiguration.getDoubleTapTimeout());
        clickNode("states");
        waitForSelectionToBe("States");
        assertLoggedAllExpectedOutcomesToRanker();
    }

    //============================================================================================
    // Long-press non-triggering gesture tests.
    //============================================================================================

    /**
     * Tests that a long-press gesture followed by scrolling does not clear the selection.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.O, message = "crbug.com/1071080")
    public void testLongPressGestureFollowedByScrollMaintainsSelection(
            @EnabledFeature int enabledFeature) throws Exception {
        longPressNode("intelligence");
        waitForPanelToPeek();
        scrollBasePage();
        assertPanelClosedOrUndefined();
        Assert.assertEquals("Intelligence", getSelectedText());
        assertLoadedNoUrl();
    }

    /**
     * Tests that a long-press gesture followed by a tap does not select.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @DisabledTest(message = "See https://crbug.com/837998")
    public void testLongPressGestureFollowedByTapDoesntSelect() throws Exception {
        FeatureList.setTestFeatures(ENABLE_NONE);

        longPressNode("intelligence");
        waitForPanelToPeek();
        clickWordNode("states-far");
        waitForGestureToClosePanelAndAssertNoSelection();
        assertLoadedNoUrl();
    }

    //============================================================================================
    // Various Tests
    //============================================================================================

    /**
     * Tests that the panel closes when its base page crashes.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    @FlakyTest(message = "Disabled in 2018 due to flakes.  See https://crbug.com/832539.")
    public void testContextualSearchDismissedOnForegroundTabCrash(
            @EnabledFeature int enabledFeature) throws Exception {
        triggerResolve("states");
        Assert.assertEquals("States", getSelectedText());
        waitForPanelToPeek();

        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            ChromeTabUtils.simulateRendererKilledForTesting(
                    sActivityTestRule.getActivity().getActivityTab(), true);
        });

        // Give the panelState time to change
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(mPanel.getPanelState(), Matchers.not(PanelState.PEEKED));
        });

        assertPanelClosedOrUndefined();
    }

    /**
     * Test the the panel does not close when some background tab crashes.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testContextualSearchNotDismissedOnBackgroundTabCrash(
            @EnabledFeature int enabledFeature) throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), sActivityTestRule.getActivity());
        final Tab tab2 =
                TabModelUtils.getCurrentTab(sActivityTestRule.getActivity().getCurrentTabModel());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TabModelUtils.setIndex(sActivityTestRule.getActivity().getCurrentTabModel(), 0);
        });

        triggerResolve("states");
        Assert.assertEquals("States", getSelectedText());
        waitForPanelToPeek();

        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                () -> { ChromeTabUtils.simulateRendererKilledForTesting(tab2, false); });

        waitForPanelToPeek();
    }

    /*
     * Test that tapping on the open-new-tab icon before having a resolved search term does not
     * promote to a tab, and that after the resolution it does promote to a tab.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testPromotesToTab(@EnabledFeature int enabledFeature) throws Exception {
        // -------- SET UP ---------
        // Track Tab creation with this helper.
        final CallbackHelper tabCreatedHelper = new CallbackHelper();
        int tabCreatedHelperCallCount = tabCreatedHelper.getCallCount();
        TabModelSelectorObserver observer = new TabModelSelectorObserver() {
            @Override
            public void onNewTabCreated(Tab tab, @TabCreationState int creationState) {
                tabCreatedHelper.notifyCalled();
            }
        };
        sActivityTestRule.getActivity().getTabModelSelector().addObserver(observer);

        // -------- TEST ---------
        // Start a slow-resolve search and maximize the Panel.
        simulateSlowResolveSearch("search");
        maximizePanel();
        waitForPanelToMaximize();

        // A click should not promote since we are still waiting to Resolve.
        forceOpenTabIconClick();

        // Assert that the Panel is still maximized.
        waitForPanelToMaximize();

        // Let the Search Term Resolution finish.
        simulateSlowResolveFinished();

        // Now a click to promote to a separate tab.
        forceOpenTabIconClick();

        // The Panel should now be closed.
        waitForPanelToClose();

        // Make sure a tab was created.
        tabCreatedHelper.waitForCallback(tabCreatedHelperCallCount);

        // -------- CLEAN UP ---------
        sActivityTestRule.getActivity().getTabModelSelector().removeObserver(observer);
    }

    //============================================================================================
    // Tap-non-triggering when ARIA annotated as interactive.
    //============================================================================================

    /**
     * Tests that a Tap gesture on an element with an ARIA role does not trigger.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapOnRoleIgnored() throws Exception {
        FeatureList.setTestFeatures(ENABLE_NONE);

        @PanelState
        int initialState = mPanel.getPanelState();
        clickNode("role");
        assertPanelStillInState(initialState);
    }

    /**
     * Tests that a Tap gesture on an element with an ARIA attribute does not trigger.
     * http://crbug.com/542874
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapOnARIAIgnored() throws Exception {
        FeatureList.setTestFeatures(ENABLE_NONE);

        @PanelState
        int initialState = mPanel.getPanelState();
        clickNode("aria");
        assertPanelStillInState(initialState);
    }

    /**
     * Tests that a Tap gesture on an element that is focusable does not trigger.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapOnFocusableIgnored() throws Exception {
        FeatureList.setTestFeatures(ENABLE_NONE);

        @PanelState
        int initialState = mPanel.getPanelState();
        clickNode("focusable");
        assertPanelStillInState(initialState);
    }

    //============================================================================================
    // Search-term resolution (server request to determine a search).
    //============================================================================================

    /**
     * Tests expanding the panel before the search term has resolved, verifies that nothing
     * loads until the resolve completes and that it's now a normal priority URL.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testExpandBeforeSearchTermResolution(@EnabledFeature int enabledFeature)
            throws Exception {
        simulateSlowResolveSearch("states");
        assertNoWebContents();

        // Expanding before the search term resolves should not load anything.
        tapPeekingBarToExpandAndAssert();
        assertLoadedNoUrl();

        // Once the response comes in, it should load.
        simulateSlowResolveFinished();
        assertContainsParameters("States");
        assertLoadedNormalPriorityUrl();
        assertWebContentsCreated();
        assertWebContentsVisible();
    }

    /**
     * Tests that an error from the Search Term Resolution request causes a fallback to a
     * search request for the literal selection.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    @DisableIf.Build(supported_abis_includes = "arm64-v8a", message = "crbug.com/765403")
    public void testSearchTermResolutionError(@EnabledFeature int enabledFeature) throws Exception {
        simulateSlowResolveSearch("states");
        assertSearchTermRequested();
        fakeResponse(false, 403, "", "", "", false);
        assertLoadedNoUrl();
        tapPeekingBarToExpandAndAssert();
        assertLoadedNormalPriorityUrl();
    }

    //============================================================================================
    // Undecided/Decided users.
    //============================================================================================

    /**
     * Tests that we do not resolve or preload when the privacy Opt-in has not been accepted.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testUnacceptedPrivacy(@EnabledFeature int enabledFeature) throws Exception {
        mPolicy.overrideDecidedStateForTesting(false);

        simulateResolvableSearchAndAssertResolveAndPreload("states", false);
    }

    /**
     * Tests that we do resolve and preload when the privacy Opt-in has been accepted.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testAcceptedPrivacy(@EnabledFeature int enabledFeature) throws Exception {
        mPolicy.overrideDecidedStateForTesting(true);

        simulateResolvableSearchAndAssertResolveAndPreload("states", true);
    }

    //============================================================================================
    // App Menu Suppression
    //============================================================================================

    /**
     * Simulates pressing the App Menu button.
     */
    private void pressAppMenuKey() {
        pressKey(KeyEvent.KEYCODE_MENU);
    }

    private void closeAppMenu() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> sActivityTestRule.getAppMenuCoordinator().getAppMenuHandler().hideAppMenu());
    }

    /**
     * Asserts whether the App Menu is visible.
     */
    private void assertAppMenuVisibility(final boolean isVisible) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(sActivityTestRule.getAppMenuCoordinator()
                                       .getAppMenuHandler()
                                       .isAppMenuShowing(),
                    Matchers.is(isVisible));
        });
    }

    /**
     * Tests that the App Menu gets suppressed when Search Panel is expanded.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    @DisableIf.Build(supported_abis_includes = "arm64-v8a", message = "crbug.com/596533")
    public void testAppMenuSuppressedWhenExpanded(@EnabledFeature int enabledFeature)
            throws Exception {
        triggerResolve("states");
        tapPeekingBarToExpandAndAssert();

        pressAppMenuKey();
        assertAppMenuVisibility(false);

        closePanel();

        pressAppMenuKey();
        assertAppMenuVisibility(true);

        closeAppMenu();
    }

    /**
     * Tests that the App Menu gets suppressed when Search Panel is maximized.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testAppMenuSuppressedWhenMaximized(@EnabledFeature int enabledFeature)
            throws Exception {
        triggerResolve("states");
        maximizePanel();
        waitForPanelToMaximize();

        pressAppMenuKey();
        assertAppMenuVisibility(false);

        pressBackButton();
        waitForPanelToClose();

        pressAppMenuKey();
        assertAppMenuVisibility(true);

        closeAppMenu();
    }

    // --------------------------------------------------------------------------------------------
    // Promo open count - watches if the promo has never been opened.
    // --------------------------------------------------------------------------------------------

    /**
     * Tests the promo open counter for users that have not opted-in to privacy.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @DisableIf.Build(supported_abis_includes = "arm64-v8a", message = "crbug.com/596533")
    // Only useful for disabling Tap triggering.
    @DisabledTest(message = "crbug.com/965706")
    public void testPromoOpenCountForUndecided() throws Exception {
        FeatureList.setTestFeatures(ENABLE_NONE);

        mPolicy.overrideDecidedStateForTesting(false);

        // A simple click / resolve / prefetch sequence without open should not change the counter.
        clickToTriggerPrefetch();
        Assert.assertEquals(0, mPolicy.getPromoOpenCount());

        // An open should count.
        clickToExpandAndClosePanel();
        Assert.assertEquals(1, mPolicy.getPromoOpenCount());

        // Another open should count.
        clickToExpandAndClosePanel();
        Assert.assertEquals(2, mPolicy.getPromoOpenCount());

        // Once the user has decided, we should stop counting.
        mPolicy.overrideDecidedStateForTesting(true);
        clickToExpandAndClosePanel();
        Assert.assertEquals(2, mPolicy.getPromoOpenCount());
    }

    /**
     * Tests the promo open counter for users that have already opted-in to privacy.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @DisableIf.Build(supported_abis_includes = "arm64-v8a", message = "crbug.com/596533")
    public void testPromoOpenCountForDecided() throws Exception {
        FeatureList.setTestFeatures(ENABLE_NONE);

        mPolicy.overrideDecidedStateForTesting(true);

        // An open should not count for decided users.
        clickToExpandAndClosePanel();
        Assert.assertEquals(0, mPolicy.getPromoOpenCount());
    }

    // --------------------------------------------------------------------------------------------
    // Tap count - number of taps between opens.
    // --------------------------------------------------------------------------------------------
    /**
     * Tests the counter for the number of taps between opens.
     */
    @Test
    @DisabledTest(message = "crbug.com/800334")
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapCount() throws Exception {
        FeatureList.setTestFeatures(ENABLE_NONE);

        resetCounters();
        Assert.assertEquals(0, mPolicy.getTapCount());

        // A simple Tap should change the counter.
        clickToTriggerPrefetch();
        Assert.assertEquals(1, mPolicy.getTapCount());

        // Another Tap should increase the counter.
        clickToTriggerPrefetch();
        Assert.assertEquals(2, mPolicy.getTapCount());

        // An open should reset the counter.
        clickToExpandAndClosePanel();
        Assert.assertEquals(0, mPolicy.getTapCount());
    }

    //============================================================================================
    // Calls to ContextualSearchObserver.
    //============================================================================================
    private static class TestContextualSearchObserver implements ContextualSearchObserver {
        private int mShowCount;
        private int mShowRedactedCount;
        private int mHideCount;
        private int mFirstShownLength;
        private int mLastShownLength;

        @Override
        public void onShowContextualSearch(@Nullable GSAContextDisplaySelection selectionContext) {
            mShowCount++;
            if (selectionContext != null
                    && selectionContext.startOffset < selectionContext.endOffset) {
                mLastShownLength = selectionContext.endOffset - selectionContext.startOffset;
                if (mFirstShownLength == 0) mFirstShownLength = mLastShownLength;
            } else {
                mShowRedactedCount++;
            }
        }

        @Override
        public void onHideContextualSearch() {
            mHideCount++;
        }

        /**
         * @return The count of Hide notifications sent to observers.
         */
        int getHideCount() {
            return mHideCount;
        }

        /**
         * @return The count of Show notifications sent to observers.
         */
        int getShowCount() {
            return mShowCount;
        }

        /**
         * @return The count of Show notifications sent to observers that had the data redacted due
         *         to our policy on privacy.
         */
        int getShowRedactedCount() {
            return mShowRedactedCount;
        }

        /**
         * @return The length of the selection for the first Show notification.
         */
        int getFirstShownLength() {
            return mFirstShownLength;
        }

        /**
         * @return The length of the selection for the last Show notification.
         */
        int getLastShownLength() {
            return mLastShownLength;
        }
    }

    /**
     * Tests that a ContextualSearchObserver gets notified when the user brings up The Contextual
     * Search panel via long press and then dismisses the panel by tapping on the base page.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testNotifyObserversAfterNonResolve(@EnabledFeature int enabledFeature)
            throws Exception {
        TestContextualSearchObserver observer = new TestContextualSearchObserver();
        mManager.addObserver(observer);
        triggerNonResolve("states");
        Assert.assertEquals(1, observer.getShowCount());
        Assert.assertEquals(0, observer.getHideCount());

        tapBasePageToClosePanel();
        Assert.assertEquals(1, observer.getShowCount());
        Assert.assertEquals(1, observer.getHideCount());
        mManager.removeObserver(observer);
    }

    /**
     * Tests that a ContextualSearchObserver gets notified without any page context when the user
     * is Undecided and our policy disallows sending surrounding text.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testNotifyObserversAfterLongPressWithoutSurroundings(
            @EnabledFeature int enabledFeature) throws Exception {
        // Mark the user undecided so we won't allow sending surroundings.
        mPolicy.overrideDecidedStateForTesting(false);
        TestContextualSearchObserver observer = new TestContextualSearchObserver();
        mManager.addObserver(observer);
        triggerNonResolve("states");
        Assert.assertEquals(1, observer.getShowRedactedCount());
        Assert.assertEquals(1, observer.getShowCount());
        Assert.assertEquals(0, observer.getHideCount());

        tapBasePageToClosePanel();
        Assert.assertEquals(1, observer.getShowRedactedCount());
        Assert.assertEquals(1, observer.getShowCount());
        Assert.assertEquals(1, observer.getHideCount());
        mManager.removeObserver(observer);
    }

    /**
     * Tests that ContextualSearchObserver gets notified when user brings up contextual search
     * panel and then dismisses the panel by tapping on the base page.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testNotifyObserversAfterResolve(@EnabledFeature int enabledFeature)
            throws Exception {
        TestContextualSearchObserver observer = new TestContextualSearchObserver();
        mManager.addObserver(observer);
        simulateResolveSearch("states");
        Assert.assertEquals(1, observer.getShowCount());
        Assert.assertEquals(0, observer.getHideCount());

        tapBasePageToClosePanel();
        Assert.assertEquals(1, observer.getShowCount());
        Assert.assertEquals(1, observer.getHideCount());
        mManager.removeObserver(observer);
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
     * Tests that ContextualSearchObserver gets notified when the user brings up the contextual
     * search panel via long press and then dismisses the panel by tapping copy (hide select action
     * mode).
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testNotifyObserversOnClearSelectionAfterLongpress(
            @EnabledFeature int enabledFeature) throws Exception {
        TestContextualSearchObserver observer = new TestContextualSearchObserver();
        mManager.addObserver(observer);
        longPressNode("states");
        Assert.assertEquals(0, observer.getHideCount());

        // Dismiss select action mode.
        assertWaitForSelectActionBarVisible(true);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> getSelectionPopupController().destroySelectActionMode());
        assertWaitForSelectActionBarVisible(false);

        waitForPanelToClose();
        Assert.assertEquals(1, observer.getShowCount());
        Assert.assertEquals(1, observer.getHideCount());
        mManager.removeObserver(observer);
    }

    /**
     * Tests that the Contextual Search panel does not reappear when a long-press selection is
     * modified after the user has taken an action to explicitly dismiss the panel. Also tests
     * that the panel reappears when a new selection is made.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testPreventHandlingCurrentSelectionModification() throws Exception {
        FeatureList.setTestFeatures(ENABLE_NONE);

        simulateNonResolveSearch("search");

        // Dismiss the Contextual Search panel.
        closePanel();
        Assert.assertEquals("Search", getSelectedText());

        // Simulate a selection change event and assert that the panel has not reappeared.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SelectionClient selectionClient = mManager.getContextualSearchSelectionClient();
            selectionClient.onSelectionEvent(
                    SelectionEventType.SELECTION_HANDLE_DRAG_STARTED, 333, 450);
            selectionClient.onSelectionEvent(
                    SelectionEventType.SELECTION_HANDLE_DRAG_STOPPED, 303, 450);
        });
        assertPanelClosedOrUndefined();

        // Select a different word and assert that the panel has appeared.
        simulateNonResolveSearch("resolution");
        // The simulateNonResolveSearch call will verify that the panel peeks.
    }

    /**
     * Tests a bunch of taps in a row.
     * We've had reliability problems with a sequence of simple taps, due to async dissolving
     * of selection bounds, so this helps prevent a regression with that.
     */
    @Test
    @LargeTest
    @Feature({"ContextualSearch"})
    @FlakyTest(message = "crbug.com/1036414, crbug.com/1039488")
    public void testTapALot() throws Exception {
        FeatureList.setTestFeatures(ENABLE_NONE);

        for (int i = 0; i < 50; i++) {
            clickToTriggerPrefetch();
            assertSearchTermRequested();
        }
    }

    /**
     * Tests ContextualSearchManager#shouldInterceptNavigation for a case that an external
     * navigation has a user gesture.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.Q, message = "crbug.com/1037667")
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testExternalNavigationWithUserGesture(@EnabledFeature int enabledFeature) {
        final ExternalNavigationDelegateImpl delegate =
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        ()
                                -> new ExternalNavigationDelegateImpl(
                                        sActivityTestRule.getActivity().getActivityTab()));
        final ExternalNavigationHandler externalNavHandler =
                new ExternalNavigationHandler(delegate);
        final NavigationParams navigationParams = new NavigationParams(
                new GURL("intent://test/#Intent;scheme=test;package=com.chrome.test;end"),
                GURL.emptyGURL(), 0 /* navigationId */, false /* isPost */,
                true /* hasUserGesture */, PageTransition.LINK, false /* isRedirect */,
                true /* isExternalProtocol */, true /* isMainFrame */,
                true /* isRendererInitiated */, false /* hasUserGestureCarryover */,
                null /* initiatorOrigin */);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                sActivityTestRule.getActivity().onUserInteraction();
                Assert.assertFalse(mManager.getOverlayContentDelegate().shouldInterceptNavigation(
                        externalNavHandler, navigationParams));
            }
        });
        Assert.assertEquals(1, mActivityMonitor.getHits());
    }

    /**
     * Tests ContextualSearchManager#shouldInterceptNavigation for a case that an initial
     * navigation has a user gesture but the redirected external navigation doesn't.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.Q, message = "crbug.com/1037667")
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testRedirectedExternalNavigationWithUserGesture(
            @EnabledFeature int enabledFeature) {
        final ExternalNavigationDelegateImpl delegate =
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        ()
                                -> new ExternalNavigationDelegateImpl(
                                        sActivityTestRule.getActivity().getActivityTab()));
        final ExternalNavigationHandler externalNavHandler =
                new ExternalNavigationHandler(delegate);

        final NavigationParams initialNavigationParams =
                new NavigationParams(new GURL("http://test.com"), GURL.emptyGURL(),
                        0 /* navigationId */, false /* isPost */, true /* hasUserGesture */,
                        PageTransition.LINK, false /* isRedirect */, false /* isExternalProtocol */,
                        true /* isMainFrame */, true /* isRendererInitiated */,
                        false /* hasUserGestureCarryover */, null /* initiatorOrigin */);
        final NavigationParams redirectedNavigationParams = new NavigationParams(
                new GURL("intent://test/#Intent;scheme=test;package=com.chrome.test;end"),
                GURL.emptyGURL(), 0 /* navigationId */, false /* isPost */,
                false /* hasUserGesture */, PageTransition.LINK, true /* isRedirect */,
                true /* isExternalProtocol */, true /* isMainFrame */,
                true /* isRendererInitiated */, false /* hasUserGestureCarryover */,
                null /* initiatorOrigin */);

        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                sActivityTestRule.getActivity().onUserInteraction();
                OverlayContentDelegate delegate = mManager.getOverlayContentDelegate();
                Assert.assertTrue(delegate.shouldInterceptNavigation(
                        externalNavHandler, initialNavigationParams));
                Assert.assertFalse(delegate.shouldInterceptNavigation(
                        externalNavHandler, redirectedNavigationParams));
            }
        });
        Assert.assertEquals(1, mActivityMonitor.getHits());
    }

    /**
     * Tests ContextualSearchManager#shouldInterceptNavigation for a case that an external
     * navigation doesn't have a user gesture.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testExternalNavigationWithoutUserGesture(@EnabledFeature int enabledFeature) {
        final ExternalNavigationDelegateImpl delegate =
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        ()
                                -> new ExternalNavigationDelegateImpl(
                                        sActivityTestRule.getActivity().getActivityTab()));
        final ExternalNavigationHandler externalNavHandler =
                new ExternalNavigationHandler(delegate);
        final NavigationParams navigationParams = new NavigationParams(
                new GURL("intent://test/#Intent;scheme=test;package=com.chrome.test;end"),
                GURL.emptyGURL(), 0 /* navigationId */, false /* isPost */,
                false /* hasUserGesture */, PageTransition.LINK, false /* isRedirect */,
                true /* isExternalProtocol */, true /* isMainFrame */,
                true /* isRendererInitiated */, false /* hasUserGestureCarryover */,
                null /* initiatorOrigin */);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                sActivityTestRule.getActivity().onUserInteraction();
                Assert.assertFalse(mManager.getOverlayContentDelegate().shouldInterceptNavigation(
                        externalNavHandler, navigationParams));
            }
        });
        Assert.assertEquals(0, mActivityMonitor.getHits());
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testSelectionExpansionOnSearchTermResolution(@EnabledFeature int enabledFeature)
            throws Exception {
        mFakeServer.reset();
        triggerResolve("intelligence");
        waitForPanelToPeek();

        ResolvedSearchTerm resolvedSearchTerm =
                new ResolvedSearchTerm
                        .Builder(false, 200, "Intelligence", "United States Intelligence")
                        .setSelectionStartAdjust(-14)
                        .build();
        fakeResponse(resolvedSearchTerm);
        waitForSelectionToBe("United States Intelligence");
    }

    //============================================================================================
    // Content Tests
    //============================================================================================

    /**
     * Tests that resolve followed by expand makes Content visible.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testResolveContentVisibility(@EnabledFeature int enabledFeature) throws Exception {
        // Simulate a resolving search and make sure Content is not visible.
        simulateResolveSearch("search");
        assertWebContentsCreatedButNeverMadeVisible();

        // Expanding the Panel should make the Content visible.
        tapPeekingBarToExpandAndAssert();
        assertWebContentsVisible();

        // Closing the Panel should destroy the Content.
        tapBasePageToClosePanel();
        assertNoWebContents();
    }

    /**
     * Tests that a non-resolving trigger followed by panel-expand creates Content and makes it
     * visible.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testNonResolveContentVisibility(@EnabledFeature int enabledFeature)
            throws Exception {
        // Simulate a non-resolve search and make sure no Content is created.
        simulateNonResolveSearch("search");
        assertNoWebContents();
        assertNoSearchesLoaded();

        // Expanding the Panel should make the Content visible.
        tapPeekingBarToExpandAndAssert();
        assertWebContentsCreated();
        assertWebContentsVisible();

        // Closing the Panel should destroy the Content.
        tapBasePageToClosePanel();
        assertNoWebContents();
    }

    /**
     * Tests swiping panel up and down after a tap search will only load the Content once.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    @FlakyTest(message = "crbug.com/1032955")
    public void testResolveMultipleSwipeOnlyLoadsContentOnce(@EnabledFeature int enabledFeature)
            throws Exception {
        // Simulate a resolving search and make sure Content is not visible.
        simulateResolveSearch("search");
        assertWebContentsCreatedButNeverMadeVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Expanding the Panel should make the Content visible.
        tapPeekingBarToExpandAndAssert();
        assertWebContentsVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Swiping the Panel down should not change the visibility or load content again.
        swipePanelDown();
        waitForPanelToPeek();
        assertWebContentsVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Expanding the Panel should not change the visibility or load content again.
        tapPeekingBarToExpandAndAssert();
        assertWebContentsVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Closing the Panel should destroy the Content.
        tapBasePageToClosePanel();
        assertNoWebContents();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
    }

    /**
     * Tests swiping panel up and down after a non-resolving search will only load the Content
     * once.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.P, message = "crbug.com/1032760")
    public void testNonResolveMultipleSwipeOnlyLoadsContentOnce(@EnabledFeature int enabledFeature)
            throws Exception {
        // Simulate a non-resolve search and make sure no Content is created.
        simulateNonResolveSearch("search");
        assertNoWebContents();
        assertNoSearchesLoaded();

        // Expanding the Panel should load the URL and make the Content visible.
        tapPeekingBarToExpandAndAssert();
        assertWebContentsCreated();
        assertWebContentsVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Swiping the Panel down should not change the visibility or load content again.
        swipePanelDown();
        waitForPanelToPeek();
        assertWebContentsVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Expanding the Panel should not change the visibility or load content again.
        tapPeekingBarToExpandAndAssert();
        assertWebContentsVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Closing the Panel should destroy the Content.
        tapBasePageToClosePanel();
        assertNoWebContents();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
    }

    /**
     * Tests that chained tap searches create new Content.
     * Chained Tap searches allow immediate triggering of a tap when quite close to a previous tap
     * selection since the user may have just missed the intended target.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testChainedSearchCreatesNewContent(@EnabledFeature int enabledFeature)
            throws Exception {
        // This test depends on preloading the content - which is loaded and not made visible.
        // We only preload when the user has decided to accept the privacy opt-in.
        mPolicy.overrideDecidedStateForTesting(true);

        // Simulate a resolving search and make sure Content is not visible.
        simulateResolveSearch("search");
        assertWebContentsCreatedButNeverMadeVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        WebContents wc1 = getPanelWebContents();

        waitToPreventDoubleTapRecognition();

        // Simulate a new resolving search and make sure new Content is created.
        simulateResolveSearch("term");
        assertWebContentsCreatedButNeverMadeVisible();
        Assert.assertEquals(2, mFakeServer.getLoadedUrlCount());
        WebContents wc2 = getPanelWebContents();
        Assert.assertNotSame(wc1, wc2);

        waitToPreventDoubleTapRecognition();

        // Simulate a new resolving search and make sure new Content is created.
        simulateResolveSearch("resolution");
        assertWebContentsCreatedButNeverMadeVisible();
        Assert.assertEquals(3, mFakeServer.getLoadedUrlCount());
        WebContents wc3 = getPanelWebContents();
        Assert.assertNotSame(wc2, wc3);

        // Closing the Panel should destroy the Content.
        closePanel();
        assertNoWebContents();
        Assert.assertEquals(3, mFakeServer.getLoadedUrlCount());
    }

    /**
     * Tests that chained searches load correctly.
     */
    @Test
    @DisabledTest(message = "crbug.com/551711")
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testChainedSearchLoadsCorrectSearchTerm(@EnabledFeature int enabledFeature)
            throws Exception {
        // Simulate a resolving search and make sure Content is not visible.
        simulateResolveSearch("search");
        assertWebContentsCreatedButNeverMadeVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        WebContents wc1 = getPanelWebContents();

        // Expanding the Panel should make the Content visible.
        tapPeekingBarToExpandAndAssert();
        assertWebContentsVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Swiping the Panel down should not change the visibility or load content again.
        swipePanelDown();
        waitForPanelToPeek();
        assertWebContentsVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        waitToPreventDoubleTapRecognition();

        // Now simulate a non-resolve search, leaving the Panel peeking.
        simulateNonResolveSearch("resolution");

        // Expanding the Panel should load and display the new search.
        tapPeekingBarToExpandAndAssert();
        assertWebContentsCreated();
        assertWebContentsVisible();
        Assert.assertEquals(2, mFakeServer.getLoadedUrlCount());
        assertLoadedSearchTermMatches("Resolution");
        WebContents wc2 = getPanelWebContents();
        Assert.assertNotSame(wc1, wc2);

        // Closing the Panel should destroy the Content.
        tapBasePageToClosePanel();
        assertNoWebContents();
        Assert.assertEquals(2, mFakeServer.getLoadedUrlCount());
    }

    /**
     * Tests that chained searches make Content visible when opening the Panel.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testChainedSearchContentVisibility() throws Exception {
        // Chained searches are tap-triggered very close to existing tap-triggered searches.
        FeatureList.setTestFeatures(ENABLE_NONE);

        // Simulate a resolving search and make sure Content is not visible.
        simulateResolveSearch("search");
        assertWebContentsCreatedButNeverMadeVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        WebContents wc1 = getPanelWebContents();

        waitToPreventDoubleTapRecognition();

        // Now simulate a non-resolve search, leaving the Panel peeking.
        simulateNonResolveSearch("resolution");
        assertNeverCalledWebContentsOnShow();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Expanding the Panel should load and display the new search.
        tapPeekingBarToExpandAndAssert();
        assertWebContentsCreated();
        assertWebContentsVisible();
        Assert.assertEquals(2, mFakeServer.getLoadedUrlCount());
        assertLoadedSearchTermMatches("Resolution");
        WebContents wc2 = getPanelWebContents();
        Assert.assertNotSame(wc1, wc2);
    }

    //============================================================================================
    // History Removal Tests.  These are important for privacy, and are not easy to test manually.
    //============================================================================================

    /**
     * Tests that a tap followed by closing the Panel removes the loaded URL from history.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testTapCloseRemovedFromHistory(@EnabledFeature int enabledFeature)
            throws Exception {
        // Simulate a resolving search and make sure a URL was loaded.
        simulateResolveSearch("search");
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        String url = mFakeServer.getLoadedUrl();

        // Close the Panel without seeing the Content.
        closePanel();

        // Now check that the URL has been removed from history.
        Assert.assertTrue(mFakeServer.hasRemovedUrl(url));
    }

    /**
     * Tests that a tap followed by opening the Panel does not remove the loaded URL from history.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.O, message = "crbug.com/1184410")
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testTapExpandNotRemovedFromHistory(@EnabledFeature int enabledFeature)
            throws Exception {
        // Simulate a resolving search and make sure a URL was loaded.
        simulateResolveSearch("search");
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        String url = mFakeServer.getLoadedUrl();

        // Expand Panel so that the Content becomes visible.
        tapPeekingBarToExpandAndAssert();

        // Close the Panel.
        tapBasePageToClosePanel();

        // Now check that the URL has not been removed from history, since the Content was seen.
        Assert.assertFalse(mFakeServer.hasRemovedUrl(url));
    }

    /**
     * Tests that chained searches without opening the Panel removes all loaded URLs from history.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.P, message = "crbug.com/1161540")
    public void testChainedTapsRemovedFromHistory() throws Exception {
        // Make sure we use tap for the simulateResolveSearch since only tap chains.
        FeatureList.setTestFeatures(ENABLE_NONE);

        // Simulate a resolving search and make sure a URL was loaded.
        simulateResolveSearch("search");
        String url1 = mFakeServer.getLoadedUrl();
        Assert.assertNotNull(url1);

        waitToPreventDoubleTapRecognition();

        // Simulate another resolving search and make sure another URL was loaded.
        simulateResolveSearch("term");
        String url2 = mFakeServer.getLoadedUrl();
        Assert.assertNotSame(url1, url2);

        waitToPreventDoubleTapRecognition();

        // Simulate another resolving search and make sure another URL was loaded.
        simulateResolveSearch("resolution");
        String url3 = mFakeServer.getLoadedUrl();
        Assert.assertNotSame(url2, url3);

        // Close the Panel without seeing any Content.
        closePanel();

        // Now check that all three URLs have been removed from history.
        Assert.assertEquals(3, mFakeServer.getLoadedUrlCount());
        Assert.assertTrue(mFakeServer.hasRemovedUrl(url1));
        Assert.assertTrue(mFakeServer.hasRemovedUrl(url2));
        Assert.assertTrue(mFakeServer.hasRemovedUrl(url3));
    }

    //============================================================================================
    // Translate Tests
    //============================================================================================

    /**
     * Tests that a simple Tap with language determination triggers translation.
     */
    @Test
    @FlakyTest(message = "https://crbug.com/1105488")
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testTapWithLanguage(@EnabledFeature int enabledFeature) throws Exception {
        // Resolving a German word should trigger translation.
        simulateResolveSearch("german");

        // Make sure we tried to trigger translate.
        Assert.assertTrue("Translation was not forced with the current request URL: "
                        + mManager.getRequest().getSearchUrl(),
                mManager.getRequest().isTranslationForced());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Search.ContextualSearch.TranslationNeeded"));
    }

    /**
     * Tests that a simple Tap without language determination does not trigger translation.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testTapWithoutLanguage(@EnabledFeature int enabledFeature) throws Exception {
        // Resolving an English word should NOT trigger translation.
        simulateResolveSearch("search");

        // Make sure we did not try to trigger translate.
        Assert.assertFalse(mManager.getRequest().isTranslationForced());
    }

    /**
     * Tests that a non-resolve search does trigger translation.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testNonResolveTranslates(@EnabledFeature int enabledFeature) throws Exception {
        // A non-resolving gesture on any word should trigger a forced translation.
        simulateNonResolveSearch("search");
        // Make sure we did try to trigger translate.
        Assert.assertTrue(mManager.getRequest().isTranslationForced());
    }

    /**
     * Tests the Translate Caption on a resolve gesture.
     * This test is disabled because it relies on the network and a live search result,
     * which would be flaky for bots.
     */
    @DisabledTest(message = "Useful for manual testing when a network is connected.")
    @Test
    @LargeTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testTranslateCaption(@EnabledFeature int enabledFeature) throws Exception {
        // Resolving a German word should trigger translation.
        simulateResolveSearch("german");

        // Make sure we tried to trigger translate.
        Assert.assertTrue("Translation was not forced with the current request URL: "
                        + mManager.getRequest().getSearchUrl(),
                mManager.getRequest().isTranslationForced());

        // Wait for the translate caption to be shown in the Bar.
        int waitFactor = 5; // We need to wait an extra long time for the panel content to render.
        CriteriaHelper.pollUiThread(() -> {
            ContextualSearchBarControl barControl = mPanel.getSearchBarControl();
            Criteria.checkThat(barControl, Matchers.notNullValue());
            Criteria.checkThat(barControl.getCaptionVisible(), Matchers.is(true));
            Criteria.checkThat(barControl.getCaptionText(), Matchers.notNullValue());
            Criteria.checkThat(
                    barControl.getCaptionText().toString(), Matchers.not(Matchers.isEmptyString()));
        }, 3000 * waitFactor, DEFAULT_POLLING_INTERVAL * waitFactor);
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTranslationsFeatureCanResolveLongpressGesture() throws Exception {
        FeatureList.setTestFeatures(ENABLE_TRANSLATIONS);

        Assert.assertTrue(mPolicy.canResolveLongpress());
    }

    //============================================================================================
    // END Translate Tests
    //============================================================================================

    /**
     * Tests that Contextual Search works in fullscreen. Specifically, tests that tapping a word
     * peeks the panel, expanding the bar results in the bar ending at the correct spot in the page
     * and tapping the base page closes the panel.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testTapContentAndExpandPanelInFullscreen(@EnabledFeature int enabledFeature)
            throws Exception {
        // Toggle tab to fulllscreen.
        FullscreenTestUtils.togglePersistentFullscreenAndAssert(
                sActivityTestRule.getActivity().getActivityTab(), true,
                sActivityTestRule.getActivity());

        // Simulate a resolving search and assert that the panel peeks.
        simulateResolveSearch("search");

        // Expand the panel and assert that it ends up in the right place.
        tapPeekingBarToExpandAndAssert();
        final ContextualSearchPanel panel =
                (ContextualSearchPanel) mManager.getContextualSearchPanel();
        Assert.assertEquals(
                panel.getHeight(), panel.getPanelHeightFromState(PanelState.EXPANDED), 0);

        // Tap the base page and assert that the panel is closed.
        tapBasePageToClosePanel();
    }

    /**
     * Tests that the Contextual Search panel is dismissed when entering or exiting fullscreen.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    @DisableIf.Device(type = {UiDisableIf.PHONE}) // Flaking on phones crbug.com/765796
    public void testPanelDismissedOnToggleFullscreen(@EnabledFeature int enabledFeature)
            throws Exception {
        // Simulate a resolving search and assert that the panel peeks.
        simulateResolveSearch("search");

        // Toggle tab to fullscreen.
        Tab tab = sActivityTestRule.getActivity().getActivityTab();
        FullscreenTestUtils.togglePersistentFullscreenAndAssert(
                tab, true, sActivityTestRule.getActivity());

        // Assert that the panel is closed.
        waitForPanelToClose();

        // Simulate a resolving search and assert that the panel peeks.
        simulateResolveSearch("search");

        // Toggle tab to non-fullscreen.
        FullscreenTestUtils.togglePersistentFullscreenAndAssert(
                tab, false, sActivityTestRule.getActivity());

        // Assert that the panel is closed.
        waitForPanelToClose();
    }

    /**
     * Tests that ContextualSearchImageControl correctly sets either the icon sprite or thumbnail
     * as visible.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testImageControl(@EnabledFeature int enabledFeature) throws Exception {
        simulateResolveSearch("search");

        final ContextualSearchImageControl imageControl = mPanel.getImageControl();

        Assert.assertFalse(imageControl.getThumbnailVisible());
        Assert.assertTrue(TextUtils.isEmpty(imageControl.getThumbnailUrl()));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            imageControl.setThumbnailUrl("http://someimageurl.com/image.png");
            imageControl.onThumbnailFetched(true);
        });

        Assert.assertTrue(imageControl.getThumbnailVisible());
        Assert.assertEquals(imageControl.getThumbnailUrl(), "http://someimageurl.com/image.png");

        TestThreadUtils.runOnUiThreadBlocking(() -> imageControl.hideCustomImage(false));

        Assert.assertFalse(imageControl.getThumbnailVisible());
        Assert.assertTrue(TextUtils.isEmpty(imageControl.getThumbnailUrl()));
    }

    // TODO(twellington): Add an end-to-end integration test for fetching a thumbnail based on a
    //                    a URL that is included with the resolution response.

    /**
     * Tests that Contextual Search is fully disabled when offline.
     */
    @Test
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    @FlakyTest(message = "Disabled in 2017.  https://crbug.com/761946")
    // @SmallTest
    // @Feature({"ContextualSearch"})
    // // NOTE: Remove the flag so we will run just this test with onLine detection enabled.
    // @CommandLineFlags.Remove(ContextualSearchFieldTrial.ONLINE_DETECTION_DISABLED)
    public void testNetworkDisconnectedDeactivatesSearch(@EnabledFeature int enabledFeature)
            throws Exception {
        setOnlineStatusAndReload(false);
        longPressNodeWithoutWaiting("states");
        waitForSelectActionBarVisible();
        // Verify the panel didn't open.  It should open by now if CS has not been disabled.
        // TODO(donnd): Consider waiting for some condition to be sure we'll catch all failures,
        // e.g. in case the Bar is about to show but has not yet appeared.  Currently catches ~90%.
        assertPanelClosedOrUndefined();

        // Similar sequence with network connected should peek for Longpress.
        setOnlineStatusAndReload(true);
        longPressNodeWithoutWaiting("states");
        waitForSelectActionBarVisible();
        waitForPanelToPeek();
    }

    /**
     * Tests that the quick action caption is set correctly when one is available. Also tests that
     * the caption gets changed when the panel is expanded and reset when the panel is closed.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testQuickActionCaptionAndImage(@EnabledFeature int enabledFeature)
            throws Exception {
        CompositorAnimationHandler.setTestingMode(true);

        // Simulate a resolving search to show the Bar, then set the quick action data.
        simulateResolveSearch("search");
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mPanel.onSearchTermResolved("search", null, "tel:555-555-5555",
                                QuickActionCategory.PHONE, CardTag.CT_CONTACT, null));

        ContextualSearchBarControl barControl = mPanel.getSearchBarControl();
        ContextualSearchQuickActionControl quickActionControl = barControl.getQuickActionControl();
        ContextualSearchImageControl imageControl = mPanel.getImageControl();

        // Check that the peeking bar is showing the quick action data.
        Assert.assertTrue(quickActionControl.hasQuickAction());
        Assert.assertTrue(barControl.getCaptionVisible());
        Assert.assertEquals(sActivityTestRule.getActivity().getResources().getString(
                                    R.string.contextual_search_quick_action_caption_phone),
                barControl.getCaptionText());
        Assert.assertEquals(1.f, imageControl.getCustomImageVisibilityPercentage(), 0);

        // Expand the bar.
        TestThreadUtils.runOnUiThreadBlocking(() -> mPanel.simulateTapOnEndButton());
        waitForPanelToExpand();

        // Check that the expanded bar is showing the correct image.
        Assert.assertEquals(0.f, imageControl.getCustomImageVisibilityPercentage(), 0);

        // Go back to peeking.
        swipePanelDown();
        waitForPanelToPeek();

        // Assert that the quick action data is showing.
        Assert.assertTrue(barControl.getCaptionVisible());
        Assert.assertEquals(sActivityTestRule.getActivity().getResources().getString(
                                    R.string.contextual_search_quick_action_caption_phone),
                barControl.getCaptionText());
        // TODO(donnd): figure out why we get ~0.65 on Oreo rather than 1. https://crbug.com/818515.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            Assert.assertEquals(1.f, imageControl.getCustomImageVisibilityPercentage(), 0);
        } else {
            Assert.assertTrue(0.5f < imageControl.getCustomImageVisibilityPercentage());
        }

        CompositorAnimationHandler.setTestingMode(false);
    }

    /**
     * Tests that an intent is sent when the bar is tapped and a quick action is available.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testQuickActionIntent(@EnabledFeature int enabledFeature) throws Exception {
        // Add a new filter to the activity monitor that matches the intent that should be fired.
        IntentFilter quickActionFilter = new IntentFilter(Intent.ACTION_VIEW);
        quickActionFilter.addDataScheme("tel");

        // Note that we don't reuse mActivityMonitor here or we would leak the one already added
        // (unless we removed it here first). When ActivityMonitors leak, Instrumentation silently
        // ignores matching ones added after and tests will fail.
        ActivityMonitor activityMonitor =
                InstrumentationRegistry.getInstrumentation().addMonitor(quickActionFilter,
                        new Instrumentation.ActivityResult(Activity.RESULT_OK, null), true);

        // Simulate a resolving search to show the Bar, then set the quick action data.
        simulateResolveSearch("search");
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mPanel.onSearchTermResolved("search", null, "tel:555-555-5555",
                                QuickActionCategory.PHONE, CardTag.CT_CONTACT, null));

        sActivityTestRule.getActivity().onUserInteraction();
        retryPanelBarInteractions(() -> {
            // Tap on the portion of the bar that should trigger the quick action intent to be
            // fired.
            clickPanelBar();

            // Assert that an intent was fired.
            Assert.assertEquals(1, activityMonitor.getHits());
        }, false);
        InstrumentationRegistry.getInstrumentation().removeMonitor(activityMonitor);
    }

    /**
     * Tests that the current tab is navigated to the quick action URI for
     * QuickActionCategory#WEBSITE.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.O, message = "crbug.com/1075895")
    @DisabledTest(message = "Flaky https://crbug.com/1127796")
    public void testQuickActionUrl_Longpress(@EnabledFeature int enabledFeature) throws Exception {
        // TODO(donnd): figure out why this fails to select on Longpress, but works fine on the
        // other experiment configurations including Translations (which should be identical for
        // this test). Probably something needs to be initialized between test runs.
        if (enabledFeature == EnabledFeature.LONGPRESS) return;

        final String testUrl = mTestServer.getURL("/chrome/test/data/android/google.html");

        // Simulate a resolving search to show the Bar, then set the quick action data.
        simulateResolveSearch("search");
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mPanel.onSearchTermResolved("search", null, testUrl,
                                QuickActionCategory.WEBSITE, CardTag.CT_URL, null));
        retryPanelBarInteractions(() -> {
            // Tap on the portion of the bar that should trigger the quick action.
            clickPanelBar();

            // Assert that the URL was loaded.
            ChromeTabUtils.waitForTabPageLoaded(
                    sActivityTestRule.getActivity().getActivityTab(), testUrl);
        }, false);
    }

    private void runDictionaryCardTest(@CardTag int cardTag) throws Exception {
        // Simulate a resolving search to show the Bar, then set the quick action data.
        simulateResolveSearch("search");
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mPanel.onSearchTermResolved("obscure · əbˈskyo͝or", null, null,
                                QuickActionCategory.NONE, cardTag, null));

        tapPeekingBarToExpandAndAssert();
    }

    /**
     * Tests that the flow for showing dictionary definitions works, and that tapping in the
     * bar just opens the panel instead of taking some action.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testDictionaryDefinitions(@EnabledFeature int enabledFeature) throws Exception {
        runDictionaryCardTest(CardTag.CT_DEFINITION);
    }

    /**
     * Tests that the flow for showing dictionary definitions works, and that tapping in the
     * bar just opens the panel instead of taking some action.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testContextualDictionaryDefinitions(@EnabledFeature int enabledFeature)
            throws Exception {
        runDictionaryCardTest(CardTag.CT_CONTEXTUAL_DEFINITION);
    }

    /**
     * Tests accessibility mode: Tap and Long-press don't activate CS.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testAccesibilityMode(@EnabledFeature int enabledFeature) throws Exception {
        mManager.onAccessibilityModeChanged(true);

        // Simulate a tap that resolves to show the Bar.
        clickNode("intelligence");
        assertNoWebContents();
        assertNoSearchesLoaded();

        // Simulate a Long-press.
        longPressNodeWithoutWaiting("states");
        assertNoWebContents();
        assertNoSearchesLoaded();
        mManager.onAccessibilityModeChanged(false);
    }

    /**
     * Tests when FirstRun is not completed: Tap and Long-press don't activate CS.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testFirstRunNotCompleted(@EnabledFeature int enabledFeature) throws Exception {
        // Store the original value in a temp, and mark the first run as not completed
        // for this test case.
        // Getting value from shared preference rather than FirstRunStatus#getFirstRunFlowComplete
        // to get rid of the impact from commandline switch. See https://crbug.com/1158467
        boolean originalIsFirstRunComplete = SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE, false);
        FirstRunStatus.setFirstRunFlowComplete(false);

        // Simulate a tap that resolves to show the Bar.
        clickNode("intelligence");
        assertNoWebContents();
        assertNoSearchesLoaded();

        // Simulate a Long-press.
        longPressNodeWithoutWaiting("states");
        assertNoWebContents();
        assertNoSearchesLoaded();

        // Restore the original shared preference value before this test case ends.
        FirstRunStatus.setFirstRunFlowComplete(originalIsFirstRunComplete);
    }

    //============================================================================================
    // Internal State Controller tests, which ensure that the internal logic flows as expected for
    // each type of triggering gesture.
    //============================================================================================

    /**
     * Tests that the Manager cycles through all the expected Internal States on Tap that Resolves.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testAllInternalStatesVisitedResolvingTap() throws Exception {
        FeatureList.setTestFeatures(ENABLE_NONE);

        // Set up a tracking version of the Internal State Controller.
        ContextualSearchInternalStateControllerWrapper internalStateControllerWrapper =
                ContextualSearchInternalStateControllerWrapper
                        .makeNewInternalStateControllerWrapper(mManager);
        mManager.setContextualSearchInternalStateController(internalStateControllerWrapper);

        // Simulate a gesture that resolves to show the Bar.
        simulateResolveSearch("search");

        Assert.assertEquals("Some states were started but never finished",
                internalStateControllerWrapper.getStartedStates(),
                internalStateControllerWrapper.getFinishedStates());
        Assert.assertEquals(
                "The resolving Tap gesture did not sequence through the expected states.",
                ContextualSearchInternalStateControllerWrapper.EXPECTED_TAP_RESOLVE_SEQUENCE,
                internalStateControllerWrapper.getFinishedStates());
        Assert.assertEquals(
                "The Tap gesture did not trigger a resolved search, or the resolve sequence did "
                        + "not complete.",
                InternalState.SHOWING_TAP_SEARCH, internalStateControllerWrapper.getState());
    }

    /**
     * Tests that the Manager cycles through all the expected Internal States on Long-press that
     * Resolves.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testAllInternalStatesVisitedResolvingLongpress(@EnabledFeature int enabledFeature)
            throws Exception {
        if (!mPolicy.canResolveLongpress()) return;

        // Set up a tracking version of the Internal State Controller.
        ContextualSearchInternalStateControllerWrapper internalStateControllerWrapper =
                ContextualSearchInternalStateControllerWrapper
                        .makeNewInternalStateControllerWrapper(mManager);
        mManager.setContextualSearchInternalStateController(internalStateControllerWrapper);

        // Simulate a resolving search to show the Bar.
        simulateResolveSearch("search");

        Assert.assertEquals("Some states were started but never finished",
                internalStateControllerWrapper.getStartedStates(),
                internalStateControllerWrapper.getFinishedStates());
        Assert.assertEquals(
                "The resolving Long-press gesture did not sequence through the expected states.",
                ContextualSearchInternalStateControllerWrapper.EXPECTED_LONGPRESS_RESOLVE_SEQUENCE,
                internalStateControllerWrapper.getFinishedStates());
        Assert.assertEquals(
                "The Long-press gesturedid not trigger a resolved search, or the resolve sequence "
                        + "did not complete.",
                InternalState.SHOWING_RESOLVED_LONG_PRESS_SEARCH,
                internalStateControllerWrapper.getState());
    }

    /**
     * Tests that the Manager cycles through all the expected Internal States on a Long-press.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testAllInternalStatesVisitedNonResolveLongpress() throws Exception {
        FeatureList.setTestFeatures(ENABLE_NONE);

        // Set up a tracking version of the Internal State Controller.
        ContextualSearchInternalStateControllerWrapper internalStateControllerWrapper =
                ContextualSearchInternalStateControllerWrapper
                        .makeNewInternalStateControllerWrapper(mManager);
        mManager.setContextualSearchInternalStateController(internalStateControllerWrapper);

        // Simulate a Long-press to show the Bar.
        simulateNonResolveSearch("search");

        Assert.assertEquals("Some states were started but never finished",
                internalStateControllerWrapper.getStartedStates(),
                internalStateControllerWrapper.getFinishedStates());
        Assert.assertEquals(
                "The non-resolving Long-press gesture didn't sequence through all of the expected "
                        + " states.",
                ContextualSearchInternalStateControllerWrapper.EXPECTED_LONGPRESS_SEQUENCE,
                internalStateControllerWrapper.getFinishedStates());
    }

    //============================================================================================
    // Various tests
    //============================================================================================

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testTriggeringContextualSearchHidesFindInPageOverlay(
            @EnabledFeature int enabledFeature) throws Exception {
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                sActivityTestRule.getActivity(), R.id.find_in_page_id);

        CriteriaHelper.pollUiThread(() -> {
            FindToolbar findToolbar =
                    (FindToolbar) sActivityTestRule.getActivity().findViewById(R.id.find_toolbar);
            Criteria.checkThat(findToolbar, Matchers.notNullValue());
            Criteria.checkThat(findToolbar.isShown(), Matchers.is(true));
            Criteria.checkThat(findToolbar.isAnimating(), Matchers.is(false));
        });

        // Don't type anything to Find because that may cause scrolling which makes clicking in the
        // page flaky.

        View findToolbar = sActivityTestRule.getActivity().findViewById(R.id.find_toolbar);
        Assert.assertTrue(findToolbar.isShown());

        simulateResolveSearch("search");

        waitForPanelToPeek();
        Assert.assertFalse(
                "Find Toolbar should no longer be shown once Contextual Search Panel appeared",
                findToolbar.isShown());
    }

    /**
     * Tests that expanding the selection during a Search Term Resolve notifies the observers before
     * and after the expansion.
     * TODO(donnd): move to the section for observer tests.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testNotifyObserversOnExpandSelection(@EnabledFeature int enabledFeature)
            throws Exception {
        mPolicy.overrideDecidedStateForTesting(true);
        TestContextualSearchObserver observer = new TestContextualSearchObserver();
        mManager.addObserver(observer);

        simulateSlowResolveSearch("states");
        simulateSlowResolveFinished();
        closePanel();

        Assert.assertEquals("States".length(), observer.getFirstShownLength());
        Assert.assertEquals("United States".length(), observer.getLastShownLength());
        Assert.assertEquals(2, observer.getShowCount());
        Assert.assertEquals(1, observer.getHideCount());
        mManager.removeObserver(observer);
    }

    /** Asserts that the given value is either 1 or 2.  Helpful for flaky tests. */
    private void assertValueIs1or2(int value) {
        if (value != 1) Assert.assertEquals(2, value);
    }

    /**
     * Tests a second Tap: a Tap on an existing tap-selection.
     * TODO(donnd): move to the section for observer tests.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testSecondTap() throws Exception {
        FeatureList.setTestFeatures(ENABLE_NONE);

        TestContextualSearchObserver observer = new TestContextualSearchObserver();
        mManager.addObserver(observer);

        clickWordNode("search");
        Assert.assertEquals(1, observer.getShowCount());
        Assert.assertEquals(0, observer.getHideCount());

        clickNode("search");
        waitForSelectActionBarVisible();
        closePanel();

        // Sometimes we get an additional Show notification on the second Tap, but not reliably in
        // tests.  See crbug.com/776541.
        assertValueIs1or2(observer.getShowCount());
        Assert.assertEquals(1, observer.getHideCount());
        mManager.removeObserver(observer);
    }

    /**
     * Tests Tab reparenting.  When a tab moves from one activity to another the
     * ContextualSearchTabHelper should detect the change and handle gestures for it too.  This
     * happens with multiwindow modes.
     */
    @Test
    @LargeTest
    @Feature({"ContextualSearch"})
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_TAB_MERGING_FOR_TESTING)
    @MinAndroidSdkLevel(Build.VERSION_CODES.N)
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testTabReparenting(@EnabledFeature int enabledFeature) throws Exception {
        // Move our "tap_test" tab to another activity.
        final ChromeActivity ca = sActivityTestRule.getActivity();

        // Create a new tab so |ca| isn't destroyed.
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(), ca);
        ChromeTabUtils.switchTabInCurrentTabModel(ca, 0);

        int testTabId = ca.getActivityTab().getId();
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(), ca,
                R.id.move_to_other_window_menu_id);

        // Wait for the second activity to start up and be ready for interaction.
        final ChromeTabbedActivity2 activity2 = waitForSecondChromeTabbedActivity();
        waitForTabs("CTA2", activity2, 1, testTabId);

        // Trigger on a word and wait for the selection to be established.
        triggerNode(activity2.getActivityTab(), "search");
        CriteriaHelper.pollUiThread(() -> {
            String selection = activity2.getContextualSearchManager()
                                       .getSelectionController()
                                       .getSelectedText();
            Criteria.checkThat(selection, Matchers.is("Search"));
        });
        TestThreadUtils.runOnUiThreadBlocking(() -> activity2.getCurrentTabModel().closeAllTabs());
        ApplicationTestUtils.finishActivity(activity2);
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    // TODO(donnd): Investigate support for logging user interactions for Long-press.
    public void testLoggedEventId() throws Exception {
        FeatureList.setTestFeatures(ENABLE_NONE);
        mFakeServer.reset();
        simulateResolveSearch("intelligence-logged-event-id");
        tapPeekingBarToExpandAndAssert();
        closePanel();
        // Now the event and outcome should be in local storage.
        simulateResolveSearch("search");
        // Check that we sent the logged event ID and outcome with the request.
        Assert.assertEquals(ContextualSearchFakeServer.LOGGED_EVENT_ID,
                mManager.getContext().getPreviousEventId());
        Assert.assertEquals(1, mManager.getContext().getPreviousUserInteractions());
        closePanel();
        // Now that we've sent them to the server, the local storage should be clear.
        simulateResolveSearch("search");
        Assert.assertEquals(0, mManager.getContext().getPreviousEventId());
        Assert.assertEquals(0, mManager.getContext().getPreviousUserInteractions());
        closePanel();
        // Make sure a duration was recorded in bucket 0 (due to 0 days duration running this test).
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Search.ContextualSearch.OutcomesDuration", 0));
    }

    // --------------------------------------------------------------------------------------------
    // Longpress-resolve Feature tests: force long-press experiment and make sure that triggers.
    // --------------------------------------------------------------------------------------------
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapIsIgnoredWithLongpressResolveEnabled() throws Exception {
        FeatureList.setTestFeatures(ENABLE_LONGPRESS);

        clickNode("states");
        Assert.assertNull(getSelectedText());
        assertPanelClosedOrUndefined();
        assertLoadedNoUrl();
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testLongpressResolveEnabled() throws Exception {
        FeatureList.setTestFeatures(ENABLE_LONGPRESS);

        longPressNode("states");
        assertLoadedNoUrl();
        assertSearchTermRequested();

        fakeResponse(false, 200, "states", "United States Intelligence", "alternate-term", false);
        waitForPanelToPeek();
        assertLoadedLowPriorityUrl();
        assertContainsParameters("states", "alternate-term");
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @DisableIf.Build(sdk_is_less_than = VERSION_CODES.O,
            message = "Flaky < P, https://crbug.com/1048827; Flaky on P, crbug.com/1181088")
    public void
    testLongpressExtendingSelectionExactResolve() throws Exception {
        FeatureList.setTestFeatures(ENABLE_LONGPRESS);

        // Set up UserAction monitoring.
        Set<String> userActions = new HashSet();
        userActions.add("ContextualSearch.SelectionEstablished");
        userActions.add("ContextualSearch.ManualRefine");
        UserActionMonitor userActionMonitor = new UserActionMonitor(userActions);

        // First test regular long-press.  It should not require an exact resolve.
        longPressNode("search");
        fakeAResponse();
        assertSearchTermRequested();
        assertExactResolve(false);

        // Long press a node without release so we can simulate the user extending the selection.
        long downTime = longPressNodeWithoutUp("search");

        // Extend the selection to the nearby word.
        longPressExtendSelection("term", "resolution", downTime);
        waitForSelectActionBarVisible();
        fakeAResponse();
        assertSearchTermRequested();
        assertExactResolve(true);

        // Check UMA metrics recorded.
        Assert.assertEquals(2, userActionMonitor.get("ContextualSearch.ManualRefine"));
        Assert.assertEquals(2, userActionMonitor.get("ContextualSearch.SelectionEstablished"));
    }

    // --------------------------------------------------------------------------------------------
    // Related Searches Feature tests: base feature enables requests, UI feature allows results.
    // --------------------------------------------------------------------------------------------

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testRelatedSearchesRequestedWhenEnabled() throws Exception {
        FeatureList.setTestFeatures(ENABLE_RELATED_SEARCHES);
        mPolicy.overrideAllowSendingPageUrlForTesting(true);
        simulateResolveSearch("search");
        Assert.assertFalse("Related Searches should have been requested but were not!",
                mFakeServer.getSearchContext().getRelatedSearchesStamp().isEmpty());
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.O, message = "crbug.com/1182040")
    public void testRelatedSearchesResponseWhenEnabled() throws Exception {
        FeatureList.setTestFeatures(ENABLE_RELATED_SEARCHES_UI);
        mFakeServer.reset();
        FakeResolveSearch fakeSearch = simulateResolveSearch("intelligence");
        ResolvedSearchTerm resolvedSearchTerm = fakeSearch.getResolvedSearchTerm();
        Assert.assertTrue("Related Searches results should have been returned but were not!",
                resolvedSearchTerm.relatedSearches().length > 0);
        // TODO(donnd): Add a check that the searches appeared in the Panel once the Panel can.
    }

    /**
     * Tests that a caption is shown on a non intelligent search when the force-caption feature is
     * enabled.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testNonResolveCaption() throws Exception {
        // Simulate a non-resolve search and make sure no Caption is shown.
        simulateNonResolveSearch("search");
        Assert.assertFalse(mPanel.getSearchBarControl().getCaptionVisible());
        closePanel();

        // Now try again with Caption-forcing.
        FeatureList.setTestFeatures(ENABLE_FORCE_CAPTION);
        simulateNonResolveSearch("search");
        Assert.assertTrue(mPanel.getSearchBarControl().getCaptionVisible());
        closePanel();
    }
}
