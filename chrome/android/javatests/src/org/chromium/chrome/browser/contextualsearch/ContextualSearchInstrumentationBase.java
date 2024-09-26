// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.junit.Assert.assertNotNull;

import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_POLLING_INTERVAL;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Point;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewConfiguration;
import android.widget.LinearLayout;

import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.PanelState;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelContentProgressObserver;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManagerWrapper;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFakeServer.ContextualSearchTestHost;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFakeServer.FakeResolveSearch;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFakeServer.FakeSlowResolveSearch;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.locale.LocaleManagerDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.SelectAroundCaretResult;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestSelectionPopupController;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.touch_selection.SelectionEventType;

import java.util.concurrent.TimeoutException;

/** This is a base class for various Contextual Search instrumentation tests. */
public class ContextualSearchInstrumentationBase {
    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    // --------------------------------------------------------------------------------------------

    /** ContextualSearchPanel wrapper that prevents native calls. */
    protected static class ContextualSearchPanelWrapper extends ContextualSearchPanel {
        public ContextualSearchPanelWrapper(
                Context context,
                LayoutManagerImpl layoutManager,
                OverlayPanelManager panelManager,
                Profile profile) {
            super(
                    context,
                    layoutManager,
                    panelManager,
                    null,
                    null,
                    profile,
                    null,
                    0,
                    null,
                    true,
                    null,
                    sActivityTestRule.getActivity().getEdgeToEdgeControllerSupplierForTesting());
        }

        @Override
        public void peekPanel(@StateChangeReason int reason) {
            setHeightForTesting(1);
            super.peekPanel(reason);
        }

        @Override
        public void setBasePageTextControlsVisibility(boolean visible) {}
    }

    // --------------------------------------------------------------------------------------------

    /** ContextualSearchManager wrapper that prevents network requests and most native calls. */
    protected static class ContextualSearchManagerWrapper extends ContextualSearchManager {
        public ContextualSearchManagerWrapper(ChromeActivity activity) {
            super(
                    activity,
                    ProfileManager.getLastUsedRegularProfile(),
                    null,
                    activity.getRootUiCoordinatorForTesting().getScrimCoordinator(),
                    activity.getActivityTabProvider(),
                    activity.getFullscreenManager(),
                    activity.getBrowserControlsManager(),
                    activity.getWindowAndroid(),
                    activity.getTabModelSelector(),
                    () -> activity.getLastUserInteractionTime(),
                    activity.getEdgeToEdgeControllerSupplierForTesting());
            setSelectionController(new MockCSSelectionController(activity, this));
            Profile profile = ProfileManager.getLastUsedRegularProfile();
            WebContents webContents = WebContentsFactory.createWebContents(profile, false, false);
            ContentView cv = ContentView.createContentView(activity, webContents);
            webContents.setDelegates(
                    null,
                    ViewAndroidDelegate.createBasicDelegate(cv),
                    null,
                    activity.getWindowAndroid(),
                    WebContents.createDefaultInternalsHolder());
            SelectionPopupController selectionPopupController =
                    WebContentsUtils.createSelectionPopupController(webContents);
            selectionPopupController.setSelectionClient(this.getContextualSearchSelectionClient());

            MockContextualSearchPolicy policy =
                    new MockContextualSearchPolicy(profile, getSelectionController());
            setContextualSearchPolicy(policy);
        }

        @Override
        public void startSearchTermResolutionRequest(
                String selection, boolean isExactResolve, ContextualSearchContext searchContext) {
            // Skip native calls and immediately "resolve" the search term.
            onSearchTermResolutionResponse(
                    true,
                    200,
                    selection,
                    selection,
                    "",
                    "",
                    false,
                    0,
                    10,
                    "",
                    "",
                    "",
                    "",
                    QuickActionCategory.NONE,
                    "",
                    "",
                    0,
                    "");
        }

        /**
         * @return A stubbed SelectionPopupController for mocking text selection.
         */
        public StubbedSelectionPopupController getBaseSelectionPopupController() {
            return (StubbedSelectionPopupController)
                    getSelectionController().getSelectionPopupController();
        }
    }

    // --------------------------------------------------------------------------------------------

    /** Selection controller that mocks out anything to do with a WebContents. */
    private static class MockCSSelectionController extends ContextualSearchSelectionController {
        private StubbedSelectionPopupController mPopupController;

        public MockCSSelectionController(
                ChromeActivity activity, ContextualSearchSelectionHandler handler) {
            super(activity, handler, activity.getActivityTabProvider());
            mPopupController = new StubbedSelectionPopupController();
        }

        @Override
        protected SelectionPopupController getSelectionPopupController() {
            return mPopupController;
        }
    }

    // --------------------------------------------------------------------------------------------

    /** A SelectionPopupController that has some methods stubbed out for testing. */
    protected static final class StubbedSelectionPopupController
            extends TestSelectionPopupController {
        private String mCurrentText;
        private boolean mIsFocusedNodeEditable;

        public StubbedSelectionPopupController() {}

        public void setIsFocusedNodeEditableForTest(boolean isFocusedNodeEditable) {
            mIsFocusedNodeEditable = isFocusedNodeEditable;
        }

        @Override
        public boolean isFocusedNodeEditable() {
            return mIsFocusedNodeEditable;
        }

        @Override
        public String getSelectedText() {
            return mCurrentText;
        }

        public void setSelectedText(String string) {
            mCurrentText = string;
        }
    }

    // --------------------------------------------------------------------------------------------

    /** Trigger text selection on the contextual search manager. */
    protected void mockLongpressText(String text) {
        mContextualSearchManager.getBaseSelectionPopupController().setSelectedText(text);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mContextualSearchClient.onSelectionEvent(
                                SelectionEventType.SELECTION_HANDLES_SHOWN, 0, 0));
    }

    /** Trigger text selection on the contextual search manager. */
    protected void mockTapText(String text) {
        mContextualSearchManager.getBaseSelectionPopupController().setSelectedText(text);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContextualSearchManager.getGestureStateListener().onTouchDown();
                    mContextualSearchManager.onShowUnhandledTapUIIfNeeded(0, 0);
                });
    }

    /** Trigger empty space tap. */
    protected void mockTapEmptySpace() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContextualSearchManager.onShowUnhandledTapUIIfNeeded(0, 0);
                    mContextualSearchClient.onSelectionEvent(
                            SelectionEventType.SELECTION_HANDLES_CLEARED, 0, 0);
                });
    }

    /** Generates a call indicating that surrounding text and selection range are available. */
    protected void generateTextSurroundingSelectionAvailable() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // It only makes sense to send placeholder data here because we can't easily
                    // control what's in the native context.
                    mContextualSearchManager.onTextSurroundingSelectionAvailable(
                            "UTF-8", "unused", 0, 0);
                });
    }

    /**
     * Generates an ACK for the SelectWordAroundCaret native call, which indicates that the select
     * action has completed with the given result.
     */
    protected void generateSelectWordAroundCaretAck() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // It only makes sense to send placeholder data here because we can't easily
                    // control what's in the native context.
                    mContextualSearchClient.selectAroundCaretAck(
                            new SelectAroundCaretResult(0, 0, 0, 0));
                });
    }

    // --------------------------------------------------------------------------------------------

    /**
     * The DOM node for the word "search" on the test page, which causes a plain search response
     * with the Search Term "Search" from the Fake server.
     */
    protected static final String SEARCH_NODE = "search";

    protected static final String SEARCH_NODE_TERM = "Search";

    /**
     * The DOM node for the word "intelligence" on the test page, which causes a search response for
     * the Search Term "Intelligence" and also includes Related Searches suggestions.
     */
    protected static final String RELATED_SEARCHES_NODE = "intelligence";

    private static final String TAG = "CSIBase";
    private static final int TEST_TIMEOUT = 1500;
    private static final int TEST_EXPECTED_FAILURE_TIMEOUT = 1000;

    private static final int PANEL_INTERACTION_RETRY_DELAY_MS = 200;

    private static final int DOUBLE_TAP_DELAY_MULTIPLIER = 3;

    // Search request URL paths and CGI parameters.
    private static final String LOW_PRIORITY_SEARCH_ENDPOINT = "/s?";
    private static final String NORMAL_PRIORITY_SEARCH_ENDPOINT = "/search?";
    private static final String LOW_PRIORITY_INVALID_SEARCH_ENDPOINT = "/s/invalid";
    private static final String CONTEXTUAL_SEARCH_PREFETCH_PARAM = "&pf=c";

    protected static final String EXTERNAL_APP_URL =
            "intent://test/#Intent;scheme=externalappscheme;end";

    protected ContextualSearchManager mManager;
    protected ContextualSearchPolicy mPolicy;
    protected ContextualSearchPanel mPanel;
    protected ContextualSearchFakeServer mFakeServer;
    protected EmbeddedTestServer mTestServer;

    protected String mTestPage = "/chrome/test/data/android/contextualsearch/simple_test.html";

    protected ContextualSearchManagerWrapper mContextualSearchManager;
    protected OverlayPanelManagerWrapper mPanelManager;
    private SelectionClient mContextualSearchClient;
    private LayoutManagerImpl mLayoutManager;

    protected ActivityMonitor mActivityMonitor;
    protected ContextualSearchSelectionController mSelectionController;
    private ContextualSearchInstrumentationTestHost mTestHost;

    private float mDpToPx;

    // State for an individual test.
    private FakeSlowResolveSearch mLatestSlowResolveSearch;

    @Before
    public void setUp() throws Exception {
        final ChromeActivity activity = sActivityTestRule.getActivity();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FirstRunStatus.setFirstRunFlowComplete(true);

                    mPanelManager = new OverlayPanelManagerWrapper();
                    mPanelManager.setContainerView(new LinearLayout(activity));
                    mContextualSearchManager = new ContextualSearchManagerWrapper(activity);
                    mContextualSearchClient =
                            mContextualSearchManager.getContextualSearchSelectionClient();

                    LocaleManager.getInstance()
                            .setDelegateForTest(
                                    new LocaleManagerDelegate() {
                                        @Override
                                        public boolean needToCheckForSearchEnginePromo() {
                                            return false;
                                        }
                                    });
                });

        mTestServer = sActivityTestRule.getTestServer();

        sActivityTestRule.loadUrl(mTestServer.getURL(mTestPage));
        // DOMUtils sometimes hits the wrong node due to an incorrect page scale factor,
        // so wait until that is set. https://crbug.com/1327063
        sActivityTestRule.assertWaitForPageScaleFactorMatch(1.0f);

        mManager = sActivityTestRule.getActivity().getContextualSearchManagerForTesting();
        mTestHost = new ContextualSearchInstrumentationTestHost();

        Assert.assertNotNull(mManager);
        mPanel = (ContextualSearchPanel) mManager.getContextualSearchPanel();
        Assert.assertNotNull(mPanel);

        mSelectionController = mManager.getSelectionController();
        mPolicy = mManager.getContextualSearchPolicy();
        mPolicy.overrideDecidedStateForTesting(true);

        mFakeServer =
                new ContextualSearchFakeServer(
                        mPolicy,
                        mTestHost,
                        mManager,
                        mManager.getOverlayPanelContentDelegate(),
                        new OverlayPanelContentProgressObserver(),
                        sActivityTestRule.getActivity());

        mPanel.setOverlayPanelContentFactory(mFakeServer);
        mManager.setNetworkCommunicator(mFakeServer);
        mPolicy.setNetworkCommunicator(mFakeServer);

        registerFakeSearches();

        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addCategory(Intent.CATEGORY_BROWSABLE);
        filter.addDataScheme("externalappscheme");
        mActivityMonitor =
                InstrumentationRegistry.getInstrumentation()
                        .addMonitor(
                                filter,
                                new Instrumentation.ActivityResult(Activity.RESULT_OK, null),
                                true);

        mDpToPx = sActivityTestRule.getActivity().getResources().getDisplayMetrics().density;

        // Set the test Features map for all tests regardless of whether they are parameterized.
        // Non-parameterized tests typically override this setting by calling setTestFeatures
        // again.
        // If Related Searches is enabled we need to also set that it's OK to send page content.
        mPolicy.overrideAllowSendingPageUrlForTesting(true);

        MockitoAnnotations.openMocks(this);
    }

    @After
    public void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FirstRunStatus.setFirstRunFlowComplete(false);

                    if (mManager != null) mManager.dismissContextualSearchBar();
                    if (mPanel != null) mPanel.closePanel(StateChangeReason.UNKNOWN, false);
                });
        if (mActivityMonitor != null) {
            InstrumentationRegistry.getInstrumentation().removeMonitor(mActivityMonitor);
        }
        mActivityMonitor = null;
        mLatestSlowResolveSearch = null;
        if (mPolicy != null) {
            mPolicy.overrideAllowSendingPageUrlForTesting(false);
        }
    }

    /** Allows the fake server to call into this host to drive actions when simulating a search. */
    private class ContextualSearchInstrumentationTestHost implements ContextualSearchTestHost {
        @Override
        public void triggerNonResolve(String nodeId) throws TimeoutException {
            boolean previousOptedInState = mPolicy.overrideDecidedStateForTesting(false);
            clickWordNode(nodeId);
            mPolicy.overrideDecidedStateForTesting(previousOptedInState);
        }

        @Override
        public void triggerResolve(String nodeId) throws TimeoutException {
            boolean previousOptedInState = mPolicy.overrideDecidedStateForTesting(true);
            clickWordNode(nodeId);
            mPolicy.overrideDecidedStateForTesting(previousOptedInState);
        }

        @Override
        public void waitForSelectionToBe(final String text) {
            CriteriaHelper.pollInstrumentationThread(
                    () -> {
                        Criteria.checkThat(getSelectedText(), Matchers.is(text));
                    },
                    TEST_TIMEOUT,
                    DEFAULT_POLLING_INTERVAL);
        }

        @Override
        public void waitForSearchTermResolutionToStart(final FakeResolveSearch search) {
            CriteriaHelper.pollInstrumentationThread(
                    () -> {
                        return search.didStartSearchTermResolution();
                    },
                    "Fake Search Term Resolution never started.",
                    TEST_TIMEOUT,
                    DEFAULT_POLLING_INTERVAL);
        }

        @Override
        public void waitForSearchTermResolutionToFinish(final FakeResolveSearch search) {
            CriteriaHelper.pollInstrumentationThread(
                    () -> {
                        return search.didFinishSearchTermResolution();
                    },
                    "Fake Search was never ready.",
                    TEST_TIMEOUT,
                    DEFAULT_POLLING_INTERVAL);
        }

        @Override
        public ContextualSearchPanel getPanel() {
            return mPanel;
        }
    }

    // ============================================================================================
    // Helper Functions and Methods.
    // TODO(donnd): Mark protected and use these in ContextualSearchManagerTest.
    // ============================================================================================

    /** Triggers the panel to show in the peeking state. */
    void triggerPanelPeek() throws Exception {
        // TODO(donnd): is it better to use the resolve or non-resolve implementation?
        simulateResolveSearch(SEARCH_NODE);
    }

    protected interface ThrowingRunnable {
        void run() throws TimeoutException;
    }

    protected void clearSelection() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SelectionPopupController.fromWebContents(sActivityTestRule.getWebContents())
                            .clearSelection();
                });
    }

    // ============================================================================================
    // Public API
    // ============================================================================================

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
        longPressNodeWithoutWaiting(nodeId);
        waitForPanelToPeek();
    }

    /**
     * Simulates a resolving trigger on the given node but does not wait for the panel to peek.
     *
     * @param nodeId A string containing the node ID.
     */
    protected void triggerResolve(String nodeId) throws TimeoutException {
        mTestHost.triggerResolve(nodeId);
    }

    /**
     * Simulates a non-resolve trigger on the given node and waits for the panel to peek.
     *
     * @param nodeId A string containing the node ID.
     */
    protected void triggerNonResolve(String nodeId) throws TimeoutException {
        mTestHost.triggerNonResolve(nodeId);
    }

    /**
     * Waits for the selected text string to be the given string, and asserts.
     *
     * @param text The string to wait for the selection to become.
     */
    protected void waitForSelectionToBe(final String text) {
        mTestHost.waitForSelectionToBe(text);
    }

    /**
     * Asserts that the action bar does or does not become visible in response to a selection.
     *
     * @param visible Whether the Action Bar must become visible or not.
     */
    protected void assertWaitForSelectActionBarVisible(final boolean visible) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            getSelectionPopupController().isSelectActionBarShowing(),
                            Matchers.is(visible));
                },
                TEST_TIMEOUT,
                DEFAULT_POLLING_INTERVAL);
    }

    protected SelectionPopupController getSelectionPopupController() {
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
        @SelectionEventType int dragStoppedEvent = SelectionEventType.SELECTION_HANDLE_DRAG_STOPPED;
        ThreadUtils.runOnUiThreadBlocking(
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
     * Runs the given Runnable in the main thread.
     *
     * @param runnable The Runnable.
     */
    public void runOnMainSync(Runnable runnable) {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(runnable);
    }

    // ============================================================================================
    // Fake Searches Helpers
    // ============================================================================================

    /**
     * Simulates a non-resolving search.
     *
     * @param nodeId The id of the node to be triggered.
     */
    protected void simulateNonResolveSearch(String nodeId)
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
    protected FakeResolveSearch simulateResolveSearch(String nodeId)
            throws InterruptedException, TimeoutException {
        return simulateResolvableSearchAndAssertResolveAndPreload(nodeId, true);
    }

    /** Simulates a resolve search on the default node on the page. */
    protected void simulateResolveSearch() throws Exception {
        simulateResolveSearch(SEARCH_NODE);
    }

    /**
     * Simulates a resolve-triggering gesture that may or may not actually resolve. If the gesture
     * should Resolve, the resolve and preload are asserted, and vice versa.
     *
     * @param nodeId The id of the node to be tapped.
     * @param isResolveExpected Whether a resolve is expected or not. Enforce by asserting.
     */
    protected FakeResolveSearch simulateResolvableSearchAndAssertResolveAndPreload(
            String nodeId, boolean isResolveExpected)
            throws InterruptedException, TimeoutException {
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
    protected void simulateSlowResolveSearch(String nodeId)
            throws InterruptedException, TimeoutException {
        mLatestSlowResolveSearch = mFakeServer.getFakeSlowResolveSearch(nodeId);
        assertNotNull(
                "Could not find FakeSlowResolveSearch for node ID:" + nodeId,
                mLatestSlowResolveSearch);
        mLatestSlowResolveSearch.simulate();
        waitForPanelToPeek();
    }

    /**
     * Simulates a slow response for the most recent {@link FakeSlowResolveSearch} set up by calling
     * simulateSlowResolveSearch.
     */
    protected void simulateSlowResolveFinished() throws InterruptedException, TimeoutException {
        // Allow the slow Resolution to finish, waiting for it to complete.
        mLatestSlowResolveSearch.finishResolve();
        assertLoadedSearchTermMatches(mLatestSlowResolveSearch.getSearchTerm());
    }

    /** Registers all fake searches to be used in tests. */
    private void registerFakeSearches() throws Exception {
        mFakeServer.registerFakeSearches();
    }

    // ============================================================================================
    // Fake Response
    // TODO(donnd): remove these methods and use the new infrastructure instead.
    // ============================================================================================

    /** Posts a fake response on the Main thread. */
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
    protected void fakeResponse(
            boolean isNetworkUnavailable,
            int responseCode,
            String searchTerm,
            String displayText,
            String alternateTerm,
            boolean doPreventPreload) {
        fakeResponse(
                new ResolvedSearchTerm.Builder(
                                isNetworkUnavailable,
                                responseCode,
                                searchTerm,
                                displayText,
                                alternateTerm,
                                doPreventPreload)
                        .build());
    }

    /**
     * Fakes a server response with the parameters given. {@See
     * ContextualSearchManager#handleSearchTermResolutionResponse}.
     */
    protected void fakeResponse(ResolvedSearchTerm resolvedSearchTerm) {
        if (mFakeServer.getSearchTermRequested() != null) {
            InstrumentationRegistry.getInstrumentation()
                    .runOnMainSync(new FakeResponseOnMainThread(resolvedSearchTerm));
        }
    }

    // ============================================================================================
    // Content Helpers
    // ============================================================================================

    /**
     * @return The Panel's WebContents.
     */
    protected WebContents getPanelWebContents() {
        return mPanel.getWebContents();
    }

    /**
     * @return Whether the Panel's WebContents is visible.
     */
    private boolean isWebContentsVisible() {
        return mFakeServer.isContentVisible();
    }

    /** Asserts that the Panel's WebContents is created. */
    protected void assertWebContentsCreated() {
        Assert.assertNotNull(getPanelWebContents());
    }

    /** Asserts that the Panel's WebContents is not created. */
    protected void assertNoWebContents() {
        Assert.assertNull(getPanelWebContents());
    }

    /** Asserts that the Panel's WebContents is visible. */
    protected void assertWebContentsVisible() {
        Assert.assertTrue(isWebContentsVisible());
    }

    /** Asserts that the Panel's WebContents was never shown. */
    protected void assertNeverCalledWebContentsOnShow() {
        Assert.assertFalse(mFakeServer.didEverCallWebContentsOnShow());
    }

    /** Asserts that the Panel's WebContents is created */
    protected void assertWebContentsCreatedButNeverMadeVisible() {
        assertWebContentsCreated();
        Assert.assertFalse(isWebContentsVisible());
        assertNeverCalledWebContentsOnShow();
    }

    // ============================================================================================
    // Assertions for different states
    // ============================================================================================

    void assertPeekingPanelNonResolve() {
        assertLoadedNoUrl();
    }

    // TODO(donnd): flesh out the assertions below.
    void assertClosedPanelNonResolve() {}

    void assertPanelNeverOpened() {
        // Check that we recorded a histogram entry for not-seen.
    }

    void assertPeekingPanelResolve() {
        assertLoadedLowPriorityUrl();
    }

    /** Asserts that the expanded panel did a resolve for the given {@code searchTerm}. */
    void assertExpandedPanelResolve(String searchTerm) {
        assertLoadedSearchTermMatches(searchTerm);
    }

    void assertExpandedPanelNonResolve() {
        assertSearchTermNotRequested();
    }

    void assertClosedPanelResolve() {}

    // ============================================================================================

    /**
     * Fakes navigation of the Content View to the URL that was previously requested.
     *
     * @param isFailure whether the request resulted in a failure.
     */
    protected void fakeContentViewDidNavigate(boolean isFailure) {
        String url = mFakeServer.getLoadedUrl();
        mManager.getOverlayPanelContentDelegate()
                .onMainFrameNavigation(url, false, isFailure, false);
    }

    /**
     * Simulates a click on the given word node. Waits for the bar to peek. TODO(donnd): rename to
     * include the waitForPanelToPeek semantic, or rename clickNode to clickNodeWithoutWaiting.
     *
     * @param nodeId A string containing the node ID.
     */
    protected void clickWordNode(String nodeId) throws TimeoutException {
        clickNode(nodeId);
        waitForPanelToFreshlyPeek();
    }

    /**
     * Simulates a simple gesture that could trigger a resolve on the given node in the given tab.
     *
     * @param tab The tab that contains the node to trigger (must be frontmost).
     * @param nodeId A string containing the node ID.
     */
    public void triggerNode(Tab tab, String nodeId) throws TimeoutException {
        DOMUtils.longPressNode(tab.getWebContents(), nodeId);
    }

    /**
     * @return The selected text.
     */
    protected String getSelectedText() {
        return mSelectionController.getSelectedText();
    }

    /**
     * Asserts that the loaded search term matches the provided value.
     *
     * @param searchTerm The provided search term.
     */
    protected void assertLoadedSearchTermMatches(String searchTerm) {
        boolean doesMatch = false;
        String loadedUrl = mFakeServer.getLoadedUrl();
        doesMatch = loadedUrl != null && loadedUrl.contains("q=" + searchTerm);
        String message =
                loadedUrl == null ? "but there was no loaded URL!" : "in URL: " + loadedUrl;
        Assert.assertTrue(
                "Expected to find searchTerm '" + searchTerm + "', " + message, doesMatch);
    }

    /** Asserts that the given parameters are present in the most recently loaded URL. */
    protected void assertContainsParameters(String... terms) {
        Assert.assertNotNull("Fake server didn't load a SERP URL", mFakeServer.getLoadedUrl());
        for (String term : terms) {
            Assert.assertTrue(
                    "Expected search term not found:" + term,
                    mFakeServer.getLoadedUrl().contains(term));
        }
    }

    /** Asserts that a Search Term has been requested. */
    protected void assertSearchTermRequested() {
        Assert.assertNotNull(mFakeServer.getSearchTermRequested());
    }

    /** Asserts that there has not been any Search Term requested. */
    private void assertSearchTermNotRequested() {
        Assert.assertNull(mFakeServer.getSearchTermRequested());
    }

    /** Asserts that the panel is currently closed or in an undefined state. */
    void assertPanelClosedOrUndefined() {
        boolean success = false;
        if (mPanel == null) {
            success = true;
        } else {
            @PanelState int panelState = mPanel.getPanelState();
            success = panelState == PanelState.CLOSED || panelState == PanelState.UNDEFINED;
        }
        Assert.assertTrue(
                "Expected the panel to be closed or undefined but it was in state: "
                        + mPanel.getPanelState(),
                success);
    }

    /** Asserts that no URL has been loaded in the Overlay Panel. */
    protected void assertLoadedNoUrl() {
        Assert.assertTrue(
                "Requested a search or preload when none was expected!",
                mFakeServer.getLoadedUrl() == null);
    }

    /** Asserts that a low-priority URL has been loaded in the Overlay Panel. */
    protected void assertLoadedLowPriorityUrl() {
        String message =
                "Expected a low priority search request URL, but got "
                        + (mFakeServer.getLoadedUrl() != null
                                ? mFakeServer.getLoadedUrl()
                                : "null");
        Assert.assertTrue(
                message,
                mFakeServer.getLoadedUrl() != null
                        && mFakeServer.getLoadedUrl().contains(LOW_PRIORITY_SEARCH_ENDPOINT));
        Assert.assertTrue(
                "Low priority request does not have the required prefetch parameter!",
                mFakeServer.getLoadedUrl() != null
                        && mFakeServer.getLoadedUrl().contains(CONTEXTUAL_SEARCH_PREFETCH_PARAM));
    }

    /**
     * Asserts that a low-priority URL that is intentionally invalid has been loaded in the Overlay
     * Panel (in order to produce an error).
     */
    protected void assertLoadedLowPriorityInvalidUrl() {
        String message =
                "Expected a low priority invalid search request URL, but got "
                        + String.valueOf(mFakeServer.getLoadedUrl());
        Assert.assertTrue(
                message,
                mFakeServer.getLoadedUrl() != null
                        && mFakeServer
                                .getLoadedUrl()
                                .contains(LOW_PRIORITY_INVALID_SEARCH_ENDPOINT));
        Assert.assertTrue(
                "Low priority request does not have the required prefetch parameter!",
                mFakeServer.getLoadedUrl() != null
                        && mFakeServer.getLoadedUrl().contains(CONTEXTUAL_SEARCH_PREFETCH_PARAM));
    }

    /** Asserts that a normal priority URL has been loaded in the Overlay Panel. */
    protected void assertLoadedNormalPriorityUrl() {
        String message =
                "Expected a normal priority search request URL, but got "
                        + (mFakeServer.getLoadedUrl() != null
                                ? mFakeServer.getLoadedUrl()
                                : "null");
        Assert.assertTrue(
                message,
                mFakeServer.getLoadedUrl() != null
                        && mFakeServer.getLoadedUrl().contains(NORMAL_PRIORITY_SEARCH_ENDPOINT));
        Assert.assertTrue(
                "Normal priority request should not have the prefetch parameter, but did!",
                mFakeServer.getLoadedUrl() != null
                        && !mFakeServer.getLoadedUrl().contains(CONTEXTUAL_SEARCH_PREFETCH_PARAM));
    }

    /**
     * Waits for a Normal priority URL to be loaded, or asserts that the load never happened. This
     * is needed when we test with a live internet connection and an invalid url fails to load (as
     * expected. See crbug.com/682953 for background.
     */
    protected void waitForNormalPriorityUrlLoaded() {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(mFakeServer.getLoadedUrl(), Matchers.notNullValue());
                    Criteria.checkThat(
                            mFakeServer.getLoadedUrl(),
                            Matchers.containsString(NORMAL_PRIORITY_SEARCH_ENDPOINT));
                },
                TEST_TIMEOUT,
                DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Asserts that no URLs have been loaded in the Overlay Panel since the last {@link
     * ContextualSearchFakeServer#reset}.
     */
    protected void assertNoSearchesLoaded() {
        Assert.assertEquals(0, mFakeServer.getLoadedUrlCount());
        assertLoadedNoUrl();
    }

    /**
     * Asserts that a Search Term has been requested.
     *
     * @param isExactResolve Whether the Resolve request must be exact (non-expanding).
     */
    protected void assertExactResolve(boolean isExactResolve) {
        Assert.assertEquals(isExactResolve, mFakeServer.getIsExactResolve());
    }

    /**
     * Waits for the Search Panel (the Search Bar) to peek up from the bottom, and asserts that it
     * did peek.
     */
    protected void waitForPanelToPeek() {
        waitForPanelToEnterState(PanelState.PEEKED);
    }

    /**
     * Waits for the Search Panel (the Search Bar) to peek up from the bottom, and asserts that it
     * did peek. Ignores an existing Search Panel that peeked.
     */
    protected void waitForPanelToFreshlyPeek() {
        int lastPeekSequence = mPanel.getLastPeekSequence();
        final @PanelState int state = PanelState.PEEKED;
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mPanel, Matchers.notNullValue());
                    Criteria.checkThat(mPanel.getPanelState(), Matchers.is(state));
                    Criteria.checkThat(
                            mPanel.getLastPeekSequence(), Matchers.not(lastPeekSequence));
                    Criteria.checkThat(mPanel.isHeightAnimationRunning(), Matchers.is(false));
                },
                TEST_TIMEOUT,
                DEFAULT_POLLING_INTERVAL);
    }

    /** Waits for the Search Panel to expand, and asserts that it did expand. */
    protected void waitForPanelToExpand() {
        waitForPanelToEnterState(PanelState.EXPANDED);
    }

    /** Waits for the Search Panel to maximize, and asserts that it did maximize. */
    protected void waitForPanelToMaximize() {
        waitForPanelToEnterState(PanelState.MAXIMIZED);
    }

    /** Waits for the Search Panel to close, and asserts that it did close. */
    protected void waitForPanelToClose() {
        waitForPanelToEnterState(PanelState.CLOSED);
    }

    /**
     * Waits for the Search Panel to enter the given {@code PanelState} and assert.
     *
     * @param state The {@link PanelState} to wait for.
     */
    private void waitForPanelToEnterState(final @PanelState int state) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mPanel, Matchers.notNullValue());
                    Criteria.checkThat(mPanel.getPanelState(), Matchers.is(state));
                    Criteria.checkThat(mPanel.isHeightAnimationRunning(), Matchers.is(false));
                },
                TEST_TIMEOUT,
                DEFAULT_POLLING_INTERVAL);
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
     *     should not change the panel state.
     */
    protected void assertPanelStillInState(final @PanelState int initialState)
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
    protected void waitForGestureToClosePanelAndAssertNoSelection() {
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
                () -> mSelectionController.isSelectionEmpty(),
                "Selection never empty.",
                TEST_TIMEOUT,
                DEFAULT_POLLING_INTERVAL);
    }

    /** Waits for the panel to close and then waits for the selection to dissolve. */
    protected void waitForPanelToCloseAndSelectionEmpty() {
        waitForPanelToClose();
        waitForSelectionEmpty();
    }

    protected void waitToPreventDoubleTapRecognition() throws InterruptedException {
        // Avoid issues with double-tap detection by ensuring sequential taps
        // aren't treated as such. Double-tapping can also select words much as
        // longpress, in turn showing the pins and preventing contextual tap
        // refinement from nearby taps. The double-tap timeout is sufficiently
        // short that this shouldn't conflict with tap refinement by the user.
        int doubleTapTimeout = ViewConfiguration.getDoubleTapTimeout();
        Thread.sleep(doubleTapTimeout * ((long) DOUBLE_TAP_DELAY_MULTIPLIER));
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
        TouchCommon.dragTo(
                sActivityTestRule.getActivity(),
                dragStartX,
                dragEndX,
                dragStartY,
                dragEndY,
                stepCount,
                downTime);
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
        TouchCommon.dragTo(
                sActivityTestRule.getActivity(),
                dragStartX,
                dragEndX,
                dragStartY,
                dragEndY,
                halfCount,
                downTime);
        // Generate events in the stationary end position in order to simulate a "pause" in
        // the movement, therefore preventing this gesture from being interpreted as a fling.
        TouchCommon.dragTo(
                sActivityTestRule.getActivity(),
                dragEndX,
                dragEndX,
                dragEndY,
                dragEndY,
                halfCount,
                downTime);
        TouchCommon.dragEnd(sActivityTestRule.getActivity(), dragEndX, dragEndY, downTime);
    }

    /** Flings the panel up to its expanded state. */
    protected void flingPanelUp() {
        fling(0.5f, 0.95f, 0.5f, 0.55f, 1000);
    }

    /** Swipes the panel down to its peeked state. */
    protected void swipePanelDown() {
        swipe(0.5f, 0.55f, 0.5f, 0.95f, 1000);
    }

    /** Scrolls the base page. */
    protected void scrollBasePage() {
        fling(0.f, 0.75f, 0.f, 0.7f, 100);
    }

    /** Taps the base page near the top. */
    protected void tapBasePageToClosePanel() {
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

    /** Taps the base page at the given x, y position. */
    private void tapBasePage(float x, float y) {
        View root = sActivityTestRule.getActivity().getWindow().getDecorView().getRootView();
        x *= root.getWidth();
        y *= root.getHeight();
        TouchCommon.singleClickView(root, (int) x, (int) y);
    }

    /** Expands the panel by directly asking the panel to expand. */
    protected void expandPanel() throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPanel.notifyBarTouched(0);
                    if (mFakeServer.getContentsObserver() != null) {
                        mFakeServer.getContentsObserver().onVisibilityChanged(Visibility.VISIBLE);
                    }
                    mPanel.animatePanelToState(
                            PanelState.EXPANDED,
                            StateChangeReason.UNKNOWN,
                            PANEL_INTERACTION_RETRY_DELAY_MS);
                    float tapX = (mPanel.getOffsetX() + mPanel.getWidth()) / 2f;
                    float tapY = (mPanel.getOffsetY() + mPanel.getBarContainerHeight()) / 2f;
                    mPanel.handleBarClick(tapX, tapY);
                });
    }

    /** Expands the panel and asserts that it did actually expand. */
    protected void expandPanelAndAssert() throws TimeoutException {
        expandPanel();
        waitForPanelToExpand();
    }

    /** Force the Panel to peek. */
    protected void peekPanel() {
        // TODO(donnd): use a consistent method of running these test tasks, and it's probably
        // best to use ThreadUtils.runOnUiThreadBlocking as done elsewhere in this file.
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            mPanel.peekPanel(StateChangeReason.UNKNOWN);
                        });
        waitForPanelToPeek();
    }

    /** Force the Panel to maximize, and wait for it to do so. */
    protected void maximizePanel() {
        // TODO(donnd): use a consistent method of running these test tasks, and it's probably
        // best to use ThreadUtils.runOnUiThreadBlocking as done elsewhere in this file.
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            mPanel.maximizePanel(StateChangeReason.UNKNOWN);
                        });
        waitForPanelToMaximize();
    }

    /** Fakes a response to the Resolve request. */
    protected void fakeAResponse() {
        fakeResponse(false, 200, "states", "United States Intelligence", "alternate-term", false);
        waitForPanelToPeek();
        assertLoadedLowPriorityUrl();
        assertContainsParameters("states", "alternate-term");
    }

    /** Force the Panel to handle a click on open-in-a-new-tab icon. */
    protected void forceOpenTabIconClick() {
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            mPanel.handleBarClick(
                                    mPanel.getOpenTabIconX() + mPanel.getOpenTabIconDimension() / 2,
                                    mPanel.getBarHeight() / 2);
                        });
    }

    /** Force the Panel to close. */
    protected void closePanel() {
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            mPanel.closePanel(StateChangeReason.UNKNOWN, false);
                        });
    }

    /** Waits for the Action Bar to be visible in response to a selection. */
    protected void waitForSelectActionBarVisible() {
        assertWaitForSelectActionBarVisible(true);
    }

    /** Updates Read Aloud Controller's active playback tab. */
    protected void changeReadAloudActivePlaybackTab() {
        ReadAloudController readAloudController =
                sActivityTestRule.getActivity().getReadAloudControllerForTesting();

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        readAloudController.setActivePlaybackTab(
                                sActivityTestRule.getActivity().getActivityTab()));
    }
}
