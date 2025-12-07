// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.test.platform.app.InstrumentationRegistry;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Assert;

import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelContent;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelContentDelegate;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelContentFactory;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelContentProgressObserver;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.TimeoutException;

/**
 * Implements a fake Contextual Search server, for testing purposes. TODO(donnd): rename this class
 * when we refactor and rename the interface it implements. Should be something like
 * ContextualSearchFakeEnvironment.
 */
@VisibleForTesting
class ContextualSearchFakeServer
        implements ContextualSearchNetworkCommunicator, OverlayPanelContentFactory {
    private final ContextualSearchPolicy mPolicy;

    private final ContextualSearchTestHost mTestHost;
    private final ContextualSearchNetworkCommunicator mBaseManager;

    private final OverlayPanelContentDelegate mContentDelegate;
    private final OverlayPanelContentProgressObserver mProgressObserver;
    private final ChromeActivity mActivity;

    private final Map<String, FakeResolveSearch> mFakeResolveSearches = new HashMap<>();
    private final Map<String, FakeNonResolveSearch> mFakeNonResolveSearches = new HashMap<>();
    private final Map<String, FakeSlowResolveSearch> mFakeSlowResolveSearches = new HashMap<>();

    private FakeResolveSearch mActiveResolveSearch;

    private String mLoadedUrl;
    private int mLoadedUrlCount;

    private String mSearchTermRequested;
    private boolean mIsExactResolve;
    private ContextualSearchContext mSearchContext;

    private boolean mDidEverShowWebContents;

    /** An expected search, to be returned by this fake server when non-null. */
    private FakeResolveSearch mExpectedFakeResolveSearch;

    /**
     * Provides access to the test host so this fake server can drive actions when simulating a
     * search.
     */
    interface ContextualSearchTestHost {
        /**
         * Simulates a non-resolve trigger on the given node and waits for the panel to peek.
         *
         * @param nodeId A string containing the node ID.
         */
        void triggerNonResolve(String nodeId) throws TimeoutException;

        /**
         * Simulates a resolving trigger on the given node but does not wait for the panel to peek.
         *
         * @param nodeId A string containing the node ID.
         */
        void triggerResolve(String nodeId) throws TimeoutException;

        /**
         * Waits for the selected text string to be the given string, and asserts.
         *
         * @param text The string to wait for the selection to become.
         */
        void waitForSelectionToBe(final String text);

        /**
         * Waits for the Search Term Resolution to become ready.
         *
         * @param search A given FakeResolveSearch.
         */
        void waitForSearchTermResolutionToStart(final FakeResolveSearch search);

        /**
         * Waits for the Search Term Resolution to finish.
         *
         * @param search A given FakeResolveSearch.
         */
        void waitForSearchTermResolutionToFinish(final FakeResolveSearch search);

        /**
         * @return The {@link ContextualSearchPanel}.
         */
        ContextualSearchPanel getPanel();
    }

    private class ContentsObserver extends WebContentsObserver {
        private boolean mIsVisible;

        private ContentsObserver(WebContents webContents) {
            super(webContents);
            onVisibilityChanged(webContents.getVisibility());
        }

        private boolean isVisible() {
            return mIsVisible;
        }

        @Override
        public void onVisibilityChanged(@Visibility int visibility) {
            mIsVisible = visibility == Visibility.VISIBLE;
            mDidEverShowWebContents |= mIsVisible;
        }
    }

    private ContentsObserver mContentsObserver;

    boolean isContentVisible() {
        return mContentsObserver.isVisible();
    }

    WebContentsObserver getContentsObserver() {
        return mContentsObserver;
    }

    // ============================================================================================
    // FakeSearch
    // ============================================================================================

    /** Abstract class that represents a fake contextual search. */
    public abstract static class FakeSearch {
        private final String mNodeId;

        /**
         * @param nodeId The id of the node where the touch event will be simulated.
         */
        FakeSearch(String nodeId) {
            mNodeId = nodeId;
        }

        /** Simulates a fake search. */
        public abstract void simulate() throws InterruptedException, TimeoutException;

        /**
         * @return The search term that will be used in the contextual search.
         */
        public abstract String getSearchTerm();

        /**
         * @return The id of the node where the touch event will be simulated.
         */
        public String getNodeId() {
            return mNodeId;
        }
    }

    // ============================================================================================
    // FakeNonResolveSearch
    // ============================================================================================

    /**
     * Class that represents a fake non-resolve triggered contextual search. Historically this was a
     * long-press triggered search.
     */
    public class FakeNonResolveSearch extends FakeSearch {
        private final String mSearchTerm;

        /**
         * @param nodeId The id of the node where the touch event will be simulated.
         * @param searchTerm The expected text that the node should contain.
         */
        FakeNonResolveSearch(String nodeId, String searchTerm) {
            super(nodeId);

            mSearchTerm = searchTerm;
        }

        @Override
        public void simulate() throws InterruptedException, TimeoutException {
            mTestHost.triggerNonResolve(getNodeId());
            mTestHost.waitForSelectionToBe(mSearchTerm);
        }

        @Override
        public String getSearchTerm() {
            return mSearchTerm;
        }
    }

    // ============================================================================================
    // FakeResolveSearch
    // ============================================================================================

    /** Class that represents a fake resolve-triggered contextual search. */
    public class FakeResolveSearch extends FakeSearch {
        protected final ResolvedSearchTerm mResolvedSearchTerm;

        boolean mDidStartResolution;
        boolean mDidFinishResolution;

        /**
         * @param nodeId The id of the node where the touch event will be simulated.
         * @param resolvedSearchTerm The details of the server's Resolve request response, which
         *     tells us what to search for.
         */
        FakeResolveSearch(String nodeId, ResolvedSearchTerm resolvedSearchTerm) {
            super(nodeId);

            mResolvedSearchTerm = resolvedSearchTerm;
        }

        /**
         * @param nodeId The id of the node where the touch event will be simulated.
         * @param searchTerm The resolved search term.
         */
        FakeResolveSearch(String nodeId, String searchTerm) {
            this(
                    nodeId,
                    new ResolvedSearchTerm.Builder(false, 200, searchTerm, searchTerm).build());
        }

        /**
         * @param nodeId The id of the node where the touch event will be simulated.
         * @param isNetworkUnavailable Whether the network is unavailable.
         * @param responseCode The HTTP response code of the resolution.
         * @param searchTerm The resolved search term.
         * @param displayText The display text.
         */
        FakeResolveSearch(
                String nodeId,
                boolean isNetworkUnavailable,
                int responseCode,
                String searchTerm,
                String displayText) {
            this(
                    nodeId,
                    new ResolvedSearchTerm.Builder(
                                    isNetworkUnavailable, responseCode, searchTerm, displayText)
                            .build());
        }

        @Override
        public void simulate() throws InterruptedException, TimeoutException {
            mActiveResolveSearch = this;

            // When a resolution is needed, the simulation does not start until the system
            // requests one, and it does not finish until the simulated resolution happens.
            mDidStartResolution = false;
            mDidFinishResolution = false;

            if (mPolicy.shouldPreviousGestureResolve()) {
                mTestHost.triggerResolve(getNodeId());
            } else {
                mTestHost.triggerNonResolve(getNodeId());
            }
            mTestHost.waitForSelectionToBe(getSearchTerm());

            if (mPolicy.shouldPreviousGestureResolve()) {
                // Now wait for the Search Term Resolution to start.
                mTestHost.waitForSearchTermResolutionToStart(this);

                // Simulate a Search Term Resolution.
                simulateSearchTermResolution();

                // Now wait for the simulated Search Term Resolution to finish.
                mTestHost.waitForSearchTermResolutionToFinish(this);
            } else {
                mDidFinishResolution = true;
            }
        }

        @Override
        public String getSearchTerm() {
            return mResolvedSearchTerm.searchTerm();
        }

        /** Notifies that a Search Term Resolution has started. */
        public void notifySearchTermResolutionStarted() {
            mDidStartResolution = true;
        }

        /**
         * @return Whether the Search Term Resolution has started.
         */
        public boolean didStartSearchTermResolution() {
            return mDidStartResolution;
        }

        /**
         * @return Whether the Search Term Resolution has finished.
         */
        public boolean didFinishSearchTermResolution() {
            return mDidFinishResolution;
        }

        /** Simulates a Search Term Resolution. */
        protected void simulateSearchTermResolution() {
            InstrumentationRegistry.getInstrumentation()
                    .runOnMainSync(
                            () -> {
                                assert didStartSearchTermResolution();
                                handleSearchTermResolutionResponse(mResolvedSearchTerm);

                                mActiveResolveSearch = null;
                                mDidFinishResolution = true;
                            });
        }

        ResolvedSearchTerm getResolvedSearchTerm() {
            return mResolvedSearchTerm;
        }
    }

    // ============================================================================================
    // FakeResolveSearch
    // ============================================================================================

    /** Class that represents a fake resolve-triggered contextual search that is slow to resolve. */
    public class FakeSlowResolveSearch extends FakeResolveSearch {
        /**
         * @param nodeId The id of the node where the touch event will be simulated.
         * @param resolvedSearchTerm The details of the server's Resolve request response, which
         *     tells us what to search for.
         */
        FakeSlowResolveSearch(String nodeId, ResolvedSearchTerm resolvedSearchTerm) {
            super(nodeId, resolvedSearchTerm);
        }

        /**
         * @param nodeId The id of the node where the touch event will be simulated.
         * @param isNetworkUnavailable Whether the network is unavailable.
         * @param responseCode The HTTP response code of the resolution.
         * @param searchTerm The resolved search term.
         * @param displayText The display text.
         */
        FakeSlowResolveSearch(
                String nodeId,
                boolean isNetworkUnavailable,
                int responseCode,
                String searchTerm,
                String displayText) {
            this(
                    nodeId,
                    new ResolvedSearchTerm.Builder(
                                    isNetworkUnavailable, responseCode, searchTerm, displayText)
                            .build());
        }

        @Override
        public void simulate() throws InterruptedException, TimeoutException {
            mActiveResolveSearch = this;

            // When a resolution is needed, the simulation does not start until the system
            // requests one, and it does not finish until the simulated resolution happens.
            mDidStartResolution = false;
            mDidFinishResolution = false;

            mTestHost.triggerResolve(getNodeId());
            mTestHost.waitForSelectionToBe(getSearchTerm());

            if (mPolicy.shouldPreviousGestureResolve()) {
                // Now wait for the Search Term Resolution to start.
                mTestHost.waitForSearchTermResolutionToStart(this);
            } else {
                throw new RuntimeException(
                        "Tried to simulate a slow resolving search when " + "not resolving!");
            }
        }

        /** Finishes the resolving of a slow-resolving search. */
        void finishResolve() throws InterruptedException, TimeoutException {
            // Simulate a Search Term Resolution.
            simulateSearchTermResolution();

            // Now wait for the simulated Search Term Resolution to finish.
            mTestHost.waitForSearchTermResolutionToFinish(this);
        }
    }

    // ============================================================================================
    // OverlayPanelContentWrapper
    // ============================================================================================

    /** A wrapper around OverlayPanelContent to be used during tests. */
    public class OverlayPanelContentWrapper extends OverlayPanelContent {
        OverlayPanelContentWrapper(
                OverlayPanelContentDelegate contentDelegate,
                OverlayPanelContentProgressObserver progressObserver,
                ChromeActivity activity,
                float barHeight) {
            super(
                    contentDelegate,
                    progressObserver,
                    activity,
                    ProfileProvider.getOrCreateProfile(
                            activity.getProfileProviderSupplier().get(), false),
                    barHeight,
                    activity.getCompositorViewHolderForTesting(),
                    activity.getWindowAndroid(),
                    activity::getActivityTab);
        }

        @Override
        public void loadUrl(String url, boolean shouldLoadImmediately) {
            mLoadedUrl = url;
            mLoadedUrlCount++;

            super.loadUrl(url, shouldLoadImmediately);
            mContentsObserver = new ContentsObserver(getWebContents());
        }
    }

    // ============================================================================================
    // ContextualSearchFakeServer
    // ============================================================================================

    /**
     * Constructs a fake Contextual Search server that will callback to the given baseManager.
     *
     * @param baseManager The manager to call back to for server responses.
     */
    @VisibleForTesting
    ContextualSearchFakeServer(
            ContextualSearchPolicy policy,
            ContextualSearchTestHost testHost,
            ContextualSearchNetworkCommunicator baseManager,
            OverlayPanelContentDelegate contentDelegate,
            OverlayPanelContentProgressObserver progressObserver,
            ChromeActivity activity) {
        mPolicy = policy;

        mTestHost = testHost;
        mBaseManager = baseManager;

        mContentDelegate = contentDelegate;
        mProgressObserver = progressObserver;
        mActivity = activity;
    }

    @Override
    public OverlayPanelContent createNewOverlayPanelContent() {
        return new OverlayPanelContentWrapper(
                mContentDelegate,
                mProgressObserver,
                mActivity,
                mTestHost.getPanel().getBarHeight());
    }

    /**
     * @return The search term requested, or {@code null} if no search term was requested.
     */
    @VisibleForTesting
    String getSearchTermRequested() {
        return mSearchTermRequested;
    }

    /**
     * @return the loaded search result page URL if any was requested.
     */
    @VisibleForTesting
    String getLoadedUrl() {
        return mLoadedUrl;
    }

    /**
     * @return The number of times we loaded a URL in the Content View.
     */
    @VisibleForTesting
    int getLoadedUrlCount() {
        return mLoadedUrlCount;
    }

    /**
     * @return Whether onShow() was ever called for the current {@code WebContents}.
     */
    @VisibleForTesting
    boolean didEverCallWebContentsOnShow() {
        return mDidEverShowWebContents;
    }

    /** Resets the fake server's member data. */
    @VisibleForTesting
    void reset() {
        mLoadedUrl = null;
        mSearchTermRequested = null;
        mLoadedUrlCount = 0;
        mIsExactResolve = false;
        mSearchContext = null;
        mExpectedFakeResolveSearch = null;
    }

    @VisibleForTesting
    boolean getIsExactResolve() {
        return mIsExactResolve;
    }

    @VisibleForTesting
    ContextualSearchContext getSearchContext() {
        return mSearchContext;
    }

    /**
     * Sets the result of the resolve request that this fake server is expected to return.
     *
     * @param nodeId the node that will trigger this resolve when selected.
     * @param resolvedSearchTermResponse the response from this fake server to return from the fake
     *     resolve request.
     */
    void setExpectations(String nodeId, ResolvedSearchTerm resolvedSearchTermResponse) {
        mExpectedFakeResolveSearch = new FakeResolveSearch(nodeId, resolvedSearchTermResponse);
    }

    // ============================================================================================
    // ContextualSearchNetworkCommunicator
    // ============================================================================================

    @Override
    public void startSearchTermResolutionRequest(
            String selection, boolean isExactResolve, ContextualSearchContext searchContext) {
        mLoadedUrl = null;
        mSearchTermRequested = selection;
        mIsExactResolve = isExactResolve;
        mSearchContext = searchContext;

        if (mActiveResolveSearch != null) {
            mActiveResolveSearch.notifySearchTermResolutionStarted();
        }
    }

    @Override
    public void handleSearchTermResolutionResponse(ResolvedSearchTerm resolvedSearchTerm) {
        mBaseManager.handleSearchTermResolutionResponse(resolvedSearchTerm);
    }

    @Override
    public void stopPanelContentsNavigation() {
        // Stub out stop() of the WebContents.
        // Navigation of the content in the overlay may have been faked in tests,
        // so stopping the WebContents navigation is unsafe.
    }

    @Override
    public @Nullable GURL getBasePageUrl() {
        GURL baseUrl = mBaseManager.getBasePageUrl();
        if (baseUrl != null) {
            // Return plain HTTP URLs so we can test that we don't give them our legacy privacy
            // exceptions.
            return new GURL(baseUrl.getSpec().replace("https://", "http://"));
        }
        return baseUrl;
    }

    // ============================================================================================
    // Fake Searches Helpers
    // ============================================================================================

    /**
     * Register fake searches that can be used in tests. Each fake search takes a node ID, which
     * represents the DOM node that will be touched. The node ID is also used as an ID for the fake
     * search of a given type (LongPress or Tap). This means that if you need different behaviors
     * you need to add new DOM nodes with different IDs in the test's HTML file.
     */
    public void registerFakeSearches() throws Exception {
        registerFakeNonResolveSearch(new FakeNonResolveSearch("search", "Search"));
        registerFakeNonResolveSearch(new FakeNonResolveSearch("term", "Term"));
        registerFakeNonResolveSearch(new FakeNonResolveSearch("resolution", "Resolution"));

        registerFakeResolveSearch(new FakeResolveSearch("states", "States"));
        //     registerFakeResolveSearch(new FakeResolveSearch("states-near""StatesNear"));
        registerFakeResolveSearch(new FakeResolveSearch("search", "Search"));
        registerFakeResolveSearch(new FakeResolveSearch("term", "Term"));
        registerFakeResolveSearch(new FakeResolveSearch("resolution", "Resolution"));

        // These resolved searches are effectively deprecated.
        // Use setExpectations() instead.
        ResolvedSearchTerm germanSearchTerm =
                new ResolvedSearchTerm.Builder(false, 200, "Deutsche", "Deutsche")
                        .setContextLanguage("de")
                        .build();
        FakeResolveSearch germanFakeTapSearch = new FakeResolveSearch("german", germanSearchTerm);
        registerFakeResolveSearch(germanFakeTapSearch);

        // Setup the "intelligence" node to return Related Searches along with the usual result.
        ResolvedSearchTerm intelligenceWithRelatedSearches =
                buildResolvedSearchTermWithRelatedSearches("Intelligence");
        FakeResolveSearch fakeSearchWithRelatedSearches =
                new FakeResolveSearch("intelligence", intelligenceWithRelatedSearches);
        registerFakeResolveSearch(fakeSearchWithRelatedSearches);

        // Register a fake tap search that will fake a logged event ID from the server, when
        // a fake tap is done on the intelligence-logged-event-id element in the test file.
        ResolvedSearchTerm searchTermWithId =
                new ResolvedSearchTerm.Builder(false, 200, "Intelligence", "Intelligence").build();
        FakeResolveSearch loggedIdFakeTapSearch =
                new FakeResolveSearch("intelligence-logged-event-id", searchTermWithId);
        registerFakeResolveSearch(loggedIdFakeTapSearch);

        // Register a resolving search of "States" that expands to "United States".
        ResolvedSearchTerm searchTermWithStartAdjust =
                new ResolvedSearchTerm.Builder(false, 200, "States", "States")
                        .setSelectionStartAdjust(-7)
                        .build();
        FakeSlowResolveSearch expandingStatesTapSearch =
                new FakeSlowResolveSearch("states", searchTermWithStartAdjust);
        registerFakeSlowResolveSearch(expandingStatesTapSearch);
        registerFakeSlowResolveSearch(
                new FakeSlowResolveSearch("search", false, 200, "Search", "Search"));
        registerFakeSlowResolveSearch(
                new FakeSlowResolveSearch(
                        "intelligence", false, 200, "Intelligence", "Intelligence"));
    }

    /**
     * @param id The ID of the FakeNonResolveSearch.
     * @return The FakeNonResolveSearch with the given ID.
     */
    public FakeNonResolveSearch getFakeNonResolveSearch(String id) {
        return mFakeNonResolveSearches.get(id);
    }

    /**
     * @param id The ID of the FakeResolveSearch.
     * @return The FakeResolveSearch with the given ID.
     */
    public FakeResolveSearch getFakeResolveSearch(String id) {
        if (mExpectedFakeResolveSearch != null) {
            Assert.assertEquals(
                    "The expectations node ID does not match the given node!",
                    mExpectedFakeResolveSearch.getNodeId(),
                    id);
            return mExpectedFakeResolveSearch;
        } else {
            return mFakeResolveSearches.get(id);
        }
    }

    /**
     * Returns a {@link ResolvedSearchTerm} build to include sample Related Searches that uses the
     * given string for the Search Term.
     *
     * @param searchTerm The string to use for the Search Term and Display Text.
     * @return a {@link ResolvedSearchTerm} that includes some sample Related Searches of all types.
     */
    public ResolvedSearchTerm buildResolvedSearchTermWithRelatedSearches(String searchTerm)
            throws JSONException {
        JSONObject rSearch1 = new JSONObject();
        rSearch1.put("title", "Related Search 1");
        JSONObject rSearch2 = new JSONObject();
        rSearch2.put("title", "Related Search 2");
        JSONObject rSearch3 = new JSONObject();
        rSearch3.put("title", "Related Search 3");
        JSONArray rSearches = new JSONArray();
        rSearches.put(rSearch1);
        rSearches.put(rSearch2);
        rSearches.put(rSearch3);
        JSONObject suggestions = new JSONObject();
        suggestions.put("content", rSearches);
        // Also add selection suggestions, which are shown in the Bar, so we can exercise that code.
        JSONObject rBar1 = new JSONObject();
        rBar1.put("title", "Selection Related 1");
        JSONObject rBar2 = new JSONObject();
        rBar2.put("title", "Selection Related 2");
        JSONObject rBar3 = new JSONObject();
        rBar3.put("title", "Selection Related 3");
        JSONArray selectionSearches = new JSONArray();
        selectionSearches.put(rBar1);
        selectionSearches.put(rBar2);
        selectionSearches.put(rBar3);
        suggestions.put("selection", selectionSearches);
        return new ResolvedSearchTerm.Builder(false, 200, searchTerm, searchTerm)
                .setRelatedSearchesJson(suggestions.toString())
                .build();
    }

    /**
     * @param id The ID of the FakeSlowResolveSearch.
     * @return The {@code FakeSlowResolveSearch} with the given ID.
     */
    public FakeSlowResolveSearch getFakeSlowResolveSearch(String id) {
        return mFakeSlowResolveSearches.get(id);
    }

    /**
     * Register the FakeNonResolveSearch.
     *
     * @param fakeSearch The FakeNonResolveSearch to be registered.
     */
    private void registerFakeNonResolveSearch(FakeNonResolveSearch fakeSearch) {
        mFakeNonResolveSearches.put(fakeSearch.getNodeId(), fakeSearch);
    }

    /**
     * Register the FakeResolveSearch.
     *
     * @param fakeSearch The FakeResolveSearch to be registered.
     */
    private void registerFakeResolveSearch(FakeResolveSearch fakeSearch) {
        mFakeResolveSearches.put(fakeSearch.getNodeId(), fakeSearch);
    }

    /**
     * Register the FakeSlowResolveSearch.
     *
     * @param fakeSlowResolveSearch The {@code FakeSlowResolveSearch} to be registered.
     */
    private void registerFakeSlowResolveSearch(FakeSlowResolveSearch fakeSlowResolveSearch) {
        mFakeSlowResolveSearches.put(fakeSlowResolveSearch.getNodeId(), fakeSlowResolveSearch);
    }
}
