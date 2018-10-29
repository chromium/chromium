// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.os.Handler;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.view.ViewTreeObserver.OnGlobalFocusChangeListener;

import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.SysUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.bottombar.OverlayContentDelegate;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.PanelState;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelContentViewDelegate;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchInternalStateController.InternalState;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchSelectionController.SelectionType;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.chrome.browser.externalnav.ExternalNavigationParams;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.gsa.GSAContextDisplaySelection;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabRedirectHandler;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.widget.findinpage.FindToolbarManager;
import org.chromium.chrome.browser.widget.findinpage.FindToolbarObserver;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.TriggerState;
import org.chromium.components.navigation_interception.NavigationParams;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.BrowserControlsState;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.contextual_search.mojom.OverlayPosition;
import org.chromium.net.NetworkChangeNotifier;

import java.net.MalformedURLException;
import java.net.URL;
import java.util.regex.Pattern;

/**
 * Manager for the Contextual Search feature. This class keeps track of the status of Contextual
 * Search and coordinates the control with the layout.
 */
public class ContextualSearchManager
        implements ContextualSearchManagementDelegate, ContextualSearchTranslateInterface,
                   ContextualSearchNetworkCommunicator, ContextualSearchSelectionHandler {
    /** A delegate for reporting selected context to GSA for search quality. */
    public interface ContextReporterDelegate {
        /**
         * Reports that the given display selection has been established for the current tab.
         * @param displaySelection The information about the selection being displayed.
         */
        void reportDisplaySelection(@Nullable GSAContextDisplaySelection displaySelection);
    }

    // TODO(donnd): provide an inner class that implements some of these interfaces (like the
    // ContextualSearchTranslateInterface) rather than having the manager itself implement the
    // interface because that exposes all the public methods of that interface at the manager level.

    private static final String TAG = "ContextualSearch";

    private static final String INTENT_URL_PREFIX = "intent:";

    // The animation duration of a URL being promoted to a tab when triggered by an
    // intercept navigation. This is faster than the standard tab promotion animation
    // so that it completes before the navigation.
    private static final long INTERCEPT_NAVIGATION_PROMOTION_ANIMATION_DURATION_MS = 40;

    // We blacklist this URL because malformed URLs may bring up this page.
    private static final String BLACKLISTED_URL = ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL;

    private static final Pattern CONTAINS_WHITESPACE_PATTERN = Pattern.compile("\\s");

    // When we don't need to send any "home country" code we can just pass the empty string.
    private static final String NO_HOME_COUNTRY = "";

    // How long to wait for a tap near a previous tap before hiding the UI or showing a re-Tap.
    // This setting is not critical: in practice it determines how long to wait after an invalid
    // tap for the page to respond before hiding the UI. Specifically this setting just needs to be
    // long enough for Blink's decisions before calling handleShowUnhandledTapUIIfNeeded (which
    // probably are page-dependent), and short enough that the Bar goes away fairly quickly after a
    // tap on non-text or whitespace: We currently do not get notification in these cases (hence the
    // timer).
    private static final int TAP_NEAR_PREVIOUS_DETECTION_DELAY_MS = 100;

    // How long to wait for a Tap to be converted to a Long-press gesture when the user taps on
    // an existing tap-selection.
    private static final int TAP_ON_TAP_SELECTION_DELAY_MS = 100;

    private static final int NANOSECONDS_IN_A_MILLISECOND = 1000000;

    private final ObserverList<ContextualSearchObserver> mObservers =
            new ObserverList<ContextualSearchObserver>();

    private final ChromeActivity mActivity;
    private final ContextualSearchTabPromotionDelegate mTabPromotionDelegate;
    private final ViewTreeObserver.OnGlobalFocusChangeListener mOnFocusChangeListener;
    private final TabModelObserver mTabModelObserver;

    // The Ranker logger to use to write Tap Suppression Ranker logs to UMA.
    private final ContextualSearchInteractionRecorder mTapSuppressionInteractionRecorder;

    private final ContextualSearchSelectionClient mContextualSearchSelectionClient;

    private ContextualSearchSelectionController mSelectionController;
    private ContextualSearchNetworkCommunicator mNetworkCommunicator;
    private ContextualSearchPolicy mPolicy;
    private ContextualSearchInternalStateController mInternalStateController;

    @VisibleForTesting
    protected ContextualSearchTranslateController mTranslateController;

    // The Overlay panel.
    private ContextualSearchPanel mSearchPanel;

    // The native manager associated with this object.
    private long mNativeContextualSearchManagerPtr;

    private ViewGroup mParentView;
    private TabRedirectHandler mTabRedirectHandler;
    private OverlayPanelContentViewDelegate mSearchContentViewDelegate;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private FindToolbarManager mFindToolbarManager;
    private FindToolbarObserver mFindToolbarObserver;
    private ContextualSearchIPH mInProductHelp;

    private boolean mDidStartLoadingResolvedSearchRequest;
    private long mLoadedSearchUrlTimeMs;
    private boolean mWereSearchResultsSeen;
    private boolean mWereInfoBarsHidden;
    private boolean mDidPromoteSearchNavigation;

    private boolean mWasActivatedByTap;
    private boolean mIsInitialized;
    private boolean mReceivedContextualCardsEntityData;

    // The current search context, or null.
    private ContextualSearchContext mContext;

    /**
     * This boolean is used for loading content after a long-press when content is not immediately
     * loaded.
     */
    private boolean mShouldLoadDelayedSearch;

    private boolean mIsShowingPromo;
    private boolean mIsMandatoryPromo;
    private boolean mDidLogPromoOutcome;

    /**
     * Whether contextual search manager is currently promoting a tab. We should be ignoring hide
     * requests when mIsPromotingTab is set to true.
     */
    private boolean mIsPromotingToTab;

    private ContextualSearchRequest mSearchRequest;
    private ContextualSearchRequest mLastSearchRequestLoaded;

    /** Whether the Accessibility Mode is enabled. */
    private boolean mIsAccessibilityModeEnabled;

    /** Tap Experiments and other variable behavior. */
    private QuickAnswersHeuristic mQuickAnswersHeuristic;

    // Counter for how many times we've called SelectWordAroundCaret without an ACK returned.
    // TODO(donnd): replace with a more systematic approach using the InternalStateController.
    private int mSelectWordAroundCaretCounter;

    /** Whether ContextualSearch UI is suppressed for Smart Selection. */
    private boolean mDoSuppressContextualSearchForSmartSelection;

    /** An observer that reports selected context to GSA for search quality. */
    private ContextualSearchObserver mContextReportingObserver;

    /**
     * The delegate that is responsible for promoting a {@link WebContents} to a {@link Tab}
     * when necessary.
     */
    public interface ContextualSearchTabPromotionDelegate {
        /**
         * Called when {@link WebContents} for contextual search should be promoted to a {@link
         * Tab}.
         * @param searchUrl The Search URL to be promoted.
         */
        void createContextualSearchTab(String searchUrl);
    }

    /**
     * Constructs the manager for the given activity, and will attach views to the given parent.
     * @param activity The {@code ChromeActivity} in use.
     * @param tabPromotionDelegate The {@link ContextualSearchTabPromotionDelegate} that is
     *        responsible for building tabs from contextual search {@link WebContents}.
     */
    public ContextualSearchManager(
            ChromeActivity activity, ContextualSearchTabPromotionDelegate tabPromotionDelegate) {
        mActivity = activity;
        mTabPromotionDelegate = tabPromotionDelegate;

        final View controlContainer = mActivity.findViewById(R.id.control_container);
        mOnFocusChangeListener = new OnGlobalFocusChangeListener() {
            @Override
            public void onGlobalFocusChanged(View oldFocus, View newFocus) {
                if (controlContainer != null && controlContainer.hasFocus()) {
                    hideContextualSearch(StateChangeReason.UNKNOWN);
                }
            }
        };

        mTabModelObserver = new EmptyTabModelObserver() {
            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                if ((!mIsPromotingToTab && tab.getId() != lastId)
                        || mActivity.getTabModelSelector().isIncognitoSelected()) {
                    hideContextualSearch(StateChangeReason.UNKNOWN);
                    mSelectionController.onTabSelected();
                }
            }

            @Override
            public void didAddTab(Tab tab, @TabLaunchType int type) {
                // If we're in the process of promoting this tab, just return and don't mess with
                // this state.
                if (tab.getWebContents() == getSearchPanelWebContents()) return;
                hideContextualSearch(StateChangeReason.UNKNOWN);
            }
        };

        mSelectionController = new ContextualSearchSelectionController(activity, this);
        mNetworkCommunicator = this;
        mPolicy = new ContextualSearchPolicy(mSelectionController, mNetworkCommunicator);
        mTranslateController = new ContextualSearchTranslateController(mPolicy, this);
        mInternalStateController = new ContextualSearchInternalStateController(
                mPolicy, getContextualSearchInternalStateHandler());
        mTapSuppressionInteractionRecorder = new ContextualSearchRankerLoggerImpl();
        mContextualSearchSelectionClient = new ContextualSearchSelectionClient();
        mInProductHelp = new ContextualSearchIPH();
    }

    /**
     * Initializes this manager.
     * @param parentView The parent view to attach Contextual Search UX to.
     */
    public void initialize(ViewGroup parentView) {
        mNativeContextualSearchManagerPtr = nativeInit();

        mParentView = parentView;
        mParentView.getViewTreeObserver().addOnGlobalFocusChangeListener(mOnFocusChangeListener);

        mInProductHelp.setParentView(parentView);

        mTabRedirectHandler = TabRedirectHandler.create(mActivity);

        mPolicy.initialize();

        mIsShowingPromo = false;
        mDidLogPromoOutcome = false;
        mDidStartLoadingResolvedSearchRequest = false;
        mWereSearchResultsSeen = false;
        mIsInitialized = true;

        mInternalStateController.reset(StateChangeReason.UNKNOWN);

        listenForTabModelSelectorNotifications();
    }

    /**
     * Sets the {@link FindToolbarManager} and attaches an observer that dismisses the Contextual
     * Search panel when the find toolbar is shown.
     *
     * @param findToolbarManager The {@link FindToolbarManager} for the current activity.
     */
    public void setFindToolbarManager(FindToolbarManager findToolbarManager) {
        if (mFindToolbarManager != null) {
            mFindToolbarManager.removeObserver(mFindToolbarObserver);
        }

        mFindToolbarManager = findToolbarManager;

        if (mFindToolbarObserver == null) {
            mFindToolbarObserver = new FindToolbarObserver() {
                @Override
                public void onFindToolbarShown() {
                    hideContextualSearch(StateChangeReason.UNKNOWN);
                }
            };
        }
        mFindToolbarManager.addObserver(mFindToolbarObserver);
    }

    /**
     * Destroys the native Contextual Search Manager.
     * Call this method before orphaning this object to allow it to be garbage collected.
     */
    public void destroy() {
        if (!mIsInitialized) return;

        hideContextualSearch(StateChangeReason.UNKNOWN);
        mParentView.getViewTreeObserver().removeOnGlobalFocusChangeListener(mOnFocusChangeListener);
        nativeDestroy(mNativeContextualSearchManagerPtr);
        stopListeningForHideNotifications();
        mTabRedirectHandler.clear();
        if (mFindToolbarManager != null) {
            mFindToolbarManager.removeObserver(mFindToolbarObserver);
            mFindToolbarManager = null;
            mFindToolbarObserver = null;
        }
        mInternalStateController.enter(InternalState.UNDEFINED);
    }

    @Override
    public void setContextualSearchPanel(ContextualSearchPanel panel) {
        assert panel != null;
        mSearchPanel = panel;
        mPolicy.setContextualSearchPanel(panel);
        mInProductHelp.setSearchPanel(panel);
    }

    @Override
    public ChromeActivity getChromeActivity() {
        return mActivity;
    }

    /** @return Whether the Search Panel is opened. That is, whether it is EXPANDED or MAXIMIZED. */
    public boolean isSearchPanelOpened() {
        return mSearchPanel != null && mSearchPanel.isPanelOpened();
    }

    /** @return Whether the {@code mSearchPanel} is not {@code null} and is showing. */
    private boolean isSearchPanelShowing() {
        return mSearchPanel != null && mSearchPanel.isShowing();
    }

    /**
     * @return the {@link WebContents} of the {@code mSearchPanel} or {@code null} if
     *         {@code mSearchPanel} is null or the search panel doesn't currently hold one.
     */
    private @Nullable WebContents getSearchPanelWebContents() {
        return mSearchPanel == null ? null : mSearchPanel.getWebContents();
    }

    /** @return The Base Page's {@link WebContents}. */
    @Nullable
    private WebContents getBaseWebContents() {
        return mSelectionController.getBaseWebContents();
    }

    /** @return The Base Page's {@link URL}. */
    @Nullable
    private URL getBasePageURL() {
        WebContents baseWebContents = mSelectionController.getBaseWebContents();
        if (baseWebContents == null) return null;
        try {
            return new URL(baseWebContents.getVisibleUrl());
        } catch (MalformedURLException e) {
            return null;
        }
    }

    /** Notifies that the base page has started loading a page. */
    public void onBasePageLoadStarted() {
        mSelectionController.onBasePageLoadStarted();
    }

    /** Notifies that a Context Menu has been shown. */
    void onContextMenuShown() {
        mSelectionController.onContextMenuShown();
    }

    /**
     * Hides the Contextual Search UX by changing into the IDLE state.
     * @param reason The {@link StateChangeReason} for hiding Contextual Search.
     */
    public void hideContextualSearch(@StateChangeReason int reason) {
        mInternalStateController.reset(reason);
    }

    @Override
    public void onCloseContextualSearch(@StateChangeReason int reason) {
        if (mSearchPanel == null) return;

        mSelectionController.onSearchEnded(reason);

        // Show the infobar container if it was visible before Contextual Search was shown.
        if (mWereInfoBarsHidden) {
            mWereInfoBarsHidden = false;
            InfoBarContainer container = getInfoBarContainer();
            if (container != null) {
                container.setHidden(false);
            }
        }

        if (!mWereSearchResultsSeen && mLoadedSearchUrlTimeMs != 0L) {
            removeLastSearchVisit();
        }

        // Clear the timestamp. This is to avoid future calls to hideContextualSearch clearing
        // the current URL.
        mLoadedSearchUrlTimeMs = 0L;
        mWereSearchResultsSeen = false;

        mSearchRequest = null;

        mInProductHelp.dismiss();

        if (mIsShowingPromo && !mDidLogPromoOutcome && mSearchPanel.wasPromoInteractive()) {
            ContextualSearchUma.logPromoOutcome(mWasActivatedByTap, mIsMandatoryPromo);
            mDidLogPromoOutcome = true;
        }

        mIsShowingPromo = false;
        mSearchPanel.setIsPromoActive(false, false);
        notifyHideContextualSearch();
    }

    /** Called when the system back button is pressed. Will hide the layout. */
    public boolean onBackPressed() {
        if (!mIsInitialized || !isSearchPanelShowing()) return false;
        hideContextualSearch(StateChangeReason.BACK_PRESS);
        return true;
    }

    /**
     * Shows the Contextual Search UX.
     * @param stateChangeReason The reason explaining the change of state.
     */
    private void showContextualSearch(@StateChangeReason int stateChangeReason) {
        assert mSearchPanel != null;
        if (mFindToolbarManager != null) {
            mFindToolbarManager.hideToolbar(false);
        }

        // Dismiss the undo SnackBar if present by committing all tab closures.
        mActivity.getTabModelSelector().commitAllTabClosures();

        if (!mSearchPanel.isShowing()) {
            // If visible, hide the infobar container before showing the Contextual Search panel.
            InfoBarContainer container = getInfoBarContainer();
            if (container != null && container.getVisibility() == View.VISIBLE) {
                mWereInfoBarsHidden = true;
                container.setHidden(true);
            }
        }

        // If the user is jumping from one unseen search to another search, remove the last search
        // from history.
        PanelState state = mSearchPanel.getPanelState();
        if (!mWereSearchResultsSeen && mLoadedSearchUrlTimeMs != 0L
                && state != PanelState.UNDEFINED && state != PanelState.CLOSED) {
            removeLastSearchVisit();
        }

        mSearchPanel.destroyContent();
        mReceivedContextualCardsEntityData = false;

        String selection = mSelectionController.getSelectedText();
        boolean isTap = mSelectionController.getSelectionType() == SelectionType.TAP;
        if (isTap) {
            // If the user action was not a long-press, we should not delay before loading content.
            mShouldLoadDelayedSearch = false;
        }
        if (isTap && mPolicy.shouldPreviousTapResolve()) {
            // Cache the native translate data, so JNI calls won't be made when time-critical.
            mTranslateController.cacheNativeTranslateData();
        } else if (!TextUtils.isEmpty(selection)) {
            boolean shouldPrefetch = mPolicy.shouldPrefetchSearchResult();
            mSearchRequest = new ContextualSearchRequest(selection, null, null, shouldPrefetch);
            mTranslateController.forceAutoDetectTranslateUnlessDisabled(mSearchRequest);
            mDidStartLoadingResolvedSearchRequest = false;
            mSearchPanel.setSearchTerm(selection);
            if (shouldPrefetch) loadSearchUrl();

            // Record metrics for manual refinement of the search term from long-press.
            // TODO(donnd): remove this section once metrics have been analyzed.
            if (!isTap && mSearchPanel.isPeeking()) {
                boolean isSingleWord =
                        !CONTAINS_WHITESPACE_PATTERN.matcher(selection.trim()).find();
                RecordUserAction.record(isSingleWord ? "ContextualSearch.ManualRefineSingleWord"
                                                     : "ContextualSearch.ManualRefineMultiWord");
            }
        } else {
            // The selection is no longer valid, so we can't build a request.  Don't show the UX.
            hideContextualSearch(StateChangeReason.UNKNOWN);
            return;
        }
        mWereSearchResultsSeen = false;

        // Note: now that the contextual search has properly started, set the promo involvement.
        if (mPolicy.isPromoAvailable()) {
            mIsShowingPromo = true;
            mIsMandatoryPromo = mPolicy.isMandatoryPromoAvailable();
            mDidLogPromoOutcome = false;
            mSearchPanel.setIsPromoActive(true, mIsMandatoryPromo);
            mSearchPanel.setDidSearchInvolvePromo();
        }

        mSearchPanel.requestPanelShow(stateChangeReason);

        assert mSelectionController.getSelectionType() != SelectionType.UNDETERMINED;
        mWasActivatedByTap = mSelectionController.getSelectionType() == SelectionType.TAP;

        Tracker tracker =
                TrackerFactory.getTrackerForProfile(mActivity.getActivityTab().getProfile());
        tracker.notifyEvent(mWasActivatedByTap
                        ? EventConstants.CONTEXTUAL_SEARCH_TRIGGERED_BY_TAP
                        : EventConstants.CONTEXTUAL_SEARCH_TRIGGERED_BY_LONGPRESS);

        // Log whether IPH for tapping has been shown before.
        if (mWasActivatedByTap) {
            ContextualSearchUma.logTapIPH(
                    tracker.getTriggerState(FeatureConstants.CONTEXTUAL_SEARCH_PROMOTE_TAP_FEATURE)
                    == TriggerState.HAS_BEEN_DISPLAYED);
        }
    }

    @Override
    public void startSearchTermResolutionRequest(String selection) {
        WebContents baseWebContents = getBaseWebContents();
        if (baseWebContents != null && mContext != null && mContext.canResolve()) {
            nativeStartSearchTermResolutionRequest(
                    mNativeContextualSearchManagerPtr, mContext, getBaseWebContents());
        } else {
            // Something went wrong and we couldn't resolve.
            hideContextualSearch(StateChangeReason.UNKNOWN);
        }
    }

    @Override
    @Nullable
    public URL getBasePageUrl() {
        WebContents baseWebContents = getBaseWebContents();
        if (baseWebContents == null) return null;

        try {
            return new URL(baseWebContents.getLastCommittedUrl());
        } catch (MalformedURLException e) {
            return null;
        }
    }

    /** Accessor for the {@code InfoBarContainer} currently attached to the {@code Tab}. */
    private InfoBarContainer getInfoBarContainer() {
        Tab tab = mActivity.getActivityTab();
        return tab == null ? null : InfoBarContainer.get(tab);
    }

    /** Listens for notifications that should hide the Contextual Search bar. */
    private void listenForTabModelSelectorNotifications() {
        TabModelSelector selector = mActivity.getTabModelSelector();

        mTabModelSelectorTabObserver = new TabModelSelectorTabObserver(selector) {
            @Override
            public void onPageLoadStarted(Tab tab, String url) {
                // Detects navigation of the base page for crbug.com/428368 (navigation-detection).
                hideContextualSearch(StateChangeReason.UNKNOWN);
            }

            @Override
            public void onCrash(Tab tab) {
                if (SadTab.isShowing(tab)) {
                    // Hide contextual search if the foreground tab crashed
                    hideContextualSearch(StateChangeReason.UNKNOWN);
                }
            }

            @Override
            public void onClosingStateChanged(Tab tab, boolean closing) {
                if (closing) hideContextualSearch(StateChangeReason.UNKNOWN);
            }
        };

        for (TabModel tabModel : selector.getModels()) {
            tabModel.addObserver(mTabModelObserver);
        }
    }

    /** Stops listening for notifications that should hide the Contextual Search bar. */
    private void stopListeningForHideNotifications() {
        if (mTabModelSelectorTabObserver != null) mTabModelSelectorTabObserver.destroy();

        TabModelSelector selector = mActivity.getTabModelSelector();
        if (selector != null) {
            for (TabModel tabModel : selector.getModels()) {
                tabModel.removeObserver(mTabModelObserver);
            }
        }
    }

    /** Clears our private member referencing the native manager. */
    @CalledByNative
    public void clearNativeManager() {
        assert mNativeContextualSearchManagerPtr != 0;
        mNativeContextualSearchManagerPtr = 0;
    }

    /**
     * Sets our private member referencing the native manager.
     * @param nativeManager The pointer to the native Contextual Search manager.
     */
    @CalledByNative
    public void setNativeManager(long nativeManager) {
        assert mNativeContextualSearchManagerPtr == 0;
        mNativeContextualSearchManagerPtr = nativeManager;
    }

    /**
     * Called by native code when the surrounding text and selection range are available.
     * This is done for both Tap and Long-press gestures.
     * @param encoding The original encoding used on the base page.
     * @param surroundingText The Text surrounding the selection.
     * @param startOffset The start offset of the selection.
     * @param endOffset The end offset of the selection.
     */
    @CalledByNative
    @VisibleForTesting
    void onTextSurroundingSelectionAvailable(
            final String encoding, final String surroundingText, int startOffset, int endOffset) {
        if (mInternalStateController.isStillWorkingOn(InternalState.GATHERING_SURROUNDINGS)) {
            assert mContext != null;
            // Sometimes Blink returns empty surroundings and 0 offsets so reset in that case.
            // See crbug.com/393100.
            if (surroundingText.length() == 0) {
                mInternalStateController.reset(StateChangeReason.UNKNOWN);
            } else {
                mContext.setSurroundingText(encoding, surroundingText, startOffset, endOffset);
                mInternalStateController.notifyFinishedWorkOn(InternalState.GATHERING_SURROUNDINGS);
            }
        }
    }

    /**
     * Called in response to the
     * {@link ContextualSearchManager#nativeStartSearchTermResolutionRequest} method.
     * If {@code nativeStartSearchTermResolutionRequest} is called with a previous request sill
     * pending our native delegate is supposed to cancel all previous requests.  So this code
     * should only be called with data corresponding to the most recent request.
     * @param isNetworkUnavailable Indicates if the network is unavailable, in which case all other
     *        parameters should be ignored.
     * @param responseCode The HTTP response code. If the code is not OK, the query should be
     *        ignored.
     * @param searchTerm The term to use in our subsequent search.
     * @param displayText The text to display in our UX.
     * @param alternateTerm The alternate term to display on the results page.
     * @param mid the MID for an entity to use to trigger a Knowledge Panel, or an empty string.
     *        A MID is a unique identifier for an entity in the Search Knowledge Graph.
     * @param selectionStartAdjust A positive number of characters that the start of the existing
     *        selection should be expanded by.
     * @param selectionEndAdjust A positive number of characters that the end of the existing
     *        selection should be expanded by.
     * @param contextLanguage The language of the original search term, or an empty string.
     * @param thumbnailUrl The URL of the thumbnail to display in our UX.
     * @param caption The caption to display.
     * @param quickActionUri The URI for the intent associated with the quick action.
     * @param quickActionCategory The {@link QuickActionCategory} for the quick action.
     * @param docId The ID of the document identified by the server, or 0 for not known.
     * @param snippetHash The hash of a snippet from the surrounding text, as identified by the
     *        server, or 0 for not known.
     */
    @CalledByNative
    public void onSearchTermResolutionResponse(boolean isNetworkUnavailable, int responseCode,
            final String searchTerm, final String displayText, final String alternateTerm,
            final String mid, boolean doPreventPreload, int selectionStartAdjust,
            int selectionEndAdjust, final String contextLanguage, final String thumbnailUrl,
            final String caption, final String quickActionUri, final int quickActionCategory,
            final long docId, final long snippetHash) {
        mNetworkCommunicator.handleSearchTermResolutionResponse(isNetworkUnavailable, responseCode,
                searchTerm, displayText, alternateTerm, mid, doPreventPreload, selectionStartAdjust,
                selectionEndAdjust, contextLanguage, thumbnailUrl, caption, quickActionUri,
                quickActionCategory, docId, snippetHash);
    }

    @Override
    public void handleSearchTermResolutionResponse(boolean isNetworkUnavailable, int responseCode,
            String searchTerm, String displayText, String alternateTerm, String mid,
            boolean doPreventPreload, int selectionStartAdjust, int selectionEndAdjust,
            String contextLanguage, String thumbnailUrl, String caption, String quickActionUri,
            int quickActionCategory, long docId, long snippetHash) {
        if (!mInternalStateController.isStillWorkingOn(InternalState.RESOLVING)) return;

        // Show an appropriate message for what to search for.
        String message;
        boolean doLiteralSearch = false;
        if (isNetworkUnavailable) {
            // TODO(donnd): double-check that the network is really unavailable, maybe using
            // NetworkChangeNotifier#isOnline.
            message = mActivity.getResources().getString(
                    R.string.contextual_search_network_unavailable);
        } else if (!isHttpFailureCode(responseCode) && !TextUtils.isEmpty(displayText)) {
            message = displayText;
        } else if (!mPolicy.shouldShowErrorCodeInBar()) {
            message = mSelectionController.getSelectedText();
            doLiteralSearch = true;
        } else {
            message = mActivity.getResources().getString(
                    R.string.contextual_search_error, responseCode);
            doLiteralSearch = true;
        }

        boolean receivedCaptionOrThumbnail = !TextUtils.isEmpty(caption)
                || !TextUtils.isEmpty(thumbnailUrl);

        assert mSearchPanel != null;
        mSearchPanel.onSearchTermResolved(message, thumbnailUrl, quickActionUri,
                quickActionCategory);
        if (!TextUtils.isEmpty(caption)) {
            // Call #onSetCaption() to set the caption. For entities, the caption should not be
            // regarded as an answer. In the future, when quick actions are added, doesAnswer will
            // need to be determined rather than always set to false.
            boolean doesAnswer = false;
            onSetCaption(caption, doesAnswer);
        }

        boolean quickActionShown =
                mSearchPanel.getSearchBarControl().getQuickActionControl().hasQuickAction();
        mReceivedContextualCardsEntityData = !quickActionShown && receivedCaptionOrThumbnail;

        if (mReceivedContextualCardsEntityData) {
            Tracker tracker = TrackerFactory.getTrackerForProfile(
                    Profile.getLastUsedProfile().getOriginalProfile());
            tracker.notifyEvent(EventConstants.CONTEXTUAL_SEARCH_ENTITY_RESULT);
            mInProductHelp.onEntityDataReceived(
                    mWasActivatedByTap, Profile.getLastUsedProfile().getOriginalProfile());
        }

        ContextualSearchUma.logContextualCardsDataShown(mReceivedContextualCardsEntityData);
        mSearchPanel.getPanelMetrics().setWasContextualCardsDataShown(
                mReceivedContextualCardsEntityData);
        ContextualSearchUma.logQuickActionShown(quickActionShown, quickActionCategory);
        mSearchPanel.getPanelMetrics().setWasQuickActionShown(
                quickActionShown, quickActionCategory);

        // If there was an error, fall back onto a literal search for the selection.
        // Since we're showing the panel, there must be a selection.
        if (doLiteralSearch) {
            searchTerm = mSelectionController.getSelectedText();
            alternateTerm = null;
            doPreventPreload = true;
        }
        if (!TextUtils.isEmpty(searchTerm)) {
            // TODO(donnd): Instead of preloading, we should prefetch (ie the URL should not
            // appear in the user's history until the user views it).  See crbug.com/406446.
            boolean shouldPreload = !doPreventPreload && mPolicy.shouldPrefetchSearchResult();
            mSearchRequest =
                    new ContextualSearchRequest(searchTerm, alternateTerm, mid, shouldPreload);
            // Trigger translation, if enabled.
            mTranslateController.forceTranslateIfNeeded(mSearchRequest, contextLanguage);
            mDidStartLoadingResolvedSearchRequest = false;
            if (mSearchPanel.isContentShowing()) {
                mSearchRequest.setNormalPriority();
            }
            if (mSearchPanel.isContentShowing() || shouldPreload) {
                loadSearchUrl();
            }
            mPolicy.logSearchTermResolutionDetails(searchTerm);
        }

        // Adjust the selection unless the user changed it since we initiated the search.
        if ((selectionStartAdjust != 0 || selectionEndAdjust != 0)
                && mSelectionController.getSelectionType() == SelectionType.TAP) {
            String originalSelection = mContext == null ? null : mContext.getInitialSelectedWord();
            String currentSelection = mSelectionController.getSelectedText();
            if (currentSelection != null) currentSelection = currentSelection.trim();
            if (originalSelection != null && originalSelection.trim().equals(currentSelection)) {
                mSelectionController.adjustSelection(selectionStartAdjust, selectionEndAdjust);
                mContext.onSelectionAdjusted(selectionStartAdjust, selectionEndAdjust);
            }
        }

        // Tell the interaction recorder about the docId and snippetHash, so we can correlate the
        // signals and outcomes with this crawled document (when available).
        mTapSuppressionInteractionRecorder.recordSnippetData(docId, snippetHash);

        mInternalStateController.notifyFinishedWorkOn(InternalState.RESOLVING);
    }

    /**
     * External entry point to determine if the device is currently online or not.
     * Stubbed out when under test.
     * @return Whether the device is currently online.
     */
    boolean isDeviceOnline() {
        return mNetworkCommunicator.isOnline();
    }

    /** Handles this {@link ContextualSearchNetworkCommunicator} vector when not under test. */
    @Override
    public boolean isOnline() {
        return NetworkChangeNotifier.isOnline();
    }

    /** Loads a Search Request in the Contextual Search's Content View. */
    private void loadSearchUrl() {
        assert mSearchPanel != null;
        mLoadedSearchUrlTimeMs = System.currentTimeMillis();
        mLastSearchRequestLoaded = mSearchRequest;
        String searchUrl = mSearchRequest.getSearchUrl();
        nativeWhitelistContextualSearchJsApiUrl(mNativeContextualSearchManagerPtr, searchUrl);
        mSearchPanel.loadUrlInPanel(searchUrl);
        mDidStartLoadingResolvedSearchRequest = true;

        // TODO(donnd): If the user taps on a word and quickly after that taps on the
        // peeking Search Bar, the Search Content View will not be displayed. It seems that
        // calling WebContents.onShow() while it's being created has no effect.
        // For now, we force the ContentView to be displayed by calling onShow() again
        // when a URL is being loaded. See: crbug.com/398206
        if (mSearchPanel.isContentShowing() && getSearchPanelWebContents() != null) {
            getSearchPanelWebContents().onShow();
        }
    }

    /**
     * Called to set a caption. The caption may either be included with the search term resolution
     * response or set by the page through the CS JavaScript API used to notify CS that there is
     * a caption available on the current overlay.
     * @param caption The caption to display.
     * @param doesAnswer Whether the caption should be regarded as an answer such
     *        that the user may not need to open the panel, or whether the caption
     *        is simply informative or descriptive of the answer in the full results.
     */
    @CalledByNative
    private void onSetCaption(String caption, boolean doesAnswer) {
        if (TextUtils.isEmpty(caption) || mSearchPanel == null) return;

        // Notify the UI of the caption.
        mSearchPanel.setCaption(caption);
        if (mQuickAnswersHeuristic != null) {
            mQuickAnswersHeuristic.setConditionSatisfied(true);
            mQuickAnswersHeuristic.setDoesAnswer(doesAnswer);
        }

        // Update Tap counters to account for a possible answer.
        mPolicy.updateCountersForQuickAnswer(mWasActivatedByTap, doesAnswer);
    }

    /**
     * Called by JavaScript in the Overlay to change the position of the overlay.
     * The panel cannot be changed to any opened position if it's not already opened.
     * @param desiredPosition The desired position of the Overlay Panel expressed as an
     *        OverlayPosition int (defined in contextual_search_js_api_service.mojom).
     */
    @CalledByNative
    private void onChangeOverlayPosition(int desiredPosition) {
        assert desiredPosition >= OverlayPosition.CLOSE
                && desiredPosition <= OverlayPosition.MAXIMIZE;
        // Ignore requests when the panel is not already open to prevent spam or abuse of the API.
        if (!mSearchPanel.isShowing() || desiredPosition < OverlayPosition.CLOSE
                || desiredPosition > OverlayPosition.MAXIMIZE) {
            Log.w(TAG, "Unexpected request to set Overlay position to " + desiredPosition);
            return;
        }

        // Set the position.
        switch (desiredPosition) {
            case OverlayPosition.CLOSE:
                mSearchPanel.closePanel(StateChangeReason.UNKNOWN, true);
                break;
            case OverlayPosition.PEEK:
                mSearchPanel.peekPanel(StateChangeReason.UNKNOWN);
                break;
            case OverlayPosition.EXPAND:
                mSearchPanel.expandPanel(StateChangeReason.UNKNOWN);
                break;
            case OverlayPosition.MAXIMIZE:
                mSearchPanel.maximizePanel(StateChangeReason.UNKNOWN);
                break;
        }
    }

    /**
     * Notifies that the Accessibility Mode state has changed.
     *
     * @param enabled Whether the Accessibility Mode is enabled.
     */
    public void onAccessibilityModeChanged(boolean enabled) {
        mIsAccessibilityModeEnabled = enabled;
        if (enabled) hideContextualSearch(StateChangeReason.UNKNOWN);
    }

    /**
     * Notifies that the preference state has changed.
     * @param isEnabled Whether the feature is enabled.
     */
    public void onContextualSearchPrefChanged(boolean isEnabled) {
        // The pref may be automatically changed during application startup due to enterprise
        // configuration settings, so we may not have a panel yet.
        if (mSearchPanel != null) mSearchPanel.onContextualSearchPrefChanged(isEnabled);
    }

    @Override
    public void stopPanelContentsNavigation() {
        if (getSearchPanelWebContents() == null) return;

        getSearchPanelWebContents().stop();
    }

    // ============================================================================================
    // Observers
    // ============================================================================================

    /** @param observer An observer to notify when the user performs a contextual search. */
    void addObserver(ContextualSearchObserver observer) {
        mObservers.addObserver(observer);
    }

    /** @param observer An observer to no longer notify when the user performs a contextual search.
     */
    void removeObserver(ContextualSearchObserver observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Notifies that a new selection has been established and available for Contextual Search.
     * Should be called when the selection changes to notify listeners that care about the selection
     * and surrounding text.
     * Specifically this means we're showing the Contextual Search UX for the given selection.
     * Notifies Icing of the current selection.
     * Also notifies the panel whether the selection was part of a URL.
     */
    private void notifyObserversOfContextSelectionChanged() {
        assert mContext != null;
        String surroundingText = mContext.getSurroundingText();
        assert surroundingText != null;
        int startOffset = mContext.getSelectionStartOffset();
        int endOffset = mContext.getSelectionEndOffset();
        if (!ContextualSearchFieldTrial.isPageContentNotificationDisabled()) {
            GSAContextDisplaySelection selection = new GSAContextDisplaySelection(
                    mContext.getEncoding(), surroundingText, startOffset, endOffset);
            notifyShowContextualSearch(selection);
        }
    }

    /**
     * Notifies all Contextual Search observers that a search has occurred.
     * @param selectionContext The selection and context that triggered the search.
     */
    private void notifyShowContextualSearch(GSAContextDisplaySelection selectionContext) {
        if (!mPolicy.canSendSurroundings()) selectionContext = null;

        for (ContextualSearchObserver observer : mObservers) {
            observer.onShowContextualSearch(selectionContext);
        }
    }

    /** Notifies all Contextual Search observers that a search ended and is no longer in effect. */
    private void notifyHideContextualSearch() {
        for (ContextualSearchObserver observer : mObservers) {
            observer.onHideContextualSearch();
        }
    }

    // ============================================================================================
    // ContextualSearchTranslateInterface
    // ============================================================================================

    @Override
    public String getAcceptLanguages() {
        return nativeGetAcceptLanguages(mNativeContextualSearchManagerPtr);
    }

    @Override
    public String getTranslateServiceTargetLanguage() {
        return nativeGetTargetLanguage(mNativeContextualSearchManagerPtr);
    }

    // ============================================================================================
    // OverlayContentDelegate
    // ============================================================================================

    @Override
    public OverlayContentDelegate getOverlayContentDelegate() {
        return new SearchOverlayContentDelegate();
    }

    /** Implementation of OverlayContentDelegate. Made public for testing purposes. */
    public class SearchOverlayContentDelegate extends OverlayContentDelegate {
        // Note: New navigation or changes to the WebContents are not advised in this class since
        // the WebContents is being observed and navigation is already being performed.

        public SearchOverlayContentDelegate() {}

        @Override
        public void onMainFrameLoadStarted(String url, boolean isExternalUrl) {
            assert mSearchPanel != null;
            mSearchPanel.updateBrowserControlsState();

            if (isExternalUrl) {
                onExternalNavigation(url);
            }
        }

        @Override
        public void onMainFrameNavigation(String url, boolean isExternalUrl, boolean isFailure) {
            assert mSearchPanel != null;
            if (isExternalUrl) {
                if (!ContextualSearchFieldTrial.isAmpAsSeparateTabDisabled()
                        && mPolicy.isAmpUrl(url) && mSearchPanel.didTouchContent()) {
                    onExternalNavigation(url);
                }
            } else {
                // Could be just prefetching, check if that failed.
                onContextualSearchRequestNavigation(isFailure);

                // Record metrics for when the prefetched results became viewable.
                if (mSearchRequest != null && mSearchRequest.wasPrefetch()) {
                    boolean didResolve = mPolicy.shouldPreviousTapResolve();
                    mSearchPanel.onPanelNavigatedToPrefetchedSearch(didResolve);
                }
            }
        }

        @Override
        public void onContentLoadStarted(String url) {
            mDidPromoteSearchNavigation = false;
        }

        @Override
        public void onVisibilityChanged(boolean isVisible) {
            if (isVisible) {
                mWereSearchResultsSeen = true;
                // If there's no current request, then either a search term resolution
                // is in progress or we should do a verbatim search now.
                if (mSearchRequest == null && mPolicy.shouldCreateVerbatimRequest()
                        && !TextUtils.isEmpty(mSelectionController.getSelectedText())) {
                    mSearchRequest = new ContextualSearchRequest(
                            mSelectionController.getSelectedText(), null, null, false);
                    mDidStartLoadingResolvedSearchRequest = false;
                }
                if (mSearchRequest != null
                        && (!mDidStartLoadingResolvedSearchRequest || mShouldLoadDelayedSearch)) {
                    // mShouldLoadDelayedSearch is used in the long-press case to load content.
                    // Since content is now created and destroyed for each request, was impossible
                    // to know if content was already loaded or recently needed to be; this is for
                    // the case where it needed to be.
                    mSearchRequest.setNormalPriority();
                    loadSearchUrl();
                }
                mShouldLoadDelayedSearch = true;
                mPolicy.updateCountersForOpen();
            }
        }

        @Override
        public void onContentViewCreated() {
            nativeEnableContextualSearchJsApiForWebContents(
                    mNativeContextualSearchManagerPtr, getSearchPanelWebContents());
        }

        @Override
        public void onContentViewDestroyed() {
            if (mSearchContentViewDelegate != null) {
                mSearchContentViewDelegate.releaseOverlayPanelContent();
            }
        }

        @Override
        public void onContentViewSeen() {
            assert mSearchPanel != null;
            mSearchPanel.setWasSearchContentViewSeen();
        }

        @Override
        public boolean shouldInterceptNavigation(
                ExternalNavigationHandler externalNavHandler, NavigationParams navigationParams) {
            assert mSearchPanel != null;
            mTabRedirectHandler.updateNewUrlLoading(navigationParams.pageTransitionType,
                    navigationParams.isRedirect,
                    navigationParams.hasUserGesture || navigationParams.hasUserGestureCarryover,
                    mActivity.getLastUserInteractionTime(), TabRedirectHandler.INVALID_ENTRY_INDEX);
            ExternalNavigationParams params =
                    new ExternalNavigationParams
                            .Builder(navigationParams.url, false, navigationParams.referrer,
                                    navigationParams.pageTransitionType,
                                    navigationParams.isRedirect)
                            .setApplicationMustBeInForeground(true)
                            .setRedirectHandler(mTabRedirectHandler)
                            .setIsMainFrame(navigationParams.isMainFrame)
                            .build();
            if (externalNavHandler.shouldOverrideUrlLoading(params)
                    != OverrideUrlLoadingResult.NO_OVERRIDE) {
                mSearchPanel.maximizePanelThenPromoteToTab(StateChangeReason.TAB_PROMOTION,
                        INTERCEPT_NAVIGATION_PROMOTION_ANIMATION_DURATION_MS);
                return false;
            }
            return !navigationParams.isExternalProtocol;
        }
    }

    // ============================================================================================
    // Search Content View
    // ============================================================================================

    /**
     * Sets the {@code OverlayPanelContentViewDelegate} associated with the Content View.
     * @param delegate
     */
    public void setSearchContentViewDelegate(OverlayPanelContentViewDelegate delegate) {
        mSearchContentViewDelegate = delegate;
    }

    /** Removes the last resolved search URL from the Chrome history. */
    private void removeLastSearchVisit() {
        assert mSearchPanel != null;
        if (mLastSearchRequestLoaded != null) {
            // TODO(pedrosimonetti): Consider having this feature builtin into OverlayPanelContent.
            mSearchPanel.removeLastHistoryEntry(
                    mLastSearchRequestLoaded.getSearchUrl(), mLoadedSearchUrlTimeMs);
        }
    }

    /**
     * Called when the Search content view navigates to a contextual search request URL.
     * This navigation could be for a prefetch when the panel is still closed, or
     * a load of a user-visible search result.
     * @param isFailure Whether the navigation failed.
     */
    private void onContextualSearchRequestNavigation(boolean isFailure) {
        if (mSearchRequest == null) return;

        if (mSearchRequest.isUsingLowPriority()) {
            ContextualSearchUma.logLowPrioritySearchRequestOutcome(isFailure);
        } else {
            ContextualSearchUma.logNormalPrioritySearchRequestOutcome(isFailure);
            if (mSearchRequest.getHasFailed()) {
                ContextualSearchUma.logFallbackSearchRequestOutcome(isFailure);
            }
        }

        if (isFailure && mSearchRequest.isUsingLowPriority()) {
            // We're navigating to an error page, so we want to stop and retry.
            // Stop loading the page that displays the error to the user.
            if (getSearchPanelWebContents() != null) {
                // When running tests the Content View might not exist.
                mNetworkCommunicator.stopPanelContentsNavigation();
            }
            mSearchRequest.setHasFailed();
            mSearchRequest.setNormalPriority();
            // If the content view is showing, load at normal priority now.
            if (mSearchPanel != null && mSearchPanel.isContentShowing()) {
                // NOTE: we must reuse the existing content view because we're called from within
                // a WebContentsObserver.  If we don't reuse the content view then the WebContents
                // being observed will be deleted.  We notify of the failure to trigger the reuse.
                // See crbug.com/682953 for details.
                mSearchPanel.onLoadUrlFailed();
                loadSearchUrl();
            } else {
                mDidStartLoadingResolvedSearchRequest = false;
            }
        }
    }

    @Override
    public void logCurrentState() {
        if (ContextualSearchFieldTrial.isEnabled()) {
            mPolicy.logCurrentState();
        }
    }

    /** @return Whether the given HTTP result code represents a failure or not. */
    private boolean isHttpFailureCode(int httpResultCode) {
        return httpResultCode <= 0 || httpResultCode >= 400;
    }

    /** @return whether a navigation in the search content view should promote to a separate tab. */
    private boolean shouldPromoteSearchNavigation() {
        // A navigation can be due to us loading a URL, or a touch in the search content view.
        // Require a touch, but no recent loading, in order to promote to a separate tab.
        // Note that tapping the opt-in button requires checking for recent loading.
        assert mSearchPanel != null;
        return mSearchPanel.didTouchContent() && !mSearchPanel.isProcessingPendingNavigation();
    }

    /**
     * Called to check if an external navigation is being done and take the appropriate action:
     * Auto-promotes the panel into a separate tab if that's not already being done.
     * @param url The URL we are navigating to.
     */
    public void onExternalNavigation(String url) {
        if (!mDidPromoteSearchNavigation && !BLACKLISTED_URL.equals(url)
                && !url.startsWith(INTENT_URL_PREFIX) && shouldPromoteSearchNavigation()
                && mSearchPanel != null) {
            // Do not promote to a regular tab if we're loading our Resolved Search
            // URL, otherwise we'll promote it when prefetching the Serp.
            // Don't promote URLs when they are navigating to an intent - this is
            // handled by the InterceptNavigationDelegate which uses a faster
            // maximizing animation.
            mDidPromoteSearchNavigation = true;
            mSearchPanel.maximizePanelThenPromoteToTab(StateChangeReason.SERP_NAVIGATION);
        }
    }

    @Override
    public void openResolvedSearchUrlInNewTab() {
        if (mSearchRequest != null && mSearchRequest.getSearchUrlForPromotion() != null) {
            TabModelSelector tabModelSelector = mActivity.getTabModelSelector();
            tabModelSelector.openNewTab(
                    new LoadUrlParams(mSearchRequest.getSearchUrlForPromotion()),
                    TabLaunchType.FROM_LINK,
                    tabModelSelector.getCurrentTab(),
                    tabModelSelector.isIncognitoSelected());
        }
    }

    @Override
    public boolean isRunningInCompatibilityMode() {
        return SysUtils.isLowEndDevice();
    }

    @Override
    public void promoteToTab() {
        assert mSearchPanel != null;
        // TODO(pedrosimonetti): Consider removing this member.
        mIsPromotingToTab = true;

        // If the request object is null that means that a Contextual Search has just started
        // and the Search Term Resolution response hasn't arrived yet. In this case, promoting
        // the Panel to a Tab will result in creating a new tab with URL about:blank. To prevent
        // this problem, we are ignoring tap gestures in the Search Bar if we don't know what
        // to search for.
        if (mSearchRequest != null && getSearchPanelWebContents() != null) {
            String url = getContentViewUrl(getSearchPanelWebContents());

            // If it's a search URL, format it so the SearchBox becomes visible.
            if (mSearchRequest.isContextualSearchUrl(url)) {
                url = mSearchRequest.getSearchUrlForPromotion();
            }

            if (url != null) {
                mTabPromotionDelegate.createContextualSearchTab(url);
                mSearchPanel.closePanel(StateChangeReason.TAB_PROMOTION, false);
            }
        }
        mIsPromotingToTab = false;
    }

    /**
     * Gets the currently loading or loaded URL in a WebContents.
     *
     * @param searchWebContents The given WebContents.
     * @return The current loaded URL.
     */
    private String getContentViewUrl(WebContents searchWebContents) {
        // First, check the pending navigation entry, because there might be an navigation
        // not yet committed being processed. Otherwise, get the URL from the WebContents.
        NavigationEntry entry = searchWebContents.getNavigationController().getPendingEntry();
        return entry != null ? entry.getUrl() : searchWebContents.getLastCommittedUrl();
    }

    @Override
    public void dismissContextualSearchBar() {
        hideContextualSearch(StateChangeReason.UNKNOWN);
    }

    @Override
    public void onPanelFinishedShowing() {
        mInProductHelp.onPanelFinishedShowing(
                mWasActivatedByTap, Profile.getLastUsedProfile().getOriginalProfile());
    }

    @Override
    public void onPanelResized() {
        mInProductHelp.updateBubblePosition();
    }

    /**
     * @return The {@link SelectionClient} used by Contextual Search.
     */
    SelectionClient getContextualSearchSelectionClient() {
        return mContextualSearchSelectionClient;
    }

    /**
     * Notifies Contextual Search whether the UI should be suppressed for Smart Selection.
     */
    void suppressContextualSearchForSmartSelection(boolean isSmartSelectionEnabled) {
        mDoSuppressContextualSearchForSmartSelection = isSmartSelectionEnabled;
    }

    /**
     * Implements the {@link SelectionClient} interface for Contextual Search.
     * Handles messages from Content about selection changes.  These are the key drivers of
     * Contextual Search logic.
     */
    private class ContextualSearchSelectionClient implements SelectionClient {
        @Override
        public void onSelectionChanged(String selection) {
            if (!isOverlayVideoMode() && mSearchPanel != null) {
                mSelectionController.handleSelectionChanged(selection);
                mSearchPanel.updateBrowserControlsState(BrowserControlsState.BOTH, true);
            }
        }

        @Override
        public void onSelectionEvent(int eventType, float posXPix, float posYPix) {
            if (!isOverlayVideoMode()) {
                mSelectionController.handleSelectionEvent(eventType, posXPix, posYPix);
            }
        }

        @Override
        public void selectWordAroundCaretAck(boolean didSelect, int startAdjust, int endAdjust) {
            if (mSelectWordAroundCaretCounter > 0) mSelectWordAroundCaretCounter--;
            if (mSelectWordAroundCaretCounter > 0
                    || !mInternalStateController.isStillWorkingOn(
                               InternalState.START_SHOWING_TAP_UI)) {
                return;
            }

            if (didSelect) {
                assert mContext != null;
                mContext.onSelectionAdjusted(startAdjust, endAdjust);
                // There's a race condition when we select the word between this Ack response and
                // the onSelectionChanged call.  Update the selection in case this method won the
                // race so we ensure that there's a valid selected word.
                // See https://crbug.com/889657 for details.
                String adjustedSelection = mContext.getSelection();
                if (!TextUtils.isEmpty(adjustedSelection)) {
                    mSelectionController.setSelectedText(adjustedSelection);
                }
                showSelectionAsSearchInBar(mSelectionController.getSelectedText());
                mInternalStateController.notifyFinishedWorkOn(InternalState.START_SHOWING_TAP_UI);
            } else {
                hideContextualSearch(StateChangeReason.UNKNOWN);
            }
        }

        @Override
        public boolean requestSelectionPopupUpdates(boolean shouldSuggest) {
            return false;
        }

        @Override
        public void cancelAllRequests() {}
    }

    /** Shows the Unhandled Tap UI.  Called by {@link ContextualSearchTabHelper}. */
    void onShowUnhandledTapUIIfNeeded(int x, int y, int fontSizeDips, int textRunLength) {
        if (!isOverlayVideoMode()) {
            mSelectionController.handleShowUnhandledTapUIIfNeeded(
                    x, y, fontSizeDips, textRunLength);
        }
    }

    /**
     * @return Whether the display is in a full-screen video overlay mode.
     */
    private boolean isOverlayVideoMode() {
        return mActivity.getFullscreenManager() != null
                && mActivity.getFullscreenManager().isOverlayVideoMode();
    }

    // ============================================================================================
    // Selection
    // ============================================================================================

    /**
     * Returns a new {@code GestureStateListener} that will listen for events in the Base Page.
     * This listener will handle all Contextual Search-related interactions that go through the
     * listener.
     */
    public GestureStateListener getGestureStateListener() {
        return mSelectionController.getGestureStateListener();
    }

    @Override
    public void handleScroll() {
        if (mIsAccessibilityModeEnabled) return;

        hideContextualSearch(StateChangeReason.BASE_PAGE_SCROLL);
    }

    @Override
    public void handleInvalidTap() {
        if (mIsAccessibilityModeEnabled) return;

        hideContextualSearch(StateChangeReason.BASE_PAGE_TAP);
    }

    @Override
    public void handleSuppressedTap() {
        if (mIsAccessibilityModeEnabled) return;

        hideContextualSearch(StateChangeReason.TAP_SUPPRESS);
    }

    @Override
    public void handleNonSuppressedTap(long tapTimeNanoseconds) {
        if (mIsAccessibilityModeEnabled) return;

        // If there's a wait-after-tap experiment then we may want to delay a bit longer for
        // the user to take an action like scrolling that will reset our internal state.
        long delayBeforeFinishingWorkMs = 0;
        if (ContextualSearchFieldTrial.getWaitAfterTapDelayMs() > 0 && tapTimeNanoseconds > 0) {
            delayBeforeFinishingWorkMs = ContextualSearchFieldTrial.getWaitAfterTapDelayMs()
                    - (System.nanoTime() - tapTimeNanoseconds) / NANOSECONDS_IN_A_MILLISECOND;
        }

        // Finish work on the current state, either immediately or with a delay.
        if (delayBeforeFinishingWorkMs <= 0) {
            finishSuppressionDecision();
        } else {
            new Handler().postDelayed(new Runnable() {
                @Override
                public void run() {
                    finishSuppressionDecision();
                }
            }, delayBeforeFinishingWorkMs);
        }
    }

    /**
     * Finishes work on the suppression decision if that work is still in progress.
     * If no longer working on the suppression decision then resets the Ranker-logger.
     */
    private void finishSuppressionDecision() {
        if (mInternalStateController.isStillWorkingOn(InternalState.DECIDING_SUPPRESSION)) {
            mInternalStateController.notifyFinishedWorkOn(InternalState.DECIDING_SUPPRESSION);
        } else {
            mTapSuppressionInteractionRecorder.reset();
        }
    }

    @Override
    public void handleMetricsForWouldSuppressTap(ContextualSearchHeuristics tapHeuristics) {
        mQuickAnswersHeuristic = tapHeuristics.getQuickAnswersHeuristic();
        if (mSearchPanel != null) {
            mSearchPanel.getPanelMetrics().setResultsSeenExperiments(tapHeuristics);
        }
    }

    @Override
    public void handleValidTap() {
        if (mIsAccessibilityModeEnabled) return;

        mInternalStateController.enter(InternalState.TAP_RECOGNIZED);
    }

    /**
     * Notifies this class that the selection has changed. This may be due to the user moving the
     * selection handles after a long-press, or after a Tap gesture has called selectWordAroundCaret
     * to expand the selection to a whole word.
     */
    @Override
    public void handleSelection(
            String selection, boolean selectionValid, @SelectionType int type, float x, float y) {
        if (mIsAccessibilityModeEnabled) return;

        if (!selection.isEmpty()) {
            ContextualSearchUma.logSelectionIsValid(selectionValid);

            if (selectionValid && mSearchPanel != null) {
                mSearchPanel.updateBasePageSelectionYPx(y);
                if (!mSearchPanel.isShowing()) {
                    mSearchPanel.getPanelMetrics().onSelectionEstablished(selection);
                }
                showSelectionAsSearchInBar(selection);

                if (type == SelectionType.LONG_PRESS) {
                    mInternalStateController.enter(InternalState.LONG_PRESS_RECOGNIZED);
                }
            } else {
                hideContextualSearch(StateChangeReason.INVALID_SELECTION);
            }
        }
    }

    @Override
    public void handleSelectionDismissal() {
        if (mIsAccessibilityModeEnabled) return;

        if (isSearchPanelShowing()
                && !mIsPromotingToTab
                // If the selection is dismissed when the Panel is not peeking anymore,
                // which means the Panel is at least partially expanded, then it means
                // the selection was cleared by an external source (like JavaScript),
                // so we should not dismiss the UI in here.
                // See crbug.com/516665
                && mSearchPanel.isPeeking()) {
            hideContextualSearch(StateChangeReason.CLEARED_SELECTION);
        }
    }

    @Override
    public void handleSelectionModification(
            String selection, boolean selectionValid, float x, float y) {
        if (mIsAccessibilityModeEnabled) return;

        if (isSearchPanelShowing()) {
            if (selectionValid) {
                mSearchPanel.setSearchTerm(selection);
            } else {
                hideContextualSearch(StateChangeReason.BASE_PAGE_TAP);
            }
        }
    }

    @Override
    public void handleSelectionCleared() {
        // The selection was just cleared, so we'll want to remove our UX unless it was due to
        // another Tap while the Bar is showing.
        mInternalStateController.enter(InternalState.SELECTION_CLEARED_RECOGNIZED);
    }

    @Override
    public void logNonHeuristicFeatures(ContextualSearchInteractionRecorder rankerLogger) {
        boolean didOptIn = !mPolicy.isUserUndecided();
        rankerLogger.logFeature(ContextualSearchInteractionRecorder.Feature.DID_OPT_IN, didOptIn);
        boolean isHttp = mPolicy.isBasePageHTTP(getBasePageURL());
        rankerLogger.logFeature(ContextualSearchInteractionRecorder.Feature.IS_HTTP, isHttp);
        String contentLanguage = mContext.getDetectedLanguage();
        boolean isLanguageMismatch = mTranslateController.needsTranslation(contentLanguage);
        rankerLogger.logFeature(ContextualSearchInteractionRecorder.Feature.IS_LANGUAGE_MISMATCH,
                isLanguageMismatch);
    }

    /** Shows the given selection as the Search Term in the Bar. */
    private void showSelectionAsSearchInBar(String selection) {
        if (isSearchPanelShowing()) mSearchPanel.setSearchTerm(selection);
    }

    // ============================================================================================
    // ContextualSearchInternalStateHandler implementation.
    // ============================================================================================

    @VisibleForTesting
    ContextualSearchInternalStateHandler getContextualSearchInternalStateHandler() {
        return new ContextualSearchInternalStateHandler() {
            @Override
            public void hideContextualSearchUi(@StateChangeReason int reason) {
                // Called when the IDLE state has been entered.
                if (mContext != null) mContext.destroy();
                mContext = null;
                if (mSearchPanel == null) return;

                // Make sure we write to Ranker and reset at the end of every search, even if the
                // panel was not showing because it was a suppressed tap.
                mSearchPanel.getPanelMetrics().writeInteractionOutcomesAndReset();
                if (isSearchPanelShowing()) {
                    mSearchPanel.closePanel(reason, false);
                } else {
                    if (mSelectionController.getSelectionType() == SelectionType.TAP) {
                        mSelectionController.clearSelection();
                    }
                }
            }

            @Override
            public void gatherSurroundingText() {
                if (mContext != null) mContext.destroy();
                mContext = new ContextualSearchContext() {
                    @Override
                    void onSelectionChanged() {
                        notifyObserversOfContextSelectionChanged();
                    }
                };

                boolean isTap = mSelectionController.getSelectionType() == SelectionType.TAP;
                if (!isTap && mDoSuppressContextualSearchForSmartSelection && mContext != null) {
                    // If Smart Selection is active we need to work around a race
                    // condition gathering surrounding text.  See issue 773330.
                    // Instead we just return the selection which is good enough for the assistant.
                    mInternalStateController.notifyStartingWorkOn(
                            InternalState.GATHERING_SURROUNDINGS);
                    String currentSelection = mSelectionController.getSelectedText();
                    mContext.setSurroundingText(getBaseWebContents(), currentSelection);
                    mInternalStateController.notifyFinishedWorkOn(
                            InternalState.GATHERING_SURROUNDINGS);
                    return;
                }

                if (isTap && mPolicy.shouldPreviousTapResolve()) {
                    mContext.setResolveProperties(
                            mPolicy.getHomeCountry(mActivity), mPolicy.maySendBasePageUrl());
                }
                WebContents webContents = getBaseWebContents();
                if (webContents != null) {
                    mInternalStateController.notifyStartingWorkOn(
                            InternalState.GATHERING_SURROUNDINGS);
                    nativeGatherSurroundingText(
                            mNativeContextualSearchManagerPtr, mContext, webContents);
                } else {
                    mInternalStateController.reset(StateChangeReason.UNKNOWN);
                }
            }

            /** First step where we're committed to processing the current Tap gesture. */
            @Override
            public void tapGestureCommit() {
                mInternalStateController.notifyStartingWorkOn(InternalState.TAP_GESTURE_COMMIT);
                if (!mPolicy.isTapSupported()) {
                    hideContextualSearch(StateChangeReason.UNKNOWN);
                    return;
                }
                // We may be processing a chained search (aka a retap -- a tap near a previous tap).
                // If it's chained we need to log the outcomes and reset, because we won't be hiding
                // the panel at the end of the previous search (we'll update it to the new Search).
                if (isSearchPanelShowing()) {
                    mSearchPanel.getPanelMetrics().writeInteractionOutcomesAndReset();
                }
                // Set up the next batch of Ranker logging.
                mTapSuppressionInteractionRecorder.setupLoggingForPage(getBaseWebContents());
                mSearchPanel.getPanelMetrics().setInteractionRecorder(
                        mTapSuppressionInteractionRecorder);
                ContextualSearchUma.logRankerFeaturesAvailable(false);
                mInternalStateController.notifyFinishedWorkOn(InternalState.TAP_GESTURE_COMMIT);
            }

            /** Starts the process of deciding if we'll suppress the current Tap gesture or not. */
            @Override
            public void decideSuppression() {
                mInternalStateController.notifyStartingWorkOn(InternalState.DECIDING_SUPPRESSION);
                // TODO(donnd): Move handleShouldSuppressTap out of the Selection Controller.
                mSelectionController.handleShouldSuppressTap(
                        mContext, mTapSuppressionInteractionRecorder);
            }

            /** Starts showing the Tap UI by selecting a word around the current caret. */
            @Override
            public void startShowingTapUi() {
                WebContents baseWebContents = getBaseWebContents();
                if (baseWebContents != null) {
                    mInternalStateController.notifyStartingWorkOn(
                            InternalState.START_SHOWING_TAP_UI);
                    mSelectWordAroundCaretCounter++;
                    baseWebContents.selectWordAroundCaret();
                    // Let the policy know that a valid tap gesture has been received.
                    mPolicy.registerTap();
                } else {
                    mInternalStateController.reset(StateChangeReason.UNKNOWN);
                }
            }

            /**
             * Waits for possible Tap gesture that's near enough to the previous tap to be
             * considered a "re-tap". We've done some work on the previous Tap and we just saw the
             * selection get cleared (probably due to a Tap that may or may not be valid).
             * If it's invalid we'll want to hide the UI.  If it's valid we'll want to just update
             * the UI rather than having the Bar hide and re-show.
             */
            @Override
            public void waitForPossibleTapNearPrevious() {
                mInternalStateController.notifyStartingWorkOn(
                        InternalState.WAITING_FOR_POSSIBLE_TAP_NEAR_PREVIOUS);
                new Handler().postDelayed(new Runnable() {
                    @Override
                    public void run() {
                        mInternalStateController.notifyFinishedWorkOn(
                                InternalState.WAITING_FOR_POSSIBLE_TAP_NEAR_PREVIOUS);
                    }
                }, TAP_NEAR_PREVIOUS_DETECTION_DELAY_MS);
            }

            /**
             * Waits for possible Tap gesture that's on a previously established tap-selection.
             * If the current Tap was on the previous tap-selection then this selection will become
             * a Long-press selection and we'll recognize that gesture and start processing it.
             * If that doesn't happen within our time window (which is the common case) then we'll
             * advance to the next state in normal Tap processing.
             */
            @Override
            public void waitForPossibleTapOnTapSelection() {
                mInternalStateController.notifyStartingWorkOn(
                        InternalState.WAITING_FOR_POSSIBLE_TAP_ON_TAP_SELECTION);
                new Handler().postDelayed(new Runnable() {
                    @Override
                    public void run() {
                        mInternalStateController.notifyFinishedWorkOn(
                                InternalState.WAITING_FOR_POSSIBLE_TAP_ON_TAP_SELECTION);
                    }
                }, TAP_ON_TAP_SELECTION_DELAY_MS);
            }

            /** Starts a Resolve request to our server for the best Search Term. */
            @Override
            public void resolveSearchTerm() {
                mInternalStateController.notifyStartingWorkOn(InternalState.RESOLVING);

                String selection = mSelectionController.getSelectedText();
                assert !TextUtils.isEmpty(selection);
                mNetworkCommunicator.startSearchTermResolutionRequest(selection);
                // If the we were unable to start the resolve, we've hidden the UI and set the
                // context to null.
                if (mContext == null || mSearchPanel == null) return;

                // Update the UI to show the resolve is in progress.
                mSearchPanel.setContextDetails(
                        selection, mContext.getTextContentFollowingSelection());
            }

            @Override
            public void showContextualSearchTapUi() {
                mInternalStateController.notifyStartingWorkOn(InternalState.SHOW_FULL_TAP_UI);
                showContextualSearch(StateChangeReason.TEXT_SELECT_TAP);
                ContextualSearchUma.logRankerFeaturesAvailable(true);
                mInternalStateController.notifyFinishedWorkOn(InternalState.SHOW_FULL_TAP_UI);
            }

            @Override
            public void showContextualSearchLongpressUi() {
                mInternalStateController.notifyStartingWorkOn(
                        InternalState.SHOWING_LONGPRESS_SEARCH);
                boolean suppressForSmartSelection = mDoSuppressContextualSearchForSmartSelection
                        && !ContextualSearchFieldTrial.isSuppressForSmartSelectionDisabled();
                if (suppressForSmartSelection) {
                    // Make sure we close any existing UX since Smart Select has taken this action.
                    // Specifically, this happens when the user taps on an existing tap-selection
                    // and the selection-pins show: The original tap processing may still be in
                    // progress or may have completed and the Bar is being shown.
                    hideContextualSearch(StateChangeReason.UNKNOWN);
                    RecordUserAction.record("ContextualSearch.SmartSelectionSuppressed");
                } else {
                    showContextualSearch(StateChangeReason.TEXT_SELECT_LONG_PRESS);
                }
                mInternalStateController.notifyFinishedWorkOn(
                        InternalState.SHOWING_LONGPRESS_SEARCH);
            }
        };
    }

    /**
     * @param reporter A context reporter for the feature to report the current selection when
     *                 triggered.
     */
    public void enableContextReporting(ContextReporterDelegate reporter) {
        mContextReportingObserver = new ContextualSearchObserver() {
            @Override
            public void onShowContextualSearch(GSAContextDisplaySelection contextSelection) {
                if (contextSelection != null) reporter.reportDisplaySelection(contextSelection);
            }

            @Override
            public void onHideContextualSearch() {
                reporter.reportDisplaySelection(null);
            }
        };
        addObserver(mContextReportingObserver);
    }

    /**
     * Disable context reporting for Contextual Search.
     */
    public void disableContextReporting() {
        removeObserver(mContextReportingObserver);
        mContextReportingObserver = null;
    }

    // ============================================================================================
    // Test helpers
    // ============================================================================================

    /**
     * Sets the {@link ContextualSearchNetworkCommunicator} to use for server requests.
     * @param networkCommunicator The communicator for all future requests.
     */
    @VisibleForTesting
    void setNetworkCommunicator(ContextualSearchNetworkCommunicator networkCommunicator) {
        mNetworkCommunicator = networkCommunicator;
        mPolicy.setNetworkCommunicator(mNetworkCommunicator);
    }

    /** @return The ContextualSearchPolicy currently being used. */
    @VisibleForTesting
    ContextualSearchPolicy getContextualSearchPolicy() {
        return mPolicy;
    }

    /** @param policy The {@link ContextualSearchPolicy} for testing. */
    @VisibleForTesting
    void setContextualSearchPolicy(ContextualSearchPolicy policy) {
        mPolicy = policy;
    }

    /** @return The {@link ContextualSearchPanel}, for testing purposes only. */
    @VisibleForTesting
    ContextualSearchPanel getContextualSearchPanel() {
        return mSearchPanel;
    }

    /** @return The selection controller, for testing purposes. */
    @VisibleForTesting
    ContextualSearchSelectionController getSelectionController() {
        return mSelectionController;
    }

    /** @param controller The {@link ContextualSearchSelectionController}, for testing purposes. */
    @VisibleForTesting
    void setSelectionController(ContextualSearchSelectionController controller) {
        mSelectionController = controller;
    }

    /** @return The current search request, or {@code null} if there is none, for testing. */
    @VisibleForTesting
    ContextualSearchRequest getRequest() {
        return mSearchRequest;
    }

    @VisibleForTesting
    ContextualSearchTabPromotionDelegate getTabPromotionDelegate() {
        return mTabPromotionDelegate;
    }

    @VisibleForTesting
    void setContextualSearchInternalStateController(
            ContextualSearchInternalStateController controller) {
        mInternalStateController = controller;
    }

    @VisibleForTesting
    protected ContextualSearchInternalStateController getContextualSearchInternalStateController() {
        return mInternalStateController;
    }

    @VisibleForTesting
    ContextualSearchInteractionRecorder getRankerLogger() {
        return mTapSuppressionInteractionRecorder;
    }

    // ============================================================================================
    // Native calls
    // ============================================================================================

    private native long nativeInit();
    private native void nativeDestroy(long nativeContextualSearchManager);
    private native void nativeStartSearchTermResolutionRequest(long nativeContextualSearchManager,
            ContextualSearchContext contextualSearchContext, WebContents baseWebContents);
    protected native void nativeGatherSurroundingText(long nativeContextualSearchManager,
            ContextualSearchContext contextualSearchContext, WebContents baseWebContents);
    private native void nativeWhitelistContextualSearchJsApiUrl(
            long nativeContextualSearchManager, String url);
    private native void nativeEnableContextualSearchJsApiForWebContents(
            long nativeContextualSearchManager, WebContents overlayWebContents);
    // Don't call these directly, instead call the private methods that cache the results.
    private native String nativeGetTargetLanguage(long nativeContextualSearchManager);
    private native String nativeGetAcceptLanguages(long nativeContextualSearchManager);
}
