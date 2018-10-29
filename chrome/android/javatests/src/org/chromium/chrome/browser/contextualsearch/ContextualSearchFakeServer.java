// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.net.Uri;
import android.support.annotation.Nullable;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.bottombar.OverlayContentDelegate;
import org.chromium.chrome.browser.compositor.bottombar.OverlayContentProgressObserver;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelContent;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelContentFactory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

import java.net.MalformedURLException;
import java.net.URL;
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
    static final long ARBITRARY_DOC_ID = -12345678901234l;
    static final long ARBITRARY_SNIPPET_HASH = 12345678901234567l;

    private final ContextualSearchPolicy mPolicy;

    private final ContextualSearchManagerTest mManagerTest;
    private final ContextualSearchNetworkCommunicator mBaseManager;

    private final OverlayContentDelegate mContentDelegate;
    private final OverlayContentProgressObserver mProgressObserver;
    private final ChromeActivity mActivity;

    private final ArrayList<String> mRemovedUrls = new ArrayList<String>();

    private final Map<String, FakeTapSearch> mFakeTapSearches = new HashMap<>();
    private final Map<String, FakeLongPressSearch> mFakeLongPressSearches = new HashMap<>();
    private final Map<String, FakeSlowResolveSearch> mFakeSlowResolveSearches = new HashMap<>();

    private FakeTapSearch mActiveFakeTapSearch;

    private String mLoadedUrl;
    private int mLoadedUrlCount;
    private boolean mUseInvalidLowPriorityPath;

    private String mSearchTermRequested;
    private boolean mShouldUseHttps;
    private boolean mIsOnline = true;

    private boolean mDidEverCallWebContentsOnShow;

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
    // FakeLongPressSearch
    //============================================================================================

    /**
     * Class that represents a fake long-press triggered contextual search.
     */
    public class FakeLongPressSearch extends FakeSearch {
        private final String mSearchTerm;

        /**
         * @param nodeId The id of the node where the touch event will be simulated.
         * @param searchTerm The expected text that the node should contain.
         */
        FakeLongPressSearch(String nodeId, String searchTerm) {
            super(nodeId);

            mSearchTerm = searchTerm;
        }

        @Override
        public void simulate() throws InterruptedException, TimeoutException {
            mManagerTest.longPressNode(getNodeId());
            mManagerTest.waitForSelectionToBe(mSearchTerm);
        }

        @Override
        public String getSearchTerm() {
            return mSearchTerm;
        }
    }

    //============================================================================================
    // FakeTapSearch
    //============================================================================================

    /**
     * Class that represents a fake tap triggered contextual search.
     */
    public class FakeTapSearch extends FakeSearch {
        private final boolean mIsNetworkUnavailable;
        private final int mResponseCode;
        protected final String mSearchTerm;
        private final String mDisplayText;
        private final String mAlternateTerm;
        private final String mMid;
        private final boolean mDoPreventPreload;
        private final int mStartAdjust;
        private final int mEndAdjust;
        private final String mContextLanguage;
        private final String mThumbnailUrl;
        private final String mCaption;
        private final String mQuickActionUri;
        private final int mQuickActionCategory;
        private final long mDocId;
        private final long mSnippetHash;

        boolean mDidStartResolution;
        boolean mDidFinishResolution;

        /**
         * @param nodeId                The id of the node where the touch event will be simulated.
         * @param isNetworkUnavailable  Whether the network is unavailable.
         * @param responseCode          The HTTP response code of the resolution.
         * @param searchTerm            The resolved search term.
         * @param displayText           The display text.
         * @param alternateTerm         The alternate text.
         * @param mid                   The MID to specify a KP, or an empty string.
         * @param doPreventPreload      Whether search preload should be prevented.
         * @param startAdjust           The start adjustment of the selection.
         * @param endAdjust             The end adjustment of the selection.
         * @param contextLanguage       The language of the context determined by the server.
         * @param thumbnailUrl          The URL of a thumbnail to display.
         * @param caption               The caption to display.
         * @param quickActionUri        The URI for the intent associated with the quick action.
         * @param quickActionCategory   The category for the quick action.
         * @param docId                 A document ID from the Search crawl.
         * @param snippetHash           A long hash of some characters tapped.
         */
        FakeTapSearch(String nodeId, boolean isNetworkUnavailable, int responseCode,
                String searchTerm, String displayText, String alternateTerm, String mid,
                boolean doPreventPreload, int startAdjust, int endAdjust, String contextLanguage,
                String thumbnailUrl, String caption, String quickActionUri, int quickActionCategory,
                long docId, long snippetHash) {
            super(nodeId);

            mIsNetworkUnavailable = isNetworkUnavailable;
            mResponseCode = responseCode;
            mSearchTerm = searchTerm;
            mDisplayText = displayText;
            mAlternateTerm = alternateTerm;
            mMid = mid;
            mDoPreventPreload = doPreventPreload;
            mStartAdjust = startAdjust;
            mEndAdjust = endAdjust;
            mContextLanguage = contextLanguage;
            mThumbnailUrl = thumbnailUrl;
            mCaption = caption;
            mQuickActionUri = quickActionUri;
            mQuickActionCategory = quickActionCategory;
            mDocId = docId;
            mSnippetHash = snippetHash;
        }

        @Override
        public void simulate() throws InterruptedException, TimeoutException {
            mActiveFakeTapSearch = this;

            // When a resolution is needed, the simulation does not start until the system
            // requests one, and it does not finish until the simulated resolution happens.
            mDidStartResolution = false;
            mDidFinishResolution = false;

            mManagerTest.clickNode(getNodeId());
            mManagerTest.waitForSelectionToBe(mSearchTerm);

            if (mPolicy.shouldPreviousTapResolve()) {
                // Now wait for the Search Term Resolution to start.
                mManagerTest.waitForSearchTermResolutionToStart(this);

                // Simulate a Search Term Resolution.
                simulateSearchTermResolution();

                // Now wait for the simulated Search Term Resolution to finish.
                mManagerTest.waitForSearchTermResolutionToFinish(this);
            } else {
                mDidFinishResolution = true;
            }
        }

        @Override
        public String getSearchTerm() {
            return mSearchTerm;
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
            mManagerTest.runOnMainSync(getRunnable());
        }

        /**
         * @return A Runnable to handle the fake Search Term Resolution.
         */
        private Runnable getRunnable() {
            return new Runnable() {
                @Override
                public void run() {
                    if (!mDidFinishResolution) {
                        handleSearchTermResolutionResponse(mIsNetworkUnavailable, mResponseCode,
                                mSearchTerm, mDisplayText, mAlternateTerm, mMid, mDoPreventPreload,
                                mStartAdjust, mEndAdjust, mContextLanguage, mThumbnailUrl, mCaption,
                                mQuickActionUri, mQuickActionCategory, mDocId, mSnippetHash);

                        mActiveFakeTapSearch = null;
                        mDidFinishResolution = true;
                    }
                }
            };
        }
    }

    //============================================================================================
    // FakeTapSearch
    //============================================================================================

    /**
     * Class that represents a fake tap triggered contextual search that is slow to resolve.
     */
    public class FakeSlowResolveSearch extends FakeTapSearch {
        /**
         * @param nodeId
         * @param isNetworkUnavailable
         * @param responseCode
         * @param searchTerm
         * @param displayText
         * @param alternateTerm
         * @param mid
         * @param doPreventPreload
         * @param startAdjust
         * @param endAdjust
         * @param contextLanguage
         * @param thumbnailUrl
         * @param caption
         * @param quickActionUri
         * @param quickActionCategory
         * @param docId
         * @param snippetHash
         */
        FakeSlowResolveSearch(String nodeId, boolean isNetworkUnavailable, int responseCode,
                String searchTerm, String displayText, String alternateTerm, String mid,
                boolean doPreventPreload, int startAdjust, int endAdjust, String contextLanguage,
                String thumbnailUrl, String caption, String quickActionUri, int quickActionCategory,
                long docId, long snippetHash) {
            super(nodeId, isNetworkUnavailable, responseCode, searchTerm, displayText,
                    alternateTerm, mid, doPreventPreload, startAdjust, endAdjust, contextLanguage,
                    thumbnailUrl, caption, quickActionUri, quickActionCategory, docId, snippetHash);
        }

        @Override
        public void simulate() throws InterruptedException, TimeoutException {
            mActiveFakeTapSearch = this;

            // When a resolution is needed, the simulation does not start until the system
            // requests one, and it does not finish until the simulated resolution happens.
            mDidStartResolution = false;
            mDidFinishResolution = false;

            mManagerTest.clickNode(getNodeId());
            mManagerTest.waitForSelectionToBe(mSearchTerm);

            if (mPolicy.shouldPreviousTapResolve()) {
                // Now wait for the Search Term Resolution to start.
                mManagerTest.waitForSearchTermResolutionToStart(this);
            } else {
                throw new RuntimeException("Tried to simulate a slow resolving search when "
                        + "not resolving!");
            }
        }

        /**
         * Finishes the resolving of a slow-resolving Tap search.
         * @throws InterruptedException
         * @throws TimeoutException
         */
        void finishResolve() throws InterruptedException, TimeoutException {
            // Simulate a Search Term Resolution.
            simulateSearchTermResolution();

            // Now wait for the simulated Search Term Resolution to finish.
            mManagerTest.waitForSearchTermResolutionToFinish(this);
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
            super(contentDelegate, progressObserver, activity, barHeight);
        }

        @Override
        public void loadUrl(String url, boolean shouldLoadImmediately) {
            if (mUseInvalidLowPriorityPath && isLowPriorityUrl(url)) {
                url = makeInvalidUrl(url);
            }
            mLoadedUrl = url;
            mLoadedUrlCount++;
            super.loadUrl(url, shouldLoadImmediately);
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
    ContextualSearchFakeServer(ContextualSearchPolicy policy,
            ContextualSearchManagerTest managerTest,
            ContextualSearchNetworkCommunicator baseManager,
            OverlayContentDelegate contentDelegate,
            OverlayContentProgressObserver progressObserver,
            ChromeActivity activity) {
        mPolicy = policy;

        mManagerTest = managerTest;
        mBaseManager = baseManager;

        mContentDelegate = contentDelegate;
        mProgressObserver = progressObserver;
        mActivity = activity;
    }

    @Override
    public OverlayPanelContent createNewOverlayPanelContent() {
        return new OverlayPanelContentWrapper(mContentDelegate, mProgressObserver, mActivity,
                mManagerTest.getPanel().getBarHeight());
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
     * Sets whether to return an HTTPS URL instead of HTTP, from {@link #getBasePageUrl}.
     */
    @VisibleForTesting
    void setShouldUseHttps(boolean setting) {
        mShouldUseHttps = setting;
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
        mShouldUseHttps = false;
        mIsOnline = true;
        mLoadedUrlCount = 0;
        mUseInvalidLowPriorityPath = false;
    }

    /**
     * Sets a flag to build low-priority paths that are invalid in order to test failover.
     */
    @VisibleForTesting
    void setLowPriorityPathInvalid() {
        mUseInvalidLowPriorityPath = true;
    }

    /**
     * @return Whether the most recent loadUrl was on an invalid path.
     */
    @VisibleForTesting
    boolean didAttemptLoadInvalidUrl() {
        return mUseInvalidLowPriorityPath && mLoadedUrl.contains("invalid");
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
    public void startSearchTermResolutionRequest(String selection) {
        mLoadedUrl = null;
        mSearchTermRequested = selection;

        if (mActiveFakeTapSearch != null) {
            mActiveFakeTapSearch.notifySearchTermResolutionStarted();
        }
    }

    @Override
    public void handleSearchTermResolutionResponse(boolean isNetworkUnavailable, int responseCode,
            String searchTerm, String displayText, String alternateTerm, String mid,
            boolean doPreventPreload, int selectionStartAdjust, int selectionEndAdjust,
            String contextLanguage, String thumbnailUrl, String caption, String quickActionUri,
            int quickActionCategory, long docId, long snippetHash) {
        mBaseManager.handleSearchTermResolutionResponse(isNetworkUnavailable, responseCode,
                searchTerm, displayText, alternateTerm, mid, doPreventPreload, selectionStartAdjust,
                selectionEndAdjust, contextLanguage, thumbnailUrl, caption, quickActionUri,
                quickActionCategory, docId, snippetHash);
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
    public URL getBasePageUrl() {
        URL baseUrl = mBaseManager.getBasePageUrl();
        if (mShouldUseHttps && baseUrl != null) {
            try {
                return new URL(baseUrl.toString().replace("http://", "https://"));
            } catch (MalformedURLException e) {
                // TODO(donnd): Auto-generated catch block
                e.printStackTrace();
            }
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
        registerFakeLongPressSearch(new FakeLongPressSearch("search", "Search"));
        registerFakeLongPressSearch(new FakeLongPressSearch("term", "Term"));
        registerFakeLongPressSearch(new FakeLongPressSearch("resolution", "Resolution"));

        registerFakeTapSearch(new FakeTapSearch("search", false, 200, "Search", "Search",
                "alternate-term", "", false, 0, 0, "", "", "", "", QuickActionCategory.NONE, 0, 0));
        registerFakeTapSearch(new FakeTapSearch("term", false, 200, "Term", "Term",
                "alternate-term", "", false, 0, 0, "", "", "", "", QuickActionCategory.NONE, 0, 0));
        registerFakeTapSearch(new FakeTapSearch("resolution", false, 200, "Resolution",
                "Resolution", "alternate-term", "", false, 0, 0, "", "", "", "",
                QuickActionCategory.NONE, 0, 0));
        registerFakeTapSearch(
                new FakeTapSearch("german", false, 200, "Deutsche", "Deutsche", "alternate-term",
                        "", false, 0, 0, "de", "", "", "", QuickActionCategory.NONE, 0, 0));
        registerFakeTapSearch(new FakeTapSearch("intelligence", false, 200, "Intelligence",
                "Intelligence", "alternate-term", "", false, 0, 0, "", "", "", "",
                QuickActionCategory.NONE, ARBITRARY_DOC_ID, ARBITRARY_SNIPPET_HASH));

        // Register a resolving search of "States" that expands to "United States".
        registerFakeSlowResolveSearch(new FakeSlowResolveSearch("states", false, 200, "States",
                "States", "alternate-term", "", false, -7, 0, "", "", "", "",
                QuickActionCategory.NONE, 0, 0));
        registerFakeSlowResolveSearch(new FakeSlowResolveSearch("search", false, 200, "Search",
                "Search", "alternate-term", "", false, 0, 0, "", "", "", "",
                QuickActionCategory.NONE, 0, 0));
    }

    /**
     * @param id The ID of the FakeLongPressSearch.
     * @return The FakeLongPressSearch with the given ID.
     */
    public FakeLongPressSearch getFakeLongPressSearch(String id) {
        return mFakeLongPressSearches.get(id);
    }

    /**
     * @param id The ID of the FakeTapSearch.
     * @return The FakeTapSearch with the given ID.
     */
    public FakeTapSearch getFakeTapSearch(String id) {
        return mFakeTapSearches.get(id);
    }

    /**
     * @param id The ID of the FakeSlowResolveSearch.
     * @return The {@code FakeSlowResolveSearch} with the given ID.
     */
    public FakeSlowResolveSearch getFakeSlowResolveSearch(String id) {
        return mFakeSlowResolveSearches.get(id);
    }

    /**
     * Register the FakeLongPressSearch.
     * @param fakeSearch The FakeLongPressSearch to be registered.
     */
    private void registerFakeLongPressSearch(FakeLongPressSearch fakeSearch) {
        mFakeLongPressSearches.put(fakeSearch.getNodeId(), fakeSearch);
    }

    /**
     * Register the FakeTapSearch.
     * @param fakeSearch The FakeTapSearch to be registered.
     */
    private void registerFakeTapSearch(FakeTapSearch fakeSearch) {
        mFakeTapSearches.put(fakeSearch.getNodeId(), fakeSearch);
    }

    /**
     * Register the FakeSlowResolveSearch.
     * @param fakeSlowResolveSearch The {@code FakeSlowResolveSearch} to be registered.
     */
    private void registerFakeSlowResolveSearch(FakeSlowResolveSearch fakeSlowResolveSearch) {
        mFakeSlowResolveSearches.put(fakeSlowResolveSearch.getNodeId(), fakeSlowResolveSearch);
    }
}
