// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.net.Uri;
import android.support.test.InstrumentationRegistry;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.compositor.bottombar.OverlayContentDelegate;
import org.chromium.chrome.browser.compositor.bottombar.OverlayContentProgressObserver;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelContent;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelContentFactory;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.TimeoutException;

/**
 * Implements a fake Contextual Search server, for testing purposes.
 * TODO(donnd): add more functionality to this class once the overall approach has been validated.
 * TODO(donnd): rename this class when we refactor and rename the interface it implements.  Should
 *              be something like ContextualSearchFakeEnvironment.
 */
@VisibleForTesting
class ContextualSearchFakeServer
        implements ContextualSearchNetworkCommunicator, OverlayPanelContentFactory {
    private static final String SIMPLE_SERP_URL =
            "/chrome/test/data/android/contextualsearch/simple_serp.html";

    static final long LOGGED_EVENT_ID = 1L << 50; // Arbitrary value larger than 32 bits.

    private final ContextualSearchPolicy mPolicy;

    private final ContextualSearchTestHost mTestHost;
    private final ContextualSearchNetworkCommunicator mBaseManager;

    private final OverlayContentDelegate mContentDelegate;
    private final OverlayContentProgressObserver mProgressObserver;
    private final ChromeActivity mActivity;

    private final ArrayList<String> mRemovedUrls = new ArrayList<String>();

    private final Map<String, FakeResolveSearch> mFakeResolveSearches = new HashMap<>();
    private final Map<String, FakeNonResolveSearch> mFakeNonResolveSearches = new HashMap<>();
    private final Map<String, FakeSlowResolveSearch> mFakeSlowResolveSearches = new HashMap<>();

    private FakeResolveSearch mActiveResolveSearch;

    private String mLoadedUrl;
    private int mLoadedUrlCount;
    private boolean mUseInvalidLowPriorityPath;
    private boolean mActuallyLoadALiveSerp;

    private String mSearchTermRequested;
    private boolean mIsOnline = true;
    private boolean mIsExactResolve;
    private ContextualSearchContext mSearchContext;

    private boolean mDidEverCallWebContentsOnShow;

    interface ContextualSearchTestHost {
        /**
         * Simulates a non-resolve trigger on the given node and waits for the panel to peek.
         * @param nodeId A string containing the node ID.
         */
        void triggerNonResolve(String nodeId) throws TimeoutException;

        /**
         * Simulates a resolving trigger on the given node but does not wait for the panel to peek.
         * @param nodeId A string containing the node ID.
         */
        void triggerResolve(String nodeId) throws TimeoutException;

        /**
         * Waits for the selected text string to be the given string, and asserts.
         * @param text The string to wait for the selection to become.
         */
        void waitForSelectionToBe(final String text);

        /**
         * Waits for the Search Term Resolution to become ready.
         * @param search A given FakeResolveSearch.
         */
        void waitForSearchTermResolutionToStart(final FakeResolveSearch search);

        /**
         * Waits for the Search Term Resolution to finish.
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
        }

        private boolean isVisible() {
            return mIsVisible;
        }

        @Override
        public void wasShown() {
            mIsVisible = true;
            mDidEverCallWebContentsOnShow = true;
        }

        @Override
        public void wasHidden() {
            mIsVisible = false;
        }
    };

    private ContentsObserver mContentsObserver;

    boolean isContentVisible() {
        return mContentsObserver.isVisible();
    }

    //============================================================================================
    // FakeSearch
    //============================================================================================

    /**
     * Abstract class that represents a fake contextual search.
     */
    public abstract class FakeSearch {
        private final String mNodeId;

        /**
         * @param nodeId The id of the node where the touch event will be simulated.
         */
        FakeSearch(String nodeId) {
            mNodeId = nodeId;
        }

        /**
         * Simulates a fake search.
         *
         * @throws InterruptedException
         * @throws TimeoutException
         */
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

    //============================================================================================
    // FakeNonResolveSearch
    //============================================================================================

    /**
     * Class that represents a fake non-resolve triggered contextual search.
     * Historically this was a long-press triggered search.
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

    //============================================================================================
    // FakeResolveSearch
    //============================================================================================

    /**
     * Class that represents a fake resolve-triggered contextual search.
     */
    public class FakeResolveSearch extends FakeSearch {
        protected final ResolvedSearchTerm mResolvedSearchTerm;

        boolean mDidStartResolution;
        boolean mDidFinishResolution;

        /**
         * @param nodeId                The id of the node where the touch event will be simulated.
         * @param resolvedSearchTerm    The details of the server's Resolve request response, which
         *                              tells us what to search for.
         */
        FakeResolveSearch(String nodeId, ResolvedSearchTerm resolvedSearchTerm) {
            super(nodeId);

            mResolvedSearchTerm = resolvedSearchTerm;
        }

        /**
         * @param nodeId                The id of the node where the touch event will be simulated.
         * @param searchTerm            The resolved search term.
         */
        FakeResolveSearch(String nodeId, String searchTerm) {
            this(nodeId,
                    new ResolvedSearchTerm.Builder(false, 200, searchTerm, searchTerm).build());
        }

        /**
         * @param nodeId                The id of the node where the touch event will be simulated.
         * @param isNetworkUnavailable  Whether the network is unavailable.
         * @param responseCode          The HTTP response code of the resolution.
         * @param searchTerm            The resolved search term.
         * @param displayText           The display text.
         */
        FakeResolveSearch(String nodeId, boolean isNetworkUnavailable, int responseCode,
                String searchTerm, String displayText) {
            this(nodeId,
                    new ResolvedSearchTerm
                            .Builder(isNetworkUnavailable, responseCode, searchTerm, displayText)
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

        /**
         * Notifies that a Search Term Resolution has started.
         */
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

        /**
         * Simulates a Search Term Resolution.
         */
        protected void simulateSearchTermResolution() {
            InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
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

    //============================================================================================
    // FakeResolveSearch
    //============================================================================================

    /**
     * Class that represents a fake resolve-triggered contextual search that is slow to resolve.
     */
    public class FakeSlowResolveSearch extends FakeResolveSearch {
        /**
         * @param nodeId                The id of the node where the touch event will be simulated.
         * @param resolvedSearchTerm    The details of the server's Resolve request response, which
         *                              tells us what to search for.
         */
        FakeSlowResolveSearch(String nodeId, ResolvedSearchTerm resolvedSearchTerm) {
            super(nodeId, resolvedSearchTerm);
        }

        /**
         * @param nodeId                The id of the node where the touch event will be simulated.
         * @param isNetworkUnavailable  Whether the network is unavailable.
         * @param responseCode          The HTTP response code of the resolution.
         * @param searchTerm            The resolved search term.
         * @param displayText           The display text.
         */
        FakeSlowResolveSearch(String nodeId, boolean isNetworkUnavailable, int responseCode,
                String searchTerm, String displayText) {
            this(nodeId,
                    new ResolvedSearchTerm
                            .Builder(isNetworkUnavailable, responseCode, searchTerm, displayText)
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
                throw new RuntimeException("Tried to simulate a slow resolving search when "
                        + "not resolving!");
            }
        }

        /**
         * Finishes the resolving of a slow-resolving search.
         * @throws InterruptedException
         * @throws TimeoutException
         */
        void finishResolve() throws InterruptedException, TimeoutException {
            // Simulate a Search Term Resolution.
            simulateSearchTermResolution();

            // Now wait for the simulated Search Term Resolution to finish.
            mTestHost.waitForSearchTermResolutionToFinish(this);
        }
    }

    //============================================================================================
    // OverlayPanelContentWrapper
    //============================================================================================

    /**
     * A wrapper around OverlayPanelContent to be used during tests.
     */
    public class OverlayPanelContentWrapper extends OverlayPanelContent {
        OverlayPanelContentWrapper(OverlayContentDelegate contentDelegate,
                OverlayContentProgressObserver progressObserver, ChromeActivity activity,
                float barHeight) {
            super(contentDelegate, progressObserver, activity, false, barHeight);
        }

        @Override
        public void loadUrl(String url, boolean shouldLoadImmediately) {
            if (mUseInvalidLowPriorityPath && isLowPriorityUrl(url)) {
                url = makeInvalidUrl(url);
            }
            mLoadedUrl = url;
            mLoadedUrlCount++;

            String urlToLoad = mActuallyLoadALiveSerp ? url : SIMPLE_SERP_URL;
            // TODO(donnd): make low priority if needed?
            super.loadUrl(urlToLoad, shouldLoadImmediately);
            mContentsObserver = new ContentsObserver(getWebContents());
        }

        @Override
        public void removeLastHistoryEntry(String url, long timeInMs) {
            // Override to prevent call to native code.
            mRemovedUrls.add(url);
        }

        /**
         * Creates an invalid version of the given URL.
         * @param baseUrl The URL to build upon / modify.
         * @return The same URL but with an invalid path.
         */
        private String makeInvalidUrl(String baseUrl) {
            return Uri.parse(baseUrl).buildUpon().appendPath("invalid").build().toString();
        }

        /**
         * @return Whether the given URL is a low-priority URL.
         */
        private boolean isLowPriorityUrl(String url) {
            // Just check if it's set up to prefetch.
            return url.contains("&pf=c");
        }
    }

    //============================================================================================
    // ContextualSearchFakeServer
    //============================================================================================

    /**
     * Constructs a fake Contextual Search server that will callback to the given baseManager.
     * @param baseManager The manager to call back to for server responses.
     */
    @VisibleForTesting
    ContextualSearchFakeServer(ContextualSearchPolicy policy, ContextualSearchTestHost testHost,
            ContextualSearchNetworkCommunicator baseManager, OverlayContentDelegate contentDelegate,
            OverlayContentProgressObserver progressObserver, ChromeActivity activity) {
        mPolicy = policy;

        mTestHost = testHost;
        mBaseManager = baseManager;

        mContentDelegate = contentDelegate;
        mProgressObserver = progressObserver;
        mActivity = activity;
    }

    @Override
    public OverlayPanelContent createNewOverlayPanelContent() {
        return new OverlayPanelContentWrapper(mContentDelegate, mProgressObserver, mActivity,
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
        return mDidEverCallWebContentsOnShow;
    }

    /**
     * Sets whether the device is currently online or not.
     */
    @VisibleForTesting
    void setIsOnline(boolean isOnline) {
        mIsOnline = isOnline;
    }

    /**
     * Resets the fake server's member data.
     */
    @VisibleForTesting
    void reset() {
        mLoadedUrl = null;
        mSearchTermRequested = null;
        mIsOnline = true;
        mLoadedUrlCount = 0;
        mUseInvalidLowPriorityPath = false;
        mActuallyLoadALiveSerp = false;
        mIsExactResolve = false;
        mSearchContext = null;
    }

    /**
     * Sets a flag to build low-priority paths that are invalid in order to test failover.
     */
    @VisibleForTesting
    void setLowPriorityPathInvalid() {
        mUseInvalidLowPriorityPath = true;
    }

    /**
     * Sets a flag to actually load a live Search Result Page in the Panel.
     */
    @VisibleForTesting
    void setActuallyLoadALiveSerp() {
        mActuallyLoadALiveSerp = true;
    }

    /**
     * @return Whether the most recent loadUrl was on an invalid path.
     */
    @VisibleForTesting
    boolean didAttemptLoadInvalidUrl() {
        return mUseInvalidLowPriorityPath && mLoadedUrl.contains("invalid");
    }

    @VisibleForTesting
    boolean getIsExactResolve() {
        return mIsExactResolve;
    }

    @VisibleForTesting
    ContextualSearchContext getSearchContext() {
        return mSearchContext;
    }

    //============================================================================================
    // History Removal Helpers
    //============================================================================================

    /**
     * @param url The URL to be checked.
     * @return Whether the given URL was removed from history.
     */
    public boolean hasRemovedUrl(String url) {
        return mRemovedUrls.contains(url);
    }

    //============================================================================================
    // ContextualSearchNetworkCommunicator
    //============================================================================================

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
    public boolean isOnline() {
        return mIsOnline;
    }

    @Override
    public void stopPanelContentsNavigation() {
        // Stub out stop() of the WebContents.
        // Navigation of the content in the overlay may have been faked in tests,
        // so stopping the WebContents navigation is unsafe.
    }

    @Override
    @Nullable
    public GURL getBasePageUrl() {
        GURL baseUrl = mBaseManager.getBasePageUrl();
        if (baseUrl != null) {
            // Return plain HTTP URLs so we can test that we don't give them our legacy privacy
            // exceptions.
            return new GURL(baseUrl.getSpec().replace("https://", "http://"));
        }
        return baseUrl;
    }

    //============================================================================================
    // Fake Searches Helpers
    //============================================================================================

    /**
     * Register fake searches that can be used in tests. Each fake search takes a node ID, which
     * represents the DOM node that will be touched. The node ID is also used as an ID for the
     * fake search of a given type (LongPress or Tap). This means that if you need different
     * behaviors you need to add new DOM nodes with different IDs in the test's HTML file.
     */
    public void registerFakeSearches() {
        registerFakeNonResolveSearch(new FakeNonResolveSearch("search", "Search"));
        registerFakeNonResolveSearch(new FakeNonResolveSearch("term", "Term"));
        registerFakeNonResolveSearch(new FakeNonResolveSearch("resolution", "Resolution"));

        registerFakeResolveSearch(new FakeResolveSearch("states", "States"));
        //     registerFakeResolveSearch(new FakeResolveSearch("states-near""StatesNear"));
        registerFakeResolveSearch(new FakeResolveSearch("search", "Search"));
        registerFakeResolveSearch(new FakeResolveSearch("term", "Term"));
        registerFakeResolveSearch(new FakeResolveSearch("resolution", "Resolution"));

        ResolvedSearchTerm germanSearchTerm =
                new ResolvedSearchTerm.Builder(false, 200, "Deutsche", "Deutsche")
                        .setContextLanguage("de")
                        .build();
        FakeResolveSearch germanFakeTapSearch = new FakeResolveSearch("german", germanSearchTerm);
        registerFakeResolveSearch(germanFakeTapSearch);

        // Setup the "intelligence" node to return Related Searches along with the usual result.
        ResolvedSearchTerm intelligenceWithRelatedSearches =
                new ResolvedSearchTerm.Builder(false, 200, "Intelligence", "Intelligence")
                        .setRelatedSearches(new String[] {"Related Search 1", "Related Search 2"})
                        .build();
        FakeResolveSearch fakeSearchWithRelatedSearches =
                new FakeResolveSearch("intelligence", intelligenceWithRelatedSearches);
        registerFakeResolveSearch(fakeSearchWithRelatedSearches);

        // Register a fake tap search that will fake a logged event ID from the server, when
        // a fake tap is done on the intelligence-logged-event-id element in the test file.
        ResolvedSearchTerm searchTermWithId =
                new ResolvedSearchTerm.Builder(false, 200, "Intelligence", "Intelligence")
                        .setLoggedEventId(LOGGED_EVENT_ID)
                        .build();
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
        registerFakeSlowResolveSearch(new FakeSlowResolveSearch(
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
        return mFakeResolveSearches.get(id);
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
     * @param fakeSearch The FakeNonResolveSearch to be registered.
     */
    private void registerFakeNonResolveSearch(FakeNonResolveSearch fakeSearch) {
        mFakeNonResolveSearches.put(fakeSearch.getNodeId(), fakeSearch);
    }

    /**
     * Register the FakeResolveSearch.
     * @param fakeSearch The FakeResolveSearch to be registered.
     */
    private void registerFakeResolveSearch(FakeResolveSearch fakeSearch) {
        mFakeResolveSearches.put(fakeSearch.getNodeId(), fakeSearch);
    }

    /**
     * Register the FakeSlowResolveSearch.
     * @param fakeSlowResolveSearch The {@code FakeSlowResolveSearch} to be registered.
     */
    private void registerFakeSlowResolveSearch(FakeSlowResolveSearch fakeSlowResolveSearch) {
        mFakeSlowResolveSearches.put(fakeSlowResolveSearch.getNodeId(), fakeSlowResolveSearch);
    }
}
