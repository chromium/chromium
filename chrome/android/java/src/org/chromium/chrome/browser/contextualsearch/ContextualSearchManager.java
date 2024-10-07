// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.app.Activity;
import android.net.Uri;
import android.os.Handler;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.view.ViewTreeObserver.OnGlobalFocusChangeListener;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.base.SysUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.blink_public.input.SelectionGranularity;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.PanelState;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelContentDelegate;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelStateProvider;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.RelatedSearchesControl;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchInternalStateController.InternalState;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchSelectionController.SelectionType;
import org.chromium.chrome.browser.contextualsearch.ResolvedSearchTerm.CardTag;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.layouts.SceneOverlay;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingResultType;
import org.chromium.components.external_intents.ExternalNavigationParams;
import org.chromium.components.external_intents.RedirectHandler;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.SelectAroundCaretResult;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.touch_selection.SelectionEventType;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Manages the Contextual Search feature. This class keeps track of the status of Contextual Search
 * and coordinates the control with the layout.
 *
 * <p>This class is driven by {@link ContextualSearchInternalStateController} through the {@link
 * ContextualSearchInternalStateHandler} interface to advance each stage of processing events. The
 * events are fed in by {@link ContextualSearchSelectionController} and business decisions are made
 * in the {@link ContextualSearchPolicy} class.
 *
 * <p>There is a native class corresponding to this class that communicates with the server through
 * a delegate. The server interaction is vectored through an interface to allow a stub for testing
 * in {@Link ContextualSearchNetworkCommunicator}.
 *
 * <p>The lifetime of this class corresponds to the Activity, and this class creates and owns a
 * {@link ContextualSearchPanel} with the same lifetime.
 */
public class ContextualSearchManager
        implements ContextualSearchManagementDelegate,
                ContextualSearchNetworkCommunicator,
                ContextualSearchSelectionHandler,
                ChromeAccessibilityUtil.Observer {
    // TODO(donnd): provide an inner class that implements some of these interfaces rather than
    // having the manager itself implement the interface because that exposes all the public methods
    // of that interface at the manager level.

    private static final String INTENT_URL_PREFIX = "intent:";

    // We denylist this URL because malformed URLs may bring up this page.
    private static final String DENYLISTED_URL = ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL;

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

    // A separator that we expect in the title of a dictionary response.
    private static final String DEFINITION_MID_DOT = "\u00b7";

    private final ObserverList<ContextualSearchObserver> mObservers =
            new ObserverList<ContextualSearchObserver>();

    private final Activity mActivity;
    private final Profile mProfile;
    private final ContextualSearchTabPromotionDelegate mTabPromotionDelegate;
    private final ViewTreeObserver.OnGlobalFocusChangeListener mOnFocusChangeListener;
    private final FullscreenManager.Observer mFullscreenObserver;

    private final ContextualSearchTranslation mTranslateController;
    private final ContextualSearchSelectionClient mContextualSearchSelectionClient;

    private final ScrimCoordinator mScrimCoordinator;

    /** The fullscreen state of the browser. */
    private final FullscreenManager mFullscreenManager;

    /** The state of the browser controls. */
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;

    /** A window for the creation of the overlay panel. */
    private final WindowAndroid mWindowAndroid;

    /** A means of observing all the browser's tabs. */
    private final TabModelSelector mTabModelSelector;

    /** A supplier of the last time the user interacted with the browser. */
    private final Supplier<Long> mLastUserInteractionTimeSupplier;

    private final Supplier<EdgeToEdgeController> mEdgeToEdgeControllerSupplier;

    private ContextualSearchSelectionController mSelectionController;
    private ContextualSearchNetworkCommunicator mNetworkCommunicator;
    @NonNull private ContextualSearchPolicy mPolicy;
    private ContextualSearchInternalStateController mInternalStateController;

    // The panel.
    private ContextualSearchPanel mSearchPanel;
    private ObservableSupplierImpl<OverlayPanelStateProvider> mOverlayPanelStateProviderSupplier =
            new ObservableSupplierImpl<>();

    // The native manager associated with this object.
    private long mNativeContextualSearchManagerPtr;

    private ViewGroup mParentView;
    private RedirectHandler mRedirectHandler;
    private TabModelSelectorTabModelObserver mTabModelObserver;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;

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

    /**
     * Whether contextual search manager is currently promoting a tab. We should be ignoring hide
     * requests when mIsPromotingTab is set to true.
     */
    private boolean mIsPromotingToTab;

    private ContextualSearchRequest mSearchRequest;
    private ContextualSearchRequest mLastSearchRequestLoaded;

    private RelatedSearchesList mRelatedSearches;

    /** Whether any current Search shown in the SERP is from Related Searches. */
    private boolean mIsRelatedSearchesSerp;

    /**
     * For Related Searches we need to remember the ResolvedSearchTerm for the default query so we
     * can switch back to it.
     */
    private ResolvedSearchTerm mResolvedSearchTerm;

    /** Whether the Accessibility Mode is enabled. */
    private boolean mIsAccessibilityModeEnabled;

    /** Whether bottom sheet is visible. */
    private boolean mIsBottomSheetVisible;

    // Counter for how many times we've called SelectAroundCaret without an ACK returned.
    // TODO(donnd): replace with a more systematic approach using the InternalStateController.
    private int mSelectAroundCaretCounter;

    /** A means of accessing the currently active tab. */
    private Supplier<Tab> mTabSupplier;

    /** A means of observing scene changes and attaching overlays. */
    private LayoutManagerImpl mLayoutManager;

    /** The pixel density. */
    private final float mDpToPx;

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
     *
     * @param activity The {@link Activity} in use.
     * @param profile The Profile associated with this ContextualSearchManager.
     * @param tabPromotionDelegate The {@link ContextualSearchTabPromotionDelegate} that is
     *     responsible for building tabs from contextual search {@link WebContents}.
     * @param scrimCoordinator A mechanism for showing and hiding the shared scrim.
     * @param tabSupplier Access to the tab that is currently active.
     * @param fullscreenManager Access to the fullscreen state.
     * @param browserControlsStateProvider Access to the current state of the browser controls.
     * @param windowAndroid A window to create the overlay panel with.
     * @param tabModelSelector A means of observing all tabs in the browser.
     * @param lastUserInteractionTimeSupplier A supplier of the last time a user interacted with the
     *     browser.
     * @param edgeToEdgeControllerSupplier Supplies an {@link EdgeToEdgeController} when available.
     */
    public ContextualSearchManager(
            Activity activity,
            Profile profile,
            ContextualSearchTabPromotionDelegate tabPromotionDelegate,
            ScrimCoordinator scrimCoordinator,
            Supplier<Tab> tabSupplier,
            FullscreenManager fullscreenManager,
            BrowserControlsStateProvider browserControlsStateProvider,
            WindowAndroid windowAndroid,
            TabModelSelector tabModelSelector,
            Supplier<Long> lastUserInteractionTimeSupplier,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier) {
        mActivity = activity;
        mProfile = profile;
        mTabPromotionDelegate = tabPromotionDelegate;
        mScrimCoordinator = scrimCoordinator;
        mTabSupplier = tabSupplier;
        mFullscreenManager = fullscreenManager;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mWindowAndroid = windowAndroid;
        mTabModelSelector = tabModelSelector;
        mLastUserInteractionTimeSupplier = lastUserInteractionTimeSupplier;
        mEdgeToEdgeControllerSupplier = edgeToEdgeControllerSupplier;
        mDpToPx = mActivity.getResources().getDisplayMetrics().density;

        final View controlContainer = mActivity.findViewById(R.id.control_container);
        mOnFocusChangeListener =
                new OnGlobalFocusChangeListener() {
                    @Override
                    public void onGlobalFocusChanged(View oldFocus, View newFocus) {
                        if (controlContainer != null && controlContainer.hasFocus()) {
                            hideContextualSearch(StateChangeReason.UNKNOWN);
                        }
                    }
                };

        mFullscreenObserver =
                new FullscreenManager.Observer() {
                    @Override
                    public void onEnterFullscreen(Tab tab, FullscreenOptions options) {
                        hideContextualSearch(StateChangeReason.UNKNOWN);
                    }

                    @Override
                    public void onExitFullscreen(Tab tab) {
                        hideContextualSearch(StateChangeReason.UNKNOWN);
                    }
                };

        mFullscreenManager.addObserver(mFullscreenObserver);
        mSelectionController =
                new ContextualSearchSelectionController(activity, this, mTabSupplier);
        mNetworkCommunicator = this;
        mPolicy = new ContextualSearchPolicy(mProfile, mSelectionController, mNetworkCommunicator);
        mTranslateController = new ContextualSearchTranslationImpl(mProfile);
        mInternalStateController =
                new ContextualSearchInternalStateController(
                        mPolicy, getContextualSearchInternalStateHandler());
        mContextualSearchSelectionClient = new ContextualSearchSelectionClient();
    }

    /**
     * Initializes this manager.
     *
     * @param parentView The parent view to attach Contextual Search UX to.
     * @param layoutManager A means of attaching the OverlayPanel to the scene.
     * @param bottomSheetController The {@link BottomSheetController} that is used to show {@link
     *     BottomSheetContent}.
     * @param compositorViewHolder The {@link CompositorViewHolder} for the current activity.
     * @param toolbarHeightDp The height of the toolbar in dp.
     * @param toolbarManager The manager of the toolbar, used to query toolbar state.
     * @param canPromoteToNewTab Whether the Conextual search panel can be promoted to a new tab.
     * @param intentRequestTracker The {@link IntentRequestTracker} of the current activity.
     */
    public void initialize(
            @NonNull ViewGroup parentView,
            @NonNull LayoutManagerImpl layoutManager,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull CompositorViewHolder compositorViewHolder,
            float toolbarHeightDp,
            @NonNull ToolbarManager toolbarManager,
            boolean canPromoteToNewTab,
            @NonNull IntentRequestTracker intentRequestTracker) {
        mNativeContextualSearchManagerPtr = ContextualSearchManagerJni.get().init(this, mProfile);

        mParentView = parentView;
        mParentView.getViewTreeObserver().addOnGlobalFocusChangeListener(mOnFocusChangeListener);

        mLayoutManager = layoutManager;

        ContextualSearchPanel panel =
                new ContextualSearchPanel(
                        mActivity,
                        mLayoutManager,
                        mLayoutManager.getOverlayPanelManager(),
                        mBrowserControlsStateProvider,
                        mWindowAndroid,
                        mProfile,
                        compositorViewHolder,
                        toolbarHeightDp,
                        toolbarManager,
                        canPromoteToNewTab,
                        mTabSupplier,
                        mEdgeToEdgeControllerSupplier);
        panel.setManagementDelegate(this);

        setContextualSearchPanel(panel);
        mLayoutManager.addSceneOverlay((SceneOverlay) panel);

        mRedirectHandler = RedirectHandler.create();

        mIsShowingPromo = false;
        mDidStartLoadingResolvedSearchRequest = false;
        mWereSearchResultsSeen = false;
        mIsInitialized = true;

        mInternalStateController.reset(StateChangeReason.UNKNOWN);

        listenForTabModelSelectorNotifications();
        ChromeAccessibilityUtil.get().addObserver(this);
    }

    /**
     * Destroys the native Contextual Search Manager.
     * Call this method before orphaning this object to allow it to be garbage collected.
     */
    public void destroy() {
        if (!mIsInitialized) return;

        hideContextualSearch(StateChangeReason.UNKNOWN);
        mFullscreenManager.removeObserver(mFullscreenObserver);
        mParentView.getViewTreeObserver().removeOnGlobalFocusChangeListener(mOnFocusChangeListener);
        ContextualSearchManagerJni.get().destroy(mNativeContextualSearchManagerPtr, this);
        stopListeningForHideNotifications();
        mRedirectHandler.clear();
        mInternalStateController.enter(InternalState.UNDEFINED);
        ChromeAccessibilityUtil.get().removeObserver(this);

        if (mSearchPanel != null) mSearchPanel.destroy();
        mSearchPanel = null;
        mOverlayPanelStateProviderSupplier.set(null);
    }

    @Override
    public void setContextualSearchPanel(ContextualSearchPanel panel) {
        assert panel != null;
        mSearchPanel = panel;
        mOverlayPanelStateProviderSupplier.set(mSearchPanel);
        mPolicy.setContextualSearchPanel(panel);
    }

    @Override
    public Activity getActivity() {
        return mActivity;
    }

    /** @return Whether the Search Panel is opened. That is, whether it is EXPANDED or MAXIMIZED. */
    public boolean isSearchPanelOpened() {
        return mSearchPanel != null && mSearchPanel.isPanelOpened();
    }

    /** @return Whether the {@code mSearchPanel} is not {@code null} and is showing. */
    boolean isSearchPanelShowing() {
        return mSearchPanel != null && mSearchPanel.isShowing();
    }

    /** @return Whether the {@code mSearchPanel} is not {@code null} and is currently active. */
    boolean isSearchPanelActive() {
        return mSearchPanel != null && mSearchPanel.isActive();
    }

    /**
     * @return the {@link WebContents} of the {@code mSearchPanel} or {@code null} if
     *         {@code mSearchPanel} is null or the search panel doesn't currently hold one.
     */
    private @Nullable WebContents getSearchPanelWebContents() {
        return mSearchPanel == null ? null : mSearchPanel.getWebContents();
    }

    /** @return The Base Page's {@link WebContents}. */
    private @Nullable WebContents getBaseWebContents() {
        return mSelectionController.getBaseWebContents();
    }

    /** @return The Base Page's {@link GURL}. */
    private @Nullable GURL getBasePageURL() {
        WebContents baseWebContents = mSelectionController.getBaseWebContents();
        if (baseWebContents == null) return null;
        return baseWebContents.getVisibleUrl();
    }

    /** Notifies that the base page has started loading a page. */
    public void onBasePageLoadStarted() {
        mSelectionController.onBasePageLoadStarted();
    }

    /** Notifies that a Context Menu has been shown. */
    void onContextMenuShown() {
        mSelectionController.onContextMenuShown();
    }

    @Override
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

        if (mWereSearchResultsSeen) {
            // Clear the selection, since the user just acted upon it by looking at the panel.
            // However if the selection is invalid we don't need to clear it.
            // The invalid selection might also be due to a "select-all" action by the user.
            if (reason != StateChangeReason.INVALID_SELECTION) {
                mSelectionController.clearSelection();
            }
        } else if (mLoadedSearchUrlTimeMs != 0L) {
            removeLastSearchVisit();
        }

        // Clear the timestamp. This is to avoid future calls to hideContextualSearch clearing
        // the current URL.
        mLoadedSearchUrlTimeMs = 0L;
        mWereSearchResultsSeen = false;

        mSearchRequest = null;
        mRelatedSearches = null;
        mIsRelatedSearchesSerp = false;

        mIsShowingPromo = false;
        mSearchPanel.setIsPromoActive(false);
        mSearchPanel.clearRelatedSearches();
        notifyHideContextualSearch();
    }

    @Override
    public void onPanelCollapsing() {
        if (mIsRelatedSearchesSerp && mResolvedSearchTerm != null) {
            // For now a literal search is not possible when we have Related Searches showing, but
            // may be a possibility once https://crbug.com/1223171 is done.
            final boolean isLiteralSearchPossible = false;
            displayResolvedSearchTerm(
                    mResolvedSearchTerm, mResolvedSearchTerm.searchTerm(), isLiteralSearchPossible);
        }
    }

    /**
     * Shows the Contextual Search UX.
     * @param stateChangeReason The reason explaining the change of state.
     */
    private void showContextualSearch(@StateChangeReason int stateChangeReason) {
        assert mSearchPanel != null;

        // Dismiss the undo SnackBar if present by committing all tab closures.
        mTabModelSelector.commitAllTabClosures();

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
        @PanelState int state = mSearchPanel.getPanelState();
        if (!mWereSearchResultsSeen
                && mLoadedSearchUrlTimeMs != 0L
                && state != PanelState.UNDEFINED
                && state != PanelState.CLOSED) {
            removeLastSearchVisit();
        }

        mSearchPanel.destroyContent();
        mReceivedContextualCardsEntityData = false;

        String selection = mSelectionController.getSelectedText();
        boolean canResolve = mPolicy.isResolvingGesture();
        if (canResolve) {
            // If we can resolve then we should not delay before loading content.
            mShouldLoadDelayedSearch = false;
        }
        if (canResolve && mPolicy.shouldPreviousGestureResolve()) {
            // For a resolving gestures we'll figure out translation need after the Resolve.
        } else if (!TextUtils.isEmpty(selection)) {
            // Build the literal search request for the selection.
            boolean shouldPrefetch = mPolicy.shouldPrefetchSearchResult();
            mSearchRequest = new ContextualSearchRequest(mProfile, selection, shouldPrefetch);
            mTranslateController.forceAutoDetectTranslateUnlessDisabled(mSearchRequest);
            mDidStartLoadingResolvedSearchRequest = false;
            mSearchPanel.setSearchTerm(selection);
            mIsRelatedSearchesSerp = false;
            if (shouldPrefetch) loadSearchUrl();
        } else {
            // The selection is no longer valid, so we can't build a request.  Don't show the UX.
            hideContextualSearch(StateChangeReason.UNKNOWN);
            return;
        }
        mWereSearchResultsSeen = false;

        // Note: now that the contextual search has properly started, set the promo involvement.
        if (mPolicy.isPromoAvailable()) {
            mIsShowingPromo = true;
            mSearchPanel.setIsPromoActive(true);
            mSearchPanel.setDidSearchInvolvePromo();
        }

        mSearchPanel.requestPanelShow(stateChangeReason);

        assert mSelectionController.getSelectionType() != SelectionType.UNDETERMINED;
        mWasActivatedByTap = mSelectionController.getSelectionType() == SelectionType.TAP;
    }

    @Override
    public void startSearchTermResolutionRequest(
            String selection, boolean isExactResolve, ContextualSearchContext searchContext) {
        ContextualSearchManagerJni.get()
                .startSearchTermResolutionRequest(
                        mNativeContextualSearchManagerPtr, this, mContext, getBaseWebContents());
        ContextualSearchUma.logResolveRequested(mSelectionController.isTapSelection());
    }

    @Override
    public @Nullable GURL getBasePageUrl() {
        WebContents baseWebContents = getBaseWebContents();
        if (baseWebContents == null) return null;
        return baseWebContents.getLastCommittedUrl();
    }

    /** Accessor for the {@code InfoBarContainer} currently attached to the {@code Tab}. */
    private InfoBarContainer getInfoBarContainer() {
        Tab tab = mTabSupplier.get();
        return tab == null ? null : InfoBarContainer.get(tab);
    }

    /** Listens for notifications that should hide the Contextual Search bar. */
    private void listenForTabModelSelectorNotifications() {
        TabModelSelector selector = mTabModelSelector;
        mTabModelObserver =
                new TabModelSelectorTabModelObserver(selector) {
                    @Override
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                        if ((!mIsPromotingToTab && tab.getId() != lastId)
                                || mTabModelSelector.isIncognitoSelected()) {
                            hideContextualSearch(StateChangeReason.UNKNOWN);
                            mSelectionController.onTabSelected();
                        }
                    }

                    @Override
                    public void didAddTab(
                            Tab tab,
                            @TabLaunchType int type,
                            @TabCreationState int creationState,
                            boolean markedForSelection) {
                        // If we're in the process of promoting this tab, just return and don't mess
                        // with this state.
                        if (tab.getWebContents() == getSearchPanelWebContents()) return;
                        hideContextualSearch(StateChangeReason.UNKNOWN);
                    }
                };
        mTabModelSelectorTabObserver =
                new TabModelSelectorTabObserver(selector) {
                    @Override
                    public void onPageLoadStarted(Tab tab, GURL url) {
                        // Detects navigation of the base page for crbug.com/428368
                        // (navigation-detection).
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
    }

    /** Stops listening for notifications that should hide the Contextual Search bar. */
    private void stopListeningForHideNotifications() {
        if (mTabModelObserver != null) mTabModelObserver.destroy();
        if (mTabModelSelectorTabObserver != null) mTabModelSelectorTabObserver.destroy();
        mTabModelObserver = null;
        mTabModelSelectorTabObserver = null;
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
     * Called by native code when the surrounding text and selection range are available. This is
     * done for both Tap and Long-press gestures.
     *
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
                mPolicy.logRelatedSearchesQualifiedUsers(getBasePageLanguage());
                mInternalStateController.notifyFinishedWorkOn(InternalState.GATHERING_SURROUNDINGS);
            }
        }
    }

    /**
     * Called in response to the {@link ContextualSearchManagerJni#startSearchTermResolutionRequest}
     * method. If {@code startSearchTermResolutionRequest} is called with a previous request sill
     * pending our native delegate is supposed to cancel all previous requests. So this code should
     * only be called with data corresponding to the most recent request.
     *
     * @param isNetworkUnavailable Indicates if the network is unavailable, in which case all other
     *     parameters should be ignored.
     * @param responseCode The HTTP response code. If the code is not OK, the query should be
     *     ignored.
     * @param searchTerm The term to use in our subsequent search.
     * @param displayText The text to display in our UX.
     * @param alternateTerm The alternate term to display on the results page.
     * @param mid the MID for an entity to use to trigger a Knowledge Panel, or an empty string. A
     *     MID is a unique identifier for an entity in the Search Knowledge Graph.
     * @param selectionStartAdjust A positive number of characters that the start of the existing
     *     selection should be expanded by.
     * @param selectionEndAdjust A positive number of characters that the end of the existing
     *     selection should be expanded by.
     * @param contextLanguage The language of the original search term, or an empty string.
     * @param thumbnailUrl The URL of the thumbnail to display in our UX.
     * @param caption The caption to display.
     * @param quickActionUri The URI for the intent associated with the quick action.
     * @param quickActionCategory The {@link QuickActionCategory} for the quick action.
     * @param searchUrlFull The URL for the full search to present in the overlay, or empty.
     * @param searchUrlPreload The URL for the search to preload into the overlay, or empty.
     * @param cocaCardTag The primary internal Coca card tag for the response, or {@code 0} if none.
     * @param relatedSearchesJson A blob of JSON that contains the Related Searches and config data.
     */
    @CalledByNative
    public void onSearchTermResolutionResponse(
            boolean isNetworkUnavailable,
            int responseCode,
            final String searchTerm,
            final String displayText,
            final String alternateTerm,
            final String mid,
            boolean doPreventPreload,
            int selectionStartAdjust,
            int selectionEndAdjust,
            final String contextLanguage,
            final String thumbnailUrl,
            final String caption,
            final String quickActionUri,
            @QuickActionCategory final int quickActionCategory,
            final String searchUrlFull,
            final String searchUrlPreload,
            @CardTag final int cocaCardTag,
            final String relatedSearchesJson) {
        ContextualSearchUma.logResolveReceived(mSelectionController.isTapSelection());
        ResolvedSearchTerm resolvedSearchTerm =
                new ResolvedSearchTerm.Builder(
                                isNetworkUnavailable,
                                responseCode,
                                searchTerm,
                                displayText,
                                alternateTerm,
                                mid,
                                doPreventPreload,
                                selectionStartAdjust,
                                selectionEndAdjust,
                                contextLanguage,
                                thumbnailUrl,
                                caption,
                                quickActionUri,
                                quickActionCategory,
                                searchUrlFull,
                                searchUrlPreload,
                                cocaCardTag,
                                relatedSearchesJson)
                        .build();
        mNetworkCommunicator.handleSearchTermResolutionResponse(resolvedSearchTerm);
    }

    @Override
    public void handleSearchTermResolutionResponse(ResolvedSearchTerm resolvedSearchTerm) {
        if (!mInternalStateController.isStillWorkingOn(InternalState.RESOLVING)) return;

        // Show an appropriate message for what to search for.
        String message;
        boolean doLiteralSearch = false;
        if (resolvedSearchTerm.isNetworkUnavailable()) {
            message =
                    mActivity
                            .getResources()
                            .getString(R.string.contextual_search_network_unavailable);
        } else if (!isHttpFailureCode(resolvedSearchTerm.responseCode())
                && !TextUtils.isEmpty(resolvedSearchTerm.displayText())) {
            message = resolvedSearchTerm.displayText();
        } else if (!mPolicy.shouldShowErrorCodeInBar()) {
            message = mSelectionController.getSelectedText();
            doLiteralSearch = true;
        } else {
            message =
                    mActivity
                            .getResources()
                            .getString(
                                    R.string.contextual_search_error,
                                    resolvedSearchTerm.responseCode());
            doLiteralSearch = true;
        }

        mRelatedSearches = new RelatedSearchesList(resolvedSearchTerm.relatedSearchesJson());
        mResolvedSearchTerm = resolvedSearchTerm;
        displayResolvedSearchTerm(resolvedSearchTerm, message, doLiteralSearch);

        // Adjust the selection unless the user changed it since we initiated the search.
        int selectionStartAdjust = resolvedSearchTerm.selectionStartAdjust();
        int selectionEndAdjust = resolvedSearchTerm.selectionEndAdjust();
        if ((selectionStartAdjust != 0 || selectionEndAdjust != 0)
                && (mSelectionController.getSelectionType() == SelectionType.TAP
                        || mSelectionController.getSelectionType()
                                == SelectionType.RESOLVING_LONG_PRESS)) {
            String originalSelection =
                    mContext == null ? null : mContext.getSelectionBeingResolved();
            String currentSelection = mSelectionController.getSelectedText();
            if (currentSelection != null) currentSelection = currentSelection.trim();
            if (originalSelection != null && originalSelection.trim().equals(currentSelection)) {
                mSelectionController.adjustSelection(selectionStartAdjust, selectionEndAdjust);
                mContext.onSelectionAdjusted(selectionStartAdjust, selectionEndAdjust);
            }
        }

        mInternalStateController.notifyFinishedWorkOn(InternalState.RESOLVING);
    }

    /**
     * Displays the given {@link ResolvedSearchTerm} in the panel and logs the action.
     *
     * @param resolvedSearchTerm The bundle of data from the server to be displayed
     * @param message The main message to display in the Bar. This is usually the same as the
     *     SearchTerm except in cases where an error is returned by the server.
     * @param doLiteralSearch Whether this is a literal search for the verbatim selection or a
     *     resolved search.
     */
    void displayResolvedSearchTerm(
            ResolvedSearchTerm resolvedSearchTerm, String message, boolean doLiteralSearch) {
        boolean receivedCaptionOrThumbnail =
                !TextUtils.isEmpty(resolvedSearchTerm.caption())
                        || !TextUtils.isEmpty(resolvedSearchTerm.thumbnailUrl());

        assert mSearchPanel != null;
        // If there was an error, fall back onto a literal search for the selection.
        // Since we're showing the panel, there must be a selection.
        String searchTerm = resolvedSearchTerm.searchTerm();
        String alternateTerm = resolvedSearchTerm.alternateTerm();
        boolean doPreventPreload = resolvedSearchTerm.doPreventPreload();
        if (doLiteralSearch) {
            searchTerm = mSelectionController.getSelectedText();
            alternateTerm = null;
            doPreventPreload = true;
        }

        List<String> inBarRelatedSearches = buildRelatedSearches(searchTerm);

        // Check if the searchTerm is a composite (used for Definitions for pronunciation).
        // The middle-dot character is returned by the server and marks the beginning of the
        // pronunciation.
        String pronunciation = null;
        int dotSeparatorLocation = searchTerm.indexOf(DEFINITION_MID_DOT);
        if (dotSeparatorLocation > 0 && dotSeparatorLocation < searchTerm.length()) {
            assert resolvedSearchTerm.cardTagEnum() == CardTag.CT_DEFINITION
                    || resolvedSearchTerm.cardTagEnum() == CardTag.CT_CONTEXTUAL_DEFINITION;
            // Style with the pronunciation in gray in the second half.
            String word = searchTerm.substring(0, dotSeparatorLocation);
            pronunciation = searchTerm.substring(dotSeparatorLocation + 1);
            pronunciation =
                    LocalizationUtils.isLayoutRtl()
                            ? pronunciation + DEFINITION_MID_DOT
                            : DEFINITION_MID_DOT + pronunciation;
            searchTerm = word;
            message = word;
        }

        mSearchPanel.onSearchTermResolved(
                message,
                pronunciation,
                resolvedSearchTerm.thumbnailUrl(),
                resolvedSearchTerm.quickActionUri(),
                resolvedSearchTerm.quickActionCategory(),
                resolvedSearchTerm.cardTagEnum(),
                inBarRelatedSearches);
        if (!TextUtils.isEmpty(resolvedSearchTerm.caption())) {
            setCaption(resolvedSearchTerm.caption());
        }

        boolean quickActionShown =
                mSearchPanel.getSearchBarControl().getQuickActionControl().hasQuickAction();
        mReceivedContextualCardsEntityData = !quickActionShown && receivedCaptionOrThumbnail;
        ContextualSearchUma.logContextualCardsDataShown(mReceivedContextualCardsEntityData);
        mSearchPanel.getPanelMetrics().setCardShown(resolvedSearchTerm.cardTagEnum());
        ContextualSearchUma.logQuickActionShown(
                quickActionShown, resolvedSearchTerm.quickActionCategory());
        mSearchPanel
                .getPanelMetrics()
                .setWasQuickActionShown(quickActionShown, resolvedSearchTerm.quickActionCategory());

        if (!TextUtils.isEmpty(searchTerm)) {
            // TODO(donnd): Instead of preloading, we should prefetch (ie the URL should not
            // appear in the user's history until the user views it).  See crbug.com/406446.
            boolean shouldPreload = !doPreventPreload && mPolicy.shouldPrefetchSearchResult();
            mSearchRequest =
                    new ContextualSearchRequest(
                            mProfile,
                            searchTerm,
                            alternateTerm,
                            resolvedSearchTerm.mid(),
                            shouldPreload,
                            resolvedSearchTerm.searchUrlFull(),
                            resolvedSearchTerm.searchUrlPreload());
            // Trigger translation, if enabled.
            mTranslateController.forceTranslateIfNeeded(
                    mSearchRequest,
                    resolvedSearchTerm.contextLanguage(),
                    mSelectionController.isTapSelection());
            mDidStartLoadingResolvedSearchRequest = false;
            if (mSearchPanel.isContentShowing()) {
                mSearchRequest.setNormalPriority();
            }
            if (mSearchPanel.isContentShowing() || shouldPreload) {
                loadSearchUrl();
            }
        }
    }

    /**
     * Prepares the current {@link ContextualSearchContext} for an upcoming resolve request by
     * setting properties like the appropriate languages for translation.
     */
    private void prepareContextResolveProperties() {
        String targetLanguage = mTranslateController.getTranslateServiceTargetLanguage();
        targetLanguage = targetLanguage != null ? targetLanguage : "";
        String fluentLanguages = mTranslateController.getTranslateServiceFluentLanguages();
        fluentLanguages = fluentLanguages != null ? fluentLanguages : "";
        mContext.setResolveProperties(
                mPolicy.getHomeCountry(mActivity),
                mPolicy.doSendBasePageUrl(),
                targetLanguage,
                fluentLanguages);
    }

    /** Issues a resolve request for the current selection. */
    private void issueResolveRequest() {
        boolean isExactSearch = mSelectionController.isAdjustedSelection();
        mContext.prepareToResolve(
                isExactSearch, mPolicy.getRelatedSearchesStamp(getBasePageLanguage()));
        mNetworkCommunicator.startSearchTermResolutionRequest(
                mSelectionController.getSelectedText(), isExactSearch, mContext);
    }

    /** Resets internal state that should be reset whenever a Search ends (panel is closed). */
    void resetStateAfterSearch() {
        mResolvedSearchTerm = null;
    }

    /**
     * @return Whether the device is currently online.
     */
    boolean isDeviceOnline() {
        return NetworkChangeNotifier.isOnline();
    }

    /** Loads a Search Request in the Contextual Search's Content View. */
    private void loadSearchUrl() {
        assert mSearchPanel != null;
        mLoadedSearchUrlTimeMs = System.currentTimeMillis();
        mLastSearchRequestLoaded = mSearchRequest;
        mSearchPanel.loadUrlInPanel(mSearchRequest.getSearchUrl());
        mDidStartLoadingResolvedSearchRequest = true;

        // TODO(donnd): If the user taps on a word and quickly after that taps on the peeking Search
        // Bar, the Search Content View will not be displayed. It seems that calling
        // WebContents.updateWebContentsVisibility() while it's being created has no effect. For
        // now, we force the ContentView to be displayed by calling updateWebContentsVisibility()
        // again when a URL is being loaded. See: crbug.com/398206
        if (mSearchPanel.isContentShowing() && getSearchPanelWebContents() != null) {
            getSearchPanelWebContents().updateWebContentsVisibility(Visibility.VISIBLE);
        }
    }

    /**
     * Called to set a caption to show in a second line in the Bar.
     * @param caption The caption to display.
     */
    private void setCaption(String caption) {
        // Notify the UI of the caption.
        mSearchPanel.setCaption(caption);
    }

    @Override
    public void onAccessibilityModeChanged(boolean enabled) {
        mIsAccessibilityModeEnabled = enabled;
        if (enabled) hideContextualSearch(StateChangeReason.UNKNOWN);
    }

    /** Update bottom sheet visibility state. */
    public void onBottomSheetVisible(boolean visible) {
        mIsBottomSheetVisible = visible;
        if (visible) hideContextualSearch(StateChangeReason.RESET);
    }

    /** Notifies that the preference state has changed. */
    public void onContextualSearchPrefChanged() {
        // The pref may be automatically changed during application startup due to enterprise
        // configuration settings, so we may not have a panel yet.
        if (mSearchPanel != null && mProfile != null) {
            // Nitifies panel that if the user opted in or not.
            boolean userOptedIn =
                    ContextualSearchPolicy.isContextualSearchPrefFullyOptedIn(mProfile);
            mSearchPanel.onContextualSearchPrefChanged(userOptedIn);
        }
    }

    @Override
    public void stopPanelContentsNavigation() {
        if (getSearchPanelWebContents() == null) return;

        getSearchPanelWebContents().stop();
    }

    /**
     * Tells the Panel whether it can ever hide the Browser Controls (Toolbar).
     * This is set to false by a Partial-height Chrome Custom Tab, and defaults to true.
     * @param canHideAndroidBrowserControls whether hiding is ever allowed.
     */
    public void setCanHideAndroidBrowserControls(boolean canHideAndroidBrowserControls) {
        mSearchPanel.setCanHideAndroidBrowserControls(canHideAndroidBrowserControls);
    }

    @VisibleForTesting
    public boolean getCanHideAndroidBrowserControls() {
        return mSearchPanel.getCanHideAndroidBrowserControls();
    }

    // ============================================================================================
    // Observers
    // ============================================================================================

    /** @param observer An observer to notify when the user performs a contextual search. */
    public void addObserver(ContextualSearchObserver observer) {
        mObservers.addObserver(observer);
    }

    /** @param observer An observer to no longer notify when the user performs a contextual search.
     */
    public void removeObserver(ContextualSearchObserver observer) {
        mObservers.removeObserver(observer);
    }

    /** Notifies all Contextual Search observers that a search has occurred. */
    private void notifyShowContextualSearch() {
        for (ContextualSearchObserver observer : mObservers) {
            observer.onShowContextualSearch();
        }
    }

    /** Notifies all Contextual Search observers that a search ended and is no longer in effect. */
    private void notifyHideContextualSearch() {
        for (ContextualSearchObserver observer : mObservers) {
            observer.onHideContextualSearch();
        }
    }

    // ============================================================================================
    // OverlayPanelContentDelegate
    // ============================================================================================

    @Override
    public OverlayPanelContentDelegate getOverlayPanelContentDelegate() {
        return new SearchOverlayPanelContentDelegate();
    }

    /** Implementation of OverlayPanelContentDelegate. Made public for testing purposes. */
    public class SearchOverlayPanelContentDelegate extends OverlayPanelContentDelegate {
        // Note: New navigation or changes to the WebContents are not advised in this class since
        // the WebContents is being observed and navigation is already being performed.

        public SearchOverlayPanelContentDelegate() {}

        @Override
        public void onMainFrameLoadStarted(String url, boolean isExternalUrl) {
            assert mSearchPanel != null;
            mSearchPanel.updateBrowserControlsState();

            if (isExternalUrl) {
                onExternalNavigation(url);
            }
        }

        @Override
        public void onMainFrameNavigation(
                String url, boolean isExternalUrl, boolean isFailure, boolean isError) {
            assert mSearchPanel != null;
            if (isExternalUrl) {
                if (mPolicy.isAmpUrl(url) && mSearchPanel.didTouchContent()) {
                    onExternalNavigation(url);
                }
            } else {
                // Could be just prefetching, check if that failed.
                onContextualSearchRequestNavigation(isFailure);
            }
        }

        @Override
        public void onContentLoadStarted() {
            mDidPromoteSearchNavigation = false;
        }

        @Override
        public void onVisibilityChanged(boolean isVisible) {
            if (isVisible) {
                mWereSearchResultsSeen = true;
                // If there's no current request, then either a search term resolution
                // is in progress or we should do a verbatim search now.
                if (mSearchRequest == null
                        && mPolicy.shouldCreateVerbatimRequest()
                        && !TextUtils.isEmpty(mSelectionController.getSelectedText())) {
                    mSearchRequest =
                            new ContextualSearchRequest(
                                    mProfile, mSelectionController.getSelectedText());
                    mDidStartLoadingResolvedSearchRequest = false;
                }
                if (mSearchRequest != null
                        && (!mDidStartLoadingResolvedSearchRequest || mShouldLoadDelayedSearch)) {
                    // mShouldLoadDelayedSearch is used in the non-preloading case to load content.
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
        public void onContentViewSeen() {
            assert mSearchPanel != null;
            if (!mIsRelatedSearchesSerp) mSearchPanel.setWasSearchContentViewSeen();
        }

        @Override
        public boolean shouldInterceptNavigation(
                ExternalNavigationHandler externalNavHandler,
                GURL escapedUrl,
                @PageTransition int pageTransition,
                boolean isRedirect,
                boolean hasUserGesture,
                boolean isRendererInitiated,
                GURL referrerUrl,
                boolean isInPrimaryMainFrame,
                boolean isExternalProtocol) {
            assert mSearchPanel != null;
            mRedirectHandler.updateNewUrlLoading(
                    pageTransition,
                    isRedirect,
                    hasUserGesture,
                    mLastUserInteractionTimeSupplier.get(),
                    RedirectHandler.NO_COMMITTED_ENTRY_INDEX,
                    /* isInitialNavigation= */ true,
                    isRendererInitiated);
            ExternalNavigationParams params =
                    new ExternalNavigationParams.Builder(
                                    escapedUrl, false, referrerUrl, pageTransition, isRedirect)
                            .setApplicationMustBeInForeground(true)
                            .setRedirectHandler(mRedirectHandler)
                            .setIsMainFrame(isInPrimaryMainFrame)
                            .build();
            if (externalNavHandler.shouldOverrideUrlLoading(params).getResultType()
                    != OverrideUrlLoadingResultType.NO_OVERRIDE) {
                return false;
            }
            return !isExternalProtocol;
        }

        @Override
        public void onFirstNonEmptyPaint() {
            mSearchPanel.getPanelMetrics().onFirstNonEmptyPaint(mSearchRequest.wasPrefetch());
        }
    }

    // ============================================================================================
    // Search Content View
    // ============================================================================================

    /** Removes the last resolved search URL from the Chrome history. */
    private void removeLastSearchVisit() {
        assert mSearchPanel != null;
        if (mLastSearchRequestLoaded != null) {
            ContextualSearchManagerJni.get()
                    .removeLastHistoryEntry(
                            mNativeContextualSearchManagerPtr,
                            this,
                            mLastSearchRequestLoaded.getSearchUrl(),
                            mLoadedSearchUrlTimeMs);
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

    // ============================================================================================
    // ContextualSearchManagementDelegate Overrides
    // ============================================================================================

    @Override
    public void logCurrentState() {
        if (ContextualSearchFieldTrial.isEnabled()) mPolicy.logCurrentState();
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
        if (!mDidPromoteSearchNavigation
                && mSearchPanel != null
                && !DENYLISTED_URL.equals(url)
                && !url.startsWith(INTENT_URL_PREFIX)
                && shouldPromoteSearchNavigation()) {
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
            TabModelSelector tabModelSelector = mTabModelSelector;
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
            GURL gurl = getContentViewUrl(getSearchPanelWebContents());
            // TODO(yfriedman): crbug/783819 - Finish ContextualSearch migration to gurl.
            String url = gurl.getSpec();

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
    private GURL getContentViewUrl(WebContents searchWebContents) {
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
        resetStateAfterSearch();
    }

    @Override
    public ScrimCoordinator getScrimCoordinator() {
        return mScrimCoordinator;
    }

    @Override
    public void onRelatedSearchesSuggestionClicked(int suggestionIndex) {
        // The first suggestion is the default search, so the actual related searches start from
        // index 1.
        int defaultSearchAdjustment = RelatedSearchesControl.INDEX_OF_THE_FIRST_RELATED_SEARCHES;
        assert mRelatedSearches != null
                : "There is no valid list of Related Searches for this click! "
                        + "Please update crbug.com/1307267 with this repro.";
        assert (suggestionIndex - defaultSearchAdjustment) < mRelatedSearches.getQueries().size();

        // TODO(crbug.com/40828323) remove this check once we figure out how this can happen.
        if (mRelatedSearches == null) return;

        if (mSearchPanel.isPeeking()) {
            mSearchPanel.expandPanel(StateChangeReason.CLICK);
        }
        if (suggestionIndex < RelatedSearchesControl.INDEX_OF_THE_FIRST_RELATED_SEARCHES) {
            // Click on the default query
            mSearchRequest =
                    new ContextualSearchRequest(
                            mProfile,
                            mResolvedSearchTerm.searchTerm(),
                            mResolvedSearchTerm.alternateTerm(),
                            mResolvedSearchTerm.mid(),
                            /* isLowPriorityEnabled= */ false,
                            mResolvedSearchTerm.searchUrlFull(),
                            mResolvedSearchTerm.searchUrlPreload());
            mSearchPanel.setSearchTerm(mResolvedSearchTerm.searchTerm());
            mIsRelatedSearchesSerp = false;
        } else {
            String searchQuery =
                    mRelatedSearches.getQueries().get(suggestionIndex - defaultSearchAdjustment);
            Uri searchUri =
                    mRelatedSearches.getSearchUri(suggestionIndex - defaultSearchAdjustment);
            if (searchUri != null) {
                mSearchRequest = new ContextualSearchRequest(mProfile, searchUri);
            } else {
                mSearchRequest = new ContextualSearchRequest(mProfile, searchQuery);
            }
            mSearchPanel.setSearchTerm(searchQuery);
            mIsRelatedSearchesSerp = true;
        }

        // TODO(donnd): determine what to show in the Caption, if anything.
        loadSearchUrl();

        // Make sure we show the serp contents
        if (getSearchPanelWebContents() != null) {
            getSearchPanelWebContents().updateWebContentsVisibility(Visibility.VISIBLE);
        }
    }

    @Override
    public void setContextualSearchPromoCardSelection(boolean enabled) {
        ContextualSearchPolicy.setContextualSearchFullyOptedIn(mProfile, enabled);
    }

    @Override
    public void onPromoShown() {
        ContextualSearchPolicy.onPromoShown(mProfile);
    }

    /** @return The {@link SelectionClient} used by Contextual Search. */
    SelectionClient getContextualSearchSelectionClient() {
        return mContextualSearchSelectionClient;
    }

    /**
     * Implements the {@link SelectionClient} interface for Contextual Search.
     * Handles messages from Content about selection changes.  These are the key drivers of
     * Contextual Search logic.
     */
    private class ContextualSearchSelectionClient implements SelectionClient {
        @Override
        public void onSelectionChanged(String selection) {
            if (mSearchPanel != null) {
                mSelectionController.handleSelectionChanged(selection);
                mSearchPanel.updateBrowserControlsState(BrowserControlsState.BOTH, true);
            }
        }

        @Override
        public void onSelectionEvent(
                @SelectionEventType int eventType, float posXPix, float posYPix) {
            mSelectionController.handleSelectionEvent(eventType, posXPix, posYPix);
        }

        @Override
        public void selectAroundCaretAck(@Nullable SelectAroundCaretResult result) {
            if (mSelectAroundCaretCounter > 0) mSelectAroundCaretCounter--;
            if (mSelectAroundCaretCounter > 0
                    || !mInternalStateController.isStillWorkingOn(
                            InternalState.START_SHOWING_TAP_UI)) {
                return;
            }

            // Process normally unless something went wrong with the selection.
            if (result != null) {
                assert mContext != null;
                mContext.onSelectionAdjusted(
                        result.getExtendedStartAdjust(), result.getExtendedEndAdjust());
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
    void onShowUnhandledTapUIIfNeeded(int x, int y) {
        mSelectionController.handleShowUnhandledTapUIIfNeeded(x, y);
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
    public void handleScrollStart() {
        if (isSuppressed()) return;

        hideContextualSearch(StateChangeReason.BASE_PAGE_SCROLL);
    }

    @Override
    public void handleScrollEnd() {
        if (mSelectionController.getSelectionType() == SelectionType.RESOLVING_LONG_PRESS) {
            mSearchPanel.showPanel(StateChangeReason.BASE_PAGE_SCROLL);
        }
    }

    @Override
    public void handleInvalidTap() {
        if (isSuppressed()) return;

        hideContextualSearch(StateChangeReason.BASE_PAGE_TAP);
    }

    @Override
    public void handleSuppressedTap() {
        if (isSuppressed()) return;

        hideContextualSearch(StateChangeReason.TAP_SUPPRESS);
    }

    @Override
    public void handleNonSuppressedTap(long tapTimeNanoseconds) {
        if (isSuppressed()) return;

        finishSuppressionDecision();
    }

    /** Finishes work on the suppression decision if that work is still in progress. */
    private void finishSuppressionDecision() {
        if (mInternalStateController.isStillWorkingOn(InternalState.DECIDING_SUPPRESSION)) {
            mInternalStateController.notifyFinishedWorkOn(InternalState.DECIDING_SUPPRESSION);
        }
    }

    @Override
    public void handleMetricsForWouldSuppressTap(ContextualSearchHeuristics tapHeuristics) {
        if (mSearchPanel != null) {
            mSearchPanel.getPanelMetrics().setResultsSeenExperiments(tapHeuristics);
        }
    }

    @Override
    public void handleValidTap() {
        if (isSuppressed()) return;

        // This will synchronously advance to the next state (and possibly others) before
        // returning.
        mInternalStateController.enter(InternalState.TAP_RECOGNIZED);
    }

    @Override
    public void handleValidResolvingLongpress() {
        if (isSuppressed()) return;

        mInternalStateController.enter(InternalState.RESOLVING_LONG_PRESS_RECOGNIZED);
    }

    /**
     * Notifies this class that the selection has changed. This may be due to the user moving the
     * selection handles after a long-press, or after a Tap gesture has called selectAroundCaret to
     * expand the selection to a whole word or sentence.
     */
    @Override
    public void handleSelection(
            String selection, boolean selectionValid, @SelectionType int type, float x, float y) {
        if (isSuppressed()) return;

        if (!selection.isEmpty()) {
            if (selectionValid && mSearchPanel != null) {
                mSearchPanel.updateBasePageSelectionYPx(y);
                showSelectionAsSearchInBar(selection);

                if (type == SelectionType.LONG_PRESS) {
                    mInternalStateController.enter(InternalState.LONG_PRESS_RECOGNIZED);
                } else if (type == SelectionType.RESOLVING_LONG_PRESS) {
                    mInternalStateController.enter(InternalState.RESOLVING_LONG_PRESS_RECOGNIZED);
                }
            } else {
                hideContextualSearch(StateChangeReason.INVALID_SELECTION);
            }
        }
    }

    @Override
    public void handleSelectionDismissal() {
        if (isSuppressed()) return;

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
        if (isSuppressed()) return;

        if (isSearchPanelShowing()) {
            if (selectionValid) {
                mSearchPanel.setSearchTerm(selection);
                mSearchPanel.hideCaption();
                // If we have a literal search request we should update that too.
                if (mSearchRequest != null) {
                    mSearchRequest =
                            new ContextualSearchRequest(
                                    mProfile, selection, mPolicy.shouldPrefetchSearchResult());
                }
                mIsRelatedSearchesSerp = false;
            } else {
                hideContextualSearch(StateChangeReason.INVALID_SELECTION);
            }
        }
    }

    @Override
    public void handleSelectionCleared() {
        // The selection was just cleared, so we'll want to remove our UX unless it was due to
        // another Tap while the Bar is showing.
        mInternalStateController.enter(InternalState.SELECTION_CLEARED_RECOGNIZED);
    }

    /** Shows the given selection as the Search Term in the Bar. */
    private void showSelectionAsSearchInBar(String selection) {
        if (isSearchPanelShowing()) {
            mSearchPanel.setSearchTerm(selection);
            mIsRelatedSearchesSerp = false;
        }
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

                if (isSearchPanelShowing()) {
                    mSearchPanel.closePanel(reason, false);
                } else {
                    // Clear any tap-based selection, but not longpress based selections.
                    if (mSelectionController.getSelectionType() == SelectionType.TAP) {
                        mSelectionController.clearSelection();
                    }
                }
            }

            @Override
            public void gatherSurroundingText() {
                if (mContext != null) mContext.destroy();
                mContext =
                        new ContextualSearchContext() {
                            @Override
                            void onSelectionChanged() {
                                notifyShowContextualSearch();
                            }
                        };

                boolean isResolvingGesture = mPolicy.isResolvingGesture();
                if (isResolvingGesture && mPolicy.shouldPreviousGestureResolve()) {
                    prepareContextResolveProperties();
                }
                WebContents webContents = getBaseWebContents();
                if (webContents != null) {
                    mInternalStateController.notifyStartingWorkOn(
                            InternalState.GATHERING_SURROUNDINGS);
                    ContextualSearchManagerJni.get()
                            .gatherSurroundingText(
                                    mNativeContextualSearchManagerPtr,
                                    ContextualSearchManager.this,
                                    mContext,
                                    webContents);
                } else {
                    mInternalStateController.reset(StateChangeReason.UNKNOWN);
                }
            }

            /** First step where we're committed to processing the current Tap gesture. */
            @Override
            public void tapGestureCommit() {
                mInternalStateController.notifyStartingWorkOn(InternalState.TAP_GESTURE_COMMIT);
                if (!mPolicy.isTapSupported()
                        || mSelectionController.getSelectionType()
                                == SelectionType.RESOLVING_LONG_PRESS) {
                    hideContextualSearch(StateChangeReason.UNKNOWN);
                    return;
                }
                mInternalStateController.notifyFinishedWorkOn(InternalState.TAP_GESTURE_COMMIT);
            }

            /** Starts the process of deciding if we'll suppress the current Tap gesture or not. */
            @Override
            public void decideSuppression() {
                mInternalStateController.notifyStartingWorkOn(InternalState.DECIDING_SUPPRESSION);
                // TODO(donnd): Move handleShouldSuppressTap out of the Selection Controller.
                mSelectionController.handleShouldSuppressTap();
            }

            /** Starts showing the Tap UI by selecting a word around the current caret. */
            @Override
            public void startShowingTapUi() {
                WebContents baseWebContents = getBaseWebContents();
                if (baseWebContents != null) {
                    mInternalStateController.notifyStartingWorkOn(
                            InternalState.START_SHOWING_TAP_UI);
                    mSelectAroundCaretCounter++;
                    baseWebContents.selectAroundCaret(
                            SelectionGranularity.WORD,
                            /* shouldShowHandle= */ false,
                            /* shouldShowContextMenu= */ false,
                            mContext.getSelectionStartOffset(),
                            mContext.getSelectionEndOffset(),
                            mContext.getSurroundingText().length());
                    // Let the policy know that a valid tap gesture has been received.
                    mPolicy.registerTap();
                } else {
                    mInternalStateController.reset(StateChangeReason.UNKNOWN);
                }
            }

            /**
             * Waits for possible Tap gesture that's near enough to the previous tap to be
             * considered a "re-tap". We've done some work on the previous Tap and we just saw the
             * selection get cleared (probably due to a Tap that may or may not be valid). If it's
             * invalid we'll want to hide the UI. If it's valid we'll want to just update the UI
             * rather than having the Bar hide and re-show.
             */
            @Override
            public void waitForPossibleTapNearPrevious() {
                mInternalStateController.notifyStartingWorkOn(
                        InternalState.WAITING_FOR_POSSIBLE_TAP_NEAR_PREVIOUS);
                new Handler()
                        .postDelayed(
                                new Runnable() {
                                    @Override
                                    public void run() {
                                        // We may have been destroyed.
                                        if (mSearchPanel != null) mSearchPanel.hideCaption();
                                        mInternalStateController.notifyFinishedWorkOn(
                                                InternalState
                                                        .WAITING_FOR_POSSIBLE_TAP_NEAR_PREVIOUS);
                                    }
                                },
                                TAP_NEAR_PREVIOUS_DETECTION_DELAY_MS);
            }

            /**
             * Waits for possible Tap gesture that's on a previously established tap-selection. If
             * the current Tap was on the previous tap-selection then this selection will become a
             * Long-press selection and we'll recognize that gesture and start processing it. If
             * that doesn't happen within our time window (which is the common case) then we'll
             * advance to the next state in normal Tap processing.
             */
            @Override
            public void waitForPossibleTapOnTapSelection() {
                mInternalStateController.notifyStartingWorkOn(
                        InternalState.WAITING_FOR_POSSIBLE_TAP_ON_TAP_SELECTION);
                new Handler()
                        .postDelayed(
                                new Runnable() {
                                    @Override
                                    public void run() {
                                        mInternalStateController.notifyFinishedWorkOn(
                                                InternalState
                                                        .WAITING_FOR_POSSIBLE_TAP_ON_TAP_SELECTION);
                                    }
                                },
                                TAP_ON_TAP_SELECTION_DELAY_MS);
            }

            /** Starts a Resolve request to our server for the best Search Term. */
            @Override
            public void resolveSearchTerm() {
                mInternalStateController.notifyStartingWorkOn(InternalState.RESOLVING);

                String selection = mSelectionController.getSelectedText();
                assert !TextUtils.isEmpty(selection);

                WebContents baseWebContents = getBaseWebContents();
                if (baseWebContents != null && mContext != null && mContext.canResolve()) {
                    issueResolveRequest();
                } else {
                    // Something went wrong and we couldn't resolve.
                    hideContextualSearch(StateChangeReason.UNKNOWN);
                    return;
                }

                // If the we were unable to start the resolve, we've hidden the UI and set the
                // context to null.
                if (mContext == null || mSearchPanel == null) return;

                // Update the UI to show the resolve is in progress.
                mSearchPanel.setContextDetails(
                        selection, mContext.getTextContentFollowingSelection());
            }

            @Override
            public void showContextualSearchResolvingUi() {
                if (mSelectionController.getSelectionType() == SelectionType.UNDETERMINED) {
                    mInternalStateController.reset(StateChangeReason.INVALID_SELECTION);
                } else {
                    mInternalStateController.notifyStartingWorkOn(InternalState.SHOW_RESOLVING_UI);
                    boolean isTap = mSelectionController.getSelectionType() == SelectionType.TAP;
                    showContextualSearch(
                            isTap
                                    ? StateChangeReason.TEXT_SELECT_TAP
                                    : StateChangeReason.TEXT_SELECT_LONG_PRESS);
                    mInternalStateController.notifyFinishedWorkOn(InternalState.SHOW_RESOLVING_UI);
                }
            }

            @Override
            public void showContextualSearchLiteralSearchUi() {
                mInternalStateController.notifyStartingWorkOn(InternalState.SHOWING_LITERAL_SEARCH);
                showContextualSearch(
                        mSelectionController.getSelectionType() == SelectionType.LONG_PRESS
                                ? StateChangeReason.TEXT_SELECT_LONG_PRESS
                                : StateChangeReason.TEXT_SELECT_TAP);
                mInternalStateController.notifyFinishedWorkOn(InternalState.SHOWING_LITERAL_SEARCH);
            }

            @Override
            public void showingTapSearch() {
                mInternalStateController.notifyStartedAndFinished(InternalState.SHOWING_TAP_SEARCH);
            }

            @Override
            public void showingIntelligentLongpress() {
                mInternalStateController.notifyStartedAndFinished(
                        InternalState.SHOWING_RESOLVED_LONG_PRESS_SEARCH);
            }

            @Override
            public void completeSearch() {
                if (mSearchPanel != null) {
                    mSearchPanel.ensureCaption();
                }
            }
        };
    }

    /**
     * @param profile The {@link Profile} associated with this Contextual Search session.
     * @return Whether the Contextual Search feature was disabled by the user explicitly.
     */
    public static boolean isContextualSearchDisabled(Profile profile) {
        return ContextualSearchPolicy.isContextualSearchDisabled(profile);
    }

    // Private helper functions

    /** @return The language of the base page being viewed by the user. */
    private String getBasePageLanguage() {
        return mContext.getDetectedLanguage();
    }

    private int getBasePageHeight() {
        final Tab tab = mTabSupplier.get();
        if (tab == null || tab.getView() == null) return 0;
        return tab.getView().getHeight();
    }

    // ============================================================================================
    // Misc helpers
    // ============================================================================================

    /**
     * Build the searches suggestions for the Bar or Panel.
     * @param defaultSearch The resolved search term..
     * @return A {@code List<String>} of search suggestions in the bar or the Panel, or {@code null}
     *         if the feature for showing chips is not enabled.
     */
    private @Nullable List<String> buildRelatedSearches(String defaultSearch) {
        List<String> queries = mRelatedSearches.getQueries();
        if (queries.size() == 0) {
            return queries;
        }

        List<String> relatedSearches = new ArrayList<String>(queries.size() + 1);
        relatedSearches.add(defaultSearch);
        relatedSearches.addAll(queries);

        return relatedSearches;
    }

    /**
     * Returns whether the View of the Base Page is too small to show our Overlay Panel.
     * @param viewHeightLimitPixels The required height in pixels.
     */
    private boolean isViewTooSmall(int viewHeightLimitPixels) {
        int basePageHeight = getBasePageHeight();
        return basePageHeight > 0 && basePageHeight < viewHeightLimitPixels;
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

    /**
     * @return The {@link ContextualSearchPanel}, for testing purposes only.
     */
    @VisibleForTesting
    ContextualSearchPanel getContextualSearchPanel() {
        return mSearchPanel;
    }

    /**
     * @return the {@link OverlayPanelStateProvider} for observing changes to the Overlay Panel
     *     state.
     */
    public ObservableSupplier<OverlayPanelStateProvider> getOverlayPanelStateProviderSupplier() {
        return mOverlayPanelStateProviderSupplier;
    }

    /**
     * @return The selection controller, for testing purposes.
     */
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
    void setContextualSearchInternalStateController(
            ContextualSearchInternalStateController controller) {
        mInternalStateController = controller;
    }

    @VisibleForTesting
    public boolean isSuppressed() {
        boolean shouldSimplySuppress = mIsBottomSheetVisible || mIsAccessibilityModeEnabled;
        if (shouldSimplySuppress) return true;

        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_SEARCH_SUPPRESS_SHORT_VIEW)) {
            return false;
        }

        int limitViewDp = ContextualSearchFieldTrial.getContextualSearchMinimumBasePageHeightDp();

        int viewHeightLimitPixels =
                limitViewDp == 0
                        ? mActivity
                                .getResources()
                                .getDimensionPixelSize(
                                        R.dimen.contextual_search_minimum_base_page_height)
                        : Math.round(limitViewDp * mDpToPx);

        boolean isViewTooSmall = isViewTooSmall(viewHeightLimitPixels);
        ContextualSearchUma.logViewTooSmall(isViewTooSmall);
        return isViewTooSmall;
    }

    @NativeMethods
    interface Natives {
        long init(ContextualSearchManager caller, @JniType("Profile*") Profile profile);

        void destroy(long nativeContextualSearchManager, ContextualSearchManager caller);

        void startSearchTermResolutionRequest(
                long nativeContextualSearchManager,
                ContextualSearchManager caller,
                ContextualSearchContext contextualSearchContext,
                WebContents baseWebContents);

        void gatherSurroundingText(
                long nativeContextualSearchManager,
                ContextualSearchManager caller,
                ContextualSearchContext contextualSearchContext,
                WebContents baseWebContents);

        void removeLastHistoryEntry(
                long nativeContextualSearchManager,
                ContextualSearchManager caller,
                String historyUrl,
                long urlTimeMs);
    }
}
