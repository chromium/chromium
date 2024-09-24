// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.app.Activity;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.CallbackController;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.TimeUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.jank_tracker.JankScenario;
import org.chromium.base.jank_tracker.JankTracker;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.feed.FeedActionDelegateImpl;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feed.FeedActionDelegate;
import org.chromium.chrome.browser.feed.FeedReliabilityLogger;
import org.chromium.chrome.browser.feed.FeedSurfaceCoordinator;
import org.chromium.chrome.browser.feed.FeedSurfaceDelegate;
import org.chromium.chrome.browser.feed.FeedSurfaceLifecycleManager;
import org.chromium.chrome.browser.feed.FeedSurfaceProvider;
import org.chromium.chrome.browser.feed.FeedSwipeRefreshLayout;
import org.chromium.chrome.browser.feed.NtpFeedSurfaceLifecycleManager;
import org.chromium.chrome.browser.feed.componentinterfaces.SurfaceCoordinator;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.LifecycleObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager;
import org.chromium.chrome.browser.magic_stack.HomeModulesCoordinator;
import org.chromium.chrome.browser.magic_stack.HomeModulesMetricsUtils;
import org.chromium.chrome.browser.magic_stack.ModuleDelegateHost;
import org.chromium.chrome.browser.magic_stack.ModuleRegistry;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.omnibox.OmniboxFocusReason;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_resumption.SearchResumptionModuleCoordinator;
import org.chromium.chrome.browser.search_resumption.SearchResumptionModuleUtils;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.single_tab.SingleTabSwitcherCoordinator;
import org.chromium.chrome.browser.suggestions.SuggestionsMetrics;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegateImpl;
import org.chromium.chrome.browser.suggestions.tile.Tile;
import org.chromium.chrome.browser.suggestions.tile.TileGroup;
import org.chromium.chrome.browser.suggestions.tile.TileGroupDelegateImpl;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.InvalidationAwareThumbnailProvider;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.HomeSurfaceTracker;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupCreationDialogManager;
import org.chromium.chrome.browser.toolbar.top.Toolbar;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.BasicSmoothTransitionDelegate;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

import java.util.List;

/** Provides functionality when the user interacts with the NTP. */
public class NewTabPage
        implements NativePage,
                InvalidationAwareThumbnailProvider,
                TemplateUrlServiceObserver,
                BrowserControlsStateProvider.Observer,
                FeedSurfaceDelegate,
                VoiceRecognitionHandler.Observer,
                ModuleDelegateHost {
    private static final String TAG = "NewTabPage";

    // Key for the scroll position data that may be stored in a navigation entry.
    public static final String CONTEXT_MENU_USER_ACTION_PREFIX = "Suggestions";

    protected final Tab mTab;
    private final Supplier<Tab> mActivityTabProvider;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    private final String mTitle;
    private final JankTracker mJankTracker;
    private final Context mContext;
    private final int mBackgroundColor;
    protected final NewTabPageManagerImpl mNewTabPageManager;
    protected final TileGroup.Delegate mTileGroupDelegate;
    private final boolean mIsTablet;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final ContextMenuManager mContextMenuManager;
    private final ObserverList<MostVisitedTileClickObserver> mMostVisitedTileClickObservers;
    private final BottomSheetController mBottomSheetController;
    private FeedSurfaceProvider mFeedSurfaceProvider;

    private NewTabPageLayout mNewTabPageLayout;
    private TabObserver mTabObserver;
    private LifecycleObserver mLifecycleObserver;
    protected boolean mSearchProviderHasLogo;

    protected OmniboxStub mOmniboxStub;
    private VoiceRecognitionHandler mVoiceRecognitionHandler;

    // The timestamp at which the constructor was called.
    protected final long mConstructedTimeNs;

    // The timestamp at which this NTP was last shown to the user.
    private long mLastShownTimeNs;

    private boolean mIsLoaded;

    // Whether destroy() has been called.
    private boolean mIsDestroyed;

    private final int mToolbarHeight;

    private final Supplier<Toolbar> mToolbarSupplier;
    private final TabModelSelector mTabModelSelector;
    private final TemplateUrlService mTemplateUrlService;
    private final ObservableSupplier<TabContentManager> mTabContentManagerSupplier;
    private final ObservableSupplier<Integer> mTabStripHeightSupplier;

    private SingleTabSwitcherCoordinator mSingleTabSwitcherCoordinator;
    private ViewGroup mSingleTabCardContainer;
    @Nullable private HomeModulesCoordinator mHomeModulesCoordinator;
    @Nullable private ViewGroup mHomeModulesContainer;
    private ObservableSupplierImpl<Tab> mMostRecentTabSupplier = new ObservableSupplierImpl<>();
    @Nullable private Point mContextMenuStartPosition;

    private final Activity mActivity;
    @Nullable private final HomeSurfaceTracker mHomeSurfaceTracker;
    private boolean mSnapshotSingleTabCardChanged;
    private final boolean mIsInNightMode;
    @Nullable private final OneshotSupplier<ModuleRegistry> mModuleRegistrySupplier;

    @Nullable private SearchResumptionModuleCoordinator mSearchResumptionModuleCoordinator;
    private SmoothTransitionDelegate mSmoothTransitionDelegate;

    private CallbackController mCallbackController = new CallbackController();

    @Override
    public void onControlsOffsetChanged(
            int topOffset,
            int topControlsMinHeightOffset,
            int bottomOffset,
            int bottomControlsMinHeightOffset,
            boolean needsAnimate,
            boolean isVisibilityForced) {
        updateMargins();
    }

    @Override
    public void onBottomControlsHeightChanged(
            int bottomControlsHeight, int bottomControlsMinHeight) {
        updateMargins();
    }

    /**
     * Allows clients to listen for updates to the scroll changes of the search box on the
     * NTP.
     */
    public interface OnSearchBoxScrollListener {
        /**
         * Callback to be notified when the scroll position of the search box on the NTP has
         * changed.  A scroll percentage of 0, means the search box has no scroll applied and
         * is in it's natural resting position.  A value of 1 means the search box is scrolled
         * entirely to the top of the screen viewport.
         *
         * @param scrollPercentage The percentage the search box has been scrolled off the page.
         */
        void onNtpScrollChanged(float scrollPercentage);
    }

    /** An observer for most visited tile clicks. */
    public interface MostVisitedTileClickObserver {
        /**
         * Called when a most visited tile is clicked.
         * @param tile The most visited tile that was clicked.
         * @param tab The tab hosting the most visited tile section.
         */
        void onMostVisitedTileClicked(Tile tile, Tab tab);
    }

    protected class NewTabPageManagerImpl extends SuggestionsUiDelegateImpl
            implements NewTabPageManager {
        private final Tracker mTracker;

        public NewTabPageManagerImpl(
                SuggestionsNavigationDelegate navigationDelegate,
                Profile profile,
                NativePageHost nativePageHost,
                SnackbarManager snackbarManager) {
            super(navigationDelegate, profile, nativePageHost, snackbarManager);
            mTracker = TrackerFactory.getTrackerForProfile(profile);
        }

        @Override
        public boolean isLocationBarShownInNtp() {
            if (mIsDestroyed) return false;
            return isInSingleUrlBarMode() && !mNewTabPageLayout.urlFocusAnimationsDisabled();
        }

        @Override
        public boolean isVoiceSearchEnabled() {
            return mVoiceRecognitionHandler != null
                    && mVoiceRecognitionHandler.isVoiceSearchEnabled();
        }

        @Override
        public void focusSearchBox(boolean beginVoiceSearch, String pastedText) {
            if (mIsDestroyed) return;
            FeedReliabilityLogger feedReliabilityLogger =
                    mFeedSurfaceProvider.getReliabilityLogger();
            if (mVoiceRecognitionHandler != null && beginVoiceSearch) {
                if (feedReliabilityLogger != null) {
                    feedReliabilityLogger.onVoiceSearch();
                }
                mVoiceRecognitionHandler.startVoiceRecognition(
                        VoiceRecognitionHandler.VoiceInteractionSource.NTP);
                mTracker.notifyEvent(EventConstants.NTP_VOICE_SEARCH_BUTTON_CLICKED);
            } else if (mOmniboxStub != null) {
                if (feedReliabilityLogger != null) {
                    feedReliabilityLogger.onOmniboxFocused();
                }
                mOmniboxStub.setUrlBarFocus(
                        true,
                        pastedText,
                        pastedText == null
                                ? OmniboxFocusReason.FAKE_BOX_TAP
                                : OmniboxFocusReason.FAKE_BOX_LONG_PRESS);
            }
        }

        @Override
        public boolean isCurrentPage() {
            if (mIsDestroyed) return false;
            if (mOmniboxStub == null) return false;
            return getNewTabPageForCurrentTab() == NewTabPage.this;
        }

        private NewTabPage getNewTabPageForCurrentTab() {
            Tab currentTab = mActivityTabProvider.get();
            NativePage nativePage = currentTab != null ? currentTab.getNativePage() : null;
            return (nativePage instanceof NewTabPage) ? (NewTabPage) nativePage : null;
        }

        @Override
        public void onLoadingComplete() {
            if (mIsDestroyed) return;
            mIsLoaded = true;
            NewTabPageUma.recordNtpImpression(NewTabPageUma.NTP_IMPRESSION_REGULAR);
            // If not visible when loading completes, wait until onShown is received.
            if (!mTab.isHidden()) recordNtpShown();
        }
    }

    /**
     * Extends {@link TileGroupDelegateImpl} to add metrics logging that is specific to {@link
     * NewTabPage}.
     */
    private class NewTabPageTileGroupDelegate extends TileGroupDelegateImpl {
        private NewTabPageTileGroupDelegate(
                Context context,
                Profile profile,
                SuggestionsNavigationDelegate navigationDelegate,
                SnackbarManager snackbarManager) {
            super(context, profile, navigationDelegate, snackbarManager);
        }

        @Override
        public void onLoadingComplete(List<Tile> tiles) {
            if (mIsDestroyed) return;

            super.onLoadingComplete(tiles);
            mNewTabPageLayout.onTilesLoaded();
        }

        @Override
        public void openMostVisitedItem(int windowDisposition, Tile tile) {
            if (mIsDestroyed) return;

            super.openMostVisitedItem(windowDisposition, tile);
            for (MostVisitedTileClickObserver observer : mMostVisitedTileClickObservers) {
                observer.onMostVisitedTileClicked(tile, mTab);
            }
        }
    }

    /**
     * Constructs a NewTabPage.
     *
     * @param activity The activity used for context to create the new tab page's View.
     * @param browserControlsStateProvider {@link BrowserControlsStateProvider} to observe for
     *     offset changes.
     * @param activityTabProvider Provides the current active tab.
     * @param modalDialogManager {@link ModalDialogManager} for the app.
     * @param snackbarManager {@link SnackbarManager} object.
     * @param lifecycleDispatcher Activity lifecycle dispatcher.
     * @param tabModelSelector {@link TabModelSelector} object.
     * @param isTablet {@code true} if running on a Tablet device.
     * @param uma {@link NewTabPageUma} object recording user metrics.
     * @param isInNightMode {@code true} if the night mode setting is on.
     * @param nativePageHost The host that is showing this new tab page.
     * @param tab The {@link Tab} that contains this new tab page.
     * @param url The URL that launched this new tab page.
     * @param bottomSheetController The controller for bottom sheets, used by the feed.
     * @param shareDelegateSupplier Supplies the Delegate used to open SharingHub.
     * @param windowAndroid The containing window of this page.
     * @param jankTracker {@link JankTracker} object to measure jankiness while NTP is visible.
     * @param toolbarSupplier Supplies the {@link Toolbar}.
     * @param homeSurfaceTracker Used to decide whether we are the home surface.
     * @param tabContentManagerSupplier Used to create tab thumbnails.
     * @param tabStripHeightSupplier Supplier for the tab strip height.
     * @param moduleRegistrySupplier Supplier for the {@link ModuleRegistry}.
     * @param edgeToEdgeControllerSupplier Supplier for the {@link EdgeToEdgeController}.
     */
    public NewTabPage(
            Activity activity,
            BrowserControlsStateProvider browserControlsStateProvider,
            Supplier<Tab> activityTabProvider,
            ModalDialogManager modalDialogManager,
            SnackbarManager snackbarManager,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            TabModelSelector tabModelSelector,
            boolean isTablet,
            NewTabPageUma uma,
            boolean isInNightMode,
            NativePageHost nativePageHost,
            Tab tab,
            String url,
            BottomSheetController bottomSheetController,
            Supplier<ShareDelegate> shareDelegateSupplier,
            WindowAndroid windowAndroid,
            JankTracker jankTracker,
            Supplier<Toolbar> toolbarSupplier,
            HomeSurfaceTracker homeSurfaceTracker,
            ObservableSupplier<TabContentManager> tabContentManagerSupplier,
            ObservableSupplier<Integer> tabStripHeightSupplier,
            OneshotSupplier<ModuleRegistry> moduleRegistrySupplier,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier) {
        mConstructedTimeNs = System.nanoTime();
        TraceEvent.begin(TAG);

        mActivity = activity;
        mActivityTabProvider = activityTabProvider;
        mActivityLifecycleDispatcher = lifecycleDispatcher;
        mTab = tab;
        mJankTracker = jankTracker;
        mToolbarSupplier = toolbarSupplier;
        mMostVisitedTileClickObservers = new ObserverList<>();
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mTabModelSelector = tabModelSelector;
        mBottomSheetController = bottomSheetController;
        mHomeSurfaceTracker = homeSurfaceTracker;
        mTabContentManagerSupplier = tabContentManagerSupplier;
        mIsInNightMode = isInNightMode;
        mTabStripHeightSupplier = tabStripHeightSupplier;
        mModuleRegistrySupplier = moduleRegistrySupplier;

        Profile profile = mTab.getProfile();

        var tabGroupCreationDialogManager =
                new TabGroupCreationDialogManager(
                        mActivity, modalDialogManager, /* onTabGroupCreation= */ null);
        SuggestionsNavigationDelegate navigationDelegate =
                new SuggestionsNavigationDelegate(
                        activity,
                        profile,
                        nativePageHost,
                        tabModelSelector,
                        tabGroupCreationDialogManager,
                        mTab);
        mNewTabPageManager =
                new NewTabPageManagerImpl(
                        navigationDelegate, profile, nativePageHost, snackbarManager);
        mTileGroupDelegate =
                new NewTabPageTileGroupDelegate(
                        activity, profile, navigationDelegate, snackbarManager);

        mContext = activity;
        mTitle = activity.getResources().getString(R.string.new_tab_title);

        mBackgroundColor =
                ChromeColors.getSurfaceColor(
                        mContext, R.dimen.home_surface_background_color_elevation);
        mIsTablet = isTablet;
        mTemplateUrlService = TemplateUrlServiceFactory.getForProfile(profile);
        mTemplateUrlService.addObserver(this);

        mTabObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onShown(Tab tab, @TabSelectionType int type) {
                        // Showing the NTP is only meaningful when the page has been loaded already.
                        if (mIsLoaded) recordNtpShown();
                        mNewTabPageLayout.onSwitchToForeground();
                    }

                    @Override
                    public void onHidden(Tab tab, @TabHidingType int type) {
                        if (mIsLoaded) recordNtpHidden();
                        if (mSingleTabSwitcherCoordinator != null
                                && (mHomeSurfaceTracker == null
                                        || !mHomeSurfaceTracker.canShowHomeSurface(mTab))) {
                            mSingleTabSwitcherCoordinator.hide();
                        }
                    }

                    @Override
                    public void onInteractabilityChanged(Tab tab, boolean isInteractable) {
                        // We start/stop tracking based on InteractabilityChanged in addition to
                        // Shown/Hidden because those events don't trigger for switching to tab
                        // switcher, we don't rely solely on this event because it doesn't
                        // trigger when the user navigates to a website.
                        if (isInteractable) {
                            mJankTracker.startTrackingScenario(JankScenario.NEW_TAB_PAGE);
                        } else {
                            mJankTracker.finishTrackingScenario(JankScenario.NEW_TAB_PAGE);
                        }
                    }
                };
        mTab.addObserver(mTabObserver);

        mLifecycleObserver =
                new PauseResumeWithNativeObserver() {
                    @Override
                    public void onResumeWithNative() {}

                    @Override
                    public void onPauseWithNative() {
                        // Only record when this tab is the current tab.
                        if (mActivityTabProvider.get() == mTab) {
                            RecordUserAction.record("MobileNTPPaused");
                        }
                    }
                };
        mActivityLifecycleDispatcher.register(mLifecycleObserver);

        updateSearchProviderHasLogo();
        initializeMainView(
                activity,
                windowAndroid,
                snackbarManager,
                isInNightMode,
                shareDelegateSupplier,
                url,
                edgeToEdgeControllerSupplier);

        // It is possible that the NewTabPage is created when the Tab model hasn't been initialized.
        // For example, the user changes theme when a NTP is showing, which leads to the recreation
        // of the ChromeTabbedActivity and showing the NTP as the last visited Tab.
        TabModelUtils.runOnTabStateInitialized(
                mTabModelSelector,
                mCallbackController.makeCancelable(
                        unusedTabModelSelector -> mayCreateSearchResumptionModule(profile)));

        getView()
                .addOnAttachStateChangeListener(
                        new View.OnAttachStateChangeListener() {

                            @Override
                            public void onViewAttachedToWindow(View view) {
                                updateMargins();
                                getView().removeOnAttachStateChangeListener(this);
                            }

                            @Override
                            public void onViewDetachedFromWindow(View view) {}
                        });
        mBrowserControlsStateProvider.addObserver(this);

        mToolbarHeight =
                activity.getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow);

        uma.recordContentSuggestionsDisplayStatus(profile);

        // TODO(twellington): Move this somewhere it can be shared with NewTabPageView?
        Runnable closeContextMenuCallback = activity::closeContextMenu;
        mContextMenuManager =
                new ContextMenuManager(
                        mNewTabPageManager.getNavigationDelegate(),
                        mFeedSurfaceProvider.getTouchEnabledDelegate(),
                        closeContextMenuCallback,
                        NewTabPage.CONTEXT_MENU_USER_ACTION_PREFIX);
        windowAndroid.addContextMenuCloseListener(mContextMenuManager);

        mNewTabPageLayout.initialize(
                mNewTabPageManager,
                activity,
                mTileGroupDelegate,
                mSearchProviderHasLogo,
                mTemplateUrlService.isDefaultSearchEngineGoogle(),
                mFeedSurfaceProvider.getScrollDelegate(),
                mFeedSurfaceProvider.getTouchEnabledDelegate(),
                mFeedSurfaceProvider.getUiConfig(),
                lifecycleDispatcher,
                uma,
                mTab.getProfile(),
                windowAndroid,
                mIsTablet,
                mTabStripHeightSupplier);

        initializeHomeModules();

        TraceEvent.end(TAG);
    }

    /**
     * Create and initialize the main view contained in this NewTabPage.
     *
     * @param activity The activity used to initialize the view.
     * @param windowAndroid Provides the current active tab.
     * @param snackbarManager {@link SnackbarManager} object.
     * @param isInNightMode {@code true} if the night mode setting is on.
     * @param shareDelegateSupplier Supplies a delegate used to open SharingHub.
     * @param url The URL used to identify NTP's launch origin
     * @param edgeToEdgeControllerSupplier The supplier to {@link EdgeToEdgeController}.
     */
    protected void initializeMainView(
            Activity activity,
            WindowAndroid windowAndroid,
            SnackbarManager snackbarManager,
            boolean isInNightMode,
            Supplier<ShareDelegate> shareDelegateSupplier,
            String url,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier) {
        Profile profile = mTab.getProfile();

        LayoutInflater inflater = LayoutInflater.from(activity);
        // TODO(crbug.com/347509698): Remove the log statements after fixing the bug.
        Log.i(TAG, "NewTabPageLayout inflate");
        mNewTabPageLayout = (NewTabPageLayout) inflater.inflate(R.layout.new_tab_page_layout, null);

        FeedActionDelegate actionDelegate =
                new FeedActionDelegateImpl(
                        activity,
                        snackbarManager,
                        mNewTabPageManager.getNavigationDelegate(),
                        BookmarkModel.getForProfile(profile),
                        mTabModelSelector,
                        profile,
                        mBottomSheetController) {
                    @Override
                    public void openHelpPage() {
                        NewTabPageUma.recordAction(NewTabPageUma.ACTION_CLICKED_LEARN_MORE);
                        super.openHelpPage();
                    }
                };

        mFeedSurfaceProvider =
                new FeedSurfaceCoordinator(
                        activity,
                        snackbarManager,
                        windowAndroid,
                        mJankTracker,
                        new SnapScrollHelperImpl(mNewTabPageManager, mNewTabPageLayout),
                        mNewTabPageLayout,
                        mBrowserControlsStateProvider.getTopControlsHeight(),
                        isInNightMode,
                        this,
                        profile,
                        mBottomSheetController,
                        shareDelegateSupplier,
                        /* externalScrollableContainerDelegate= */ null,
                        NewTabPageUtils.decodeOriginFromNtpUrl(url),
                        PrivacyPreferencesManagerImpl.getInstance(),
                        mToolbarSupplier,
                        mConstructedTimeNs,
                        FeedSwipeRefreshLayout.create(activity, R.id.toolbar_container),
                        /* overScrollDisabled= */ false,
                        /* viewportView= */ null,
                        actionDelegate,
                        mTabStripHeightSupplier,
                        edgeToEdgeControllerSupplier);
    }

    /** Initialize the single tab card on home surface NTP or magic stack. */
    private void initializeHomeModules() {
        boolean isTrackingTabReady =
                mHomeSurfaceTracker != null && mHomeSurfaceTracker.isHomeSurfaceTab(mTab);
        // The magic stack is shown on every NTP. There are three cases:
        // 1) on any normal NewTabPage. Initialize the magic stack here.
        // 2) The home surface NewTabPage which is created via back operations. Initialize the
        // magic stack here, and re-show the single Tab card with the previously tracked Tab.
        // 3) The home surface NewTabPage which is created at startup. The magic stack will be
        // initialized later since its tracking Tab hasn't been available yet.
        // The launch type of a home surface NTP is TabLaunchType.FROM_STARTUP.
        if (HomeModulesMetricsUtils.useMagicStack()) {
            mContextMenuStartPosition =
                    ReturnToChromeUtil.calculateContextMenuStartPosition(mActivity.getResources());
            if (isTrackingTabReady) {
                // Case 2) on home surface NTP via back operations.
                showMagicStack(mHomeSurfaceTracker.getLastActiveTabToTrack());
            } else if (mTab.getLaunchType() != TabLaunchType.FROM_STARTUP) {
                // Case 1) on normal NTP.
                showMagicStack(null);
            }
        } else if (isTrackingTabReady) { // On NTP home surface with magic stack disabled.
            showHomeSurfaceUi(mHomeSurfaceTracker.getLastActiveTabToTrack());
        }

        if (isTrackingTabReady) {
            ReturnToChromeUtil.recordHomeSurfaceShown();
        }
    }

    /**
     * @param isTablet Whether the activity is running in tablet mode.
     * @param searchProviderHasLogo Whether the default search engine has logo.
     * @return Whether the NTP is in single url bar mode, i.e. the url bar is shown in-line on the
     *         NTP.
     */
    public static boolean isInSingleUrlBarMode(boolean isTablet, boolean searchProviderHasLogo) {
        return !isTablet && searchProviderHasLogo;
    }

    /**
     * Update the margins for the content when browser controls constraints or bottom control
     *  height are changed.
     */
    private void updateMargins() {
        // TODO(mdjones): can this be merged with BasicNativePage's updateMargins?

        View view = getView();
        ViewGroup.MarginLayoutParams layoutParams =
                ((ViewGroup.MarginLayoutParams) view.getLayoutParams());
        if (layoutParams == null) return;

        // Negative |topControlsDistanceToRest| means the controls Y position is above the rest
        // position and the controls height is increasing with animation, while positive
        // |topControlsDistanceToRest| means the controls Y position is below the rest position and
        // the controls height is decreasing with animation. |getToolbarExtraYOffset()| returns
        // the margin when the controls are at rest, so |getToolbarExtraYOffset()
        // + topControlsDistanceToRest| will give the margin for the current animation frame.
        final int topControlsDistanceToRest =
                mBrowserControlsStateProvider.getContentOffset()
                        - mBrowserControlsStateProvider.getTopControlsHeight();
        final int topMargin = getToolbarExtraYOffset() + topControlsDistanceToRest;

        final int bottomMargin =
                mBrowserControlsStateProvider.getBottomControlsHeight()
                        - mBrowserControlsStateProvider.getBottomControlOffset();

        if (topMargin != layoutParams.topMargin || bottomMargin != layoutParams.bottomMargin) {
            layoutParams.topMargin = topMargin;
            layoutParams.bottomMargin = bottomMargin;
            view.setLayoutParams(layoutParams);
        }
    }

    // TODO(sinansahin): This is the same as {@link ToolbarManager#getToolbarExtraYOffset}. So, we
    // should look into sharing the logic.
    /**
     * @return The height that is included in the top controls but not in the toolbar or the tab
     *     strip.
     */
    private int getToolbarExtraYOffset() {
        return mBrowserControlsStateProvider.getTopControlsHeight()
                - mToolbarHeight
                - mTabStripHeightSupplier.get();
    }

    /**
     * @return The view container for the new tab layout.
     */
    @VisibleForTesting
    public NewTabPageLayout getNewTabPageLayout() {
        return mNewTabPageLayout;
    }

    /**
     * Updates whether the NewTabPage should animate on URL focus changes.
     * @param disable Whether to disable the animations.
     */
    public void setUrlFocusAnimationsDisabled(boolean disable) {
        mNewTabPageLayout.setUrlFocusAnimationsDisabled(disable);
    }

    private boolean isInSingleUrlBarMode() {
        return isInSingleUrlBarMode(mIsTablet, mSearchProviderHasLogo);
    }

    private void updateSearchProviderHasLogo() {
        mSearchProviderHasLogo = mTemplateUrlService.doesDefaultSearchEngineHaveLogo();
    }

    private void onSearchEngineUpdated() {
        updateSearchProviderHasLogo();
        setSearchProviderInfoOnView(
                mSearchProviderHasLogo, mTemplateUrlService.isDefaultSearchEngineGoogle());
        // TODO(crbug.com/40226731): Remove this call when the Feed position experiment is
        // cleaned up.
        updateMargins();
    }

    /**
     * Set the search provider info on the main child view, so that it can change layouts if
     * needed.
     * @param hasLogo Whether the search provider has a logo.
     * @param isGoogle Whether the search provider is Google.
     */
    private void setSearchProviderInfoOnView(boolean hasLogo, boolean isGoogle) {
        mNewTabPageLayout.setSearchProviderInfo(hasLogo, isGoogle);
    }

    /**
     * Specifies the percentage the URL is focused during an animation.  1.0 specifies that the URL
     * bar has focus and has completed the focus animation.  0 is when the URL bar is does not have
     * any focus.
     *
     * @param percent The percentage of the URL bar focus animation.
     */
    public void setUrlFocusChangeAnimationPercent(float percent) {
        mNewTabPageLayout.setUrlFocusChangeAnimationPercent(percent);
    }

    /**
     * Get the bounds of the search box in relation to the top level NewTabPage view.
     *
     * @param bounds The current drawing location of the search box.
     * @param translation The translation applied to the search box by the parent view hierarchy up
     *                    to the NewTabPage view.
     */
    public void getSearchBoxBounds(Rect bounds, Point translation) {
        mNewTabPageLayout.getSearchBoxBounds(bounds, translation, getView());
    }

    /**
     * Updates the opacity of the search box when scrolling.
     *
     * @param alpha opacity (alpha) value to use.
     */
    public void setSearchBoxAlpha(float alpha) {
        mNewTabPageLayout.setSearchBoxAlpha(alpha);
    }

    /**
     * Updates the opacity of the search provider logo when scrolling.
     *
     * @param alpha opacity (alpha) value to use.
     */
    public void setSearchProviderLogoAlpha(float alpha) {
        mNewTabPageLayout.setSearchProviderLogoAlpha(alpha);
    }

    /**
     * Set the search box background drawable.
     *
     * @param drawable The search box background.
     */
    public void setSearchBoxBackground(Drawable drawable) {
        mNewTabPageLayout.setSearchBoxBackground(drawable);
    }

    /**
     * @return Whether the location bar is shown in the NTP.
     */
    public boolean isLocationBarShownInNtp() {
        return mNewTabPageManager.isLocationBarShownInNtp();
    }

    /** @see org.chromium.chrome.browser.omnibox.NewTabPageDelegate#hasCompletedFirstLayout(). */
    public boolean hasCompletedFirstLayout() {
        return mNewTabPageLayout.getHeight() > 0;
    }

    /**
     * @return Whether the location bar has been scrolled to top in the NTP.
     */
    public boolean isLocationBarScrolledToTopInNtp() {
        return mNewTabPageLayout.getToolbarTransitionPercentage() == 1;
    }

    /**
     * Sets the listener for search box scroll changes.
     * @param listener The listener to be notified on changes.
     */
    public void setSearchBoxScrollListener(OnSearchBoxScrollListener listener) {
        mNewTabPageLayout.setSearchBoxScrollListener(listener);
    }

    /** Sets the OmniboxStub that this page interacts with. */
    public void setOmniboxStub(OmniboxStub omniboxStub) {
        mOmniboxStub = omniboxStub;
        if (mOmniboxStub != null) {
            // The toolbar can't get the reference to the native page until its initialization is
            // finished, so we can't cache it here and transfer it to the view later. We pull that
            // state from the location bar when we get a reference to it as a workaround.
            mNewTabPageLayout.setUrlFocusChangeAnimationPercent(
                    omniboxStub.isUrlBarFocused() ? 1f : 0f);

            FeedReliabilityLogger feedReliabilityLogger =
                    mFeedSurfaceProvider.getReliabilityLogger();
            if (feedReliabilityLogger != null) {
                mOmniboxStub.addUrlFocusChangeListener(feedReliabilityLogger);
            }
        }

        mVoiceRecognitionHandler = mOmniboxStub.getVoiceRecognitionHandler();
        if (mVoiceRecognitionHandler != null) {
            mVoiceRecognitionHandler.addObserver(this);
            mNewTabPageLayout.updateActionButtonVisibility();
        }
    }

    @Override
    public void notifyHidingWithBack() {
        FeedReliabilityLogger feedReliabilityLogger = mFeedSurfaceProvider.getReliabilityLogger();
        if (feedReliabilityLogger != null) {
            feedReliabilityLogger.onNavigateBack();
        }
    }

    @Override
    public void onVoiceAvailabilityImpacted() {
        mNewTabPageLayout.updateActionButtonVisibility();
    }

    /** Adds an observer to be notified on most visited tile clicks. */
    public void addMostVisitedTileClickObserver(MostVisitedTileClickObserver observer) {
        mMostVisitedTileClickObservers.addObserver(observer);
    }

    /** Removes the observer. */
    public void removeMostVisitedTileClickObserver(MostVisitedTileClickObserver observer) {
        mMostVisitedTileClickObservers.removeObserver(observer);
    }

    /**
     * Records UMA for the NTP being shown. This includes a fresh page load or being brought to the
     * foreground.
     */
    private void recordNtpShown() {
        mLastShownTimeNs = System.nanoTime();
        RecordUserAction.record("MobileNTPShown");
        mJankTracker.startTrackingScenario(JankScenario.NEW_TAB_PAGE);
        SuggestionsMetrics.recordSurfaceVisible();
    }

    /** Records UMA for the NTP being hidden and the time spent on it. */
    private void recordNtpHidden() {
        mJankTracker.finishTrackingScenario(JankScenario.NEW_TAB_PAGE);
        RecordHistogram.recordMediumTimesHistogram(
                "NewTabPage.TimeSpent",
                (System.nanoTime() - mLastShownTimeNs) / TimeUtils.NANOSECONDS_PER_MILLISECOND);
        SuggestionsMetrics.recordSurfaceHidden();
    }

    /**
     * Returns an arbitrary int value stored in the last committed navigation entry. If some step
     * fails then the default is returned instead.
     *
     * @param key The string previously used to tag this piece of data.
     * @param tab A tab that is used to access the NavigationController and the NavigationEntry
     *     extras.
     * @param defaultValue The value to return if lookup or parsing is unsuccessful.
     * @return The value for the given key.
     *     <p>TODO(crbug.com/40618119): Refactor this to be reusable across NativePage components.
     */
    private static int getIntFromNavigationEntry(String key, Tab tab, int defaultValue) {
        if (tab.getWebContents() == null) return defaultValue;

        String stringValue = getStringFromNavigationEntry(tab, key);
        if (stringValue == null || stringValue.isEmpty()) {
            return RecyclerView.NO_POSITION;
        }

        try {
            return Integer.parseInt(stringValue);
        } catch (NumberFormatException e) {
            Log.w(TAG, "Bad data found for %s : %s", key, stringValue, e);
            return RecyclerView.NO_POSITION;
        }
    }

    /**
     * Returns an arbitrary string value stored in the last committed navigation entry. If the look
     * up fails, an empty string is returned.
     *
     * @param tab A tab that is used to access the NavigationController and the NavigationEntry
     *     extras.
     * @param key The string previously used to tag this piece of data.
     * @return The value previously stored with the given key.
     *     <p>TODO(crbug.com/40618119): Refactor this to be reusable across NativePage components.
     */
    public static String getStringFromNavigationEntry(Tab tab, String key) {
        if (tab.getWebContents() == null) return "";
        NavigationController controller = tab.getWebContents().getNavigationController();
        int index = controller.getLastCommittedEntryIndex();
        return controller.getEntryExtraData(index, key);
    }

    /**
     * @return Whether the NTP has finished loaded.
     */
    public boolean isLoadedForTests() {
        return mIsLoaded;
    }

    // TemplateUrlServiceObserver overrides

    @Override
    public void onTemplateURLServiceChanged() {
        onSearchEngineUpdated();
    }

    // NativePage overrides

    @Override
    public void destroy() {
        assert !mIsDestroyed;
        assert !ViewCompat.isAttachedToWindow(getView())
                : "Destroy called before removed from window";
        if (mIsLoaded && !mTab.isHidden()) recordNtpHidden();

        mCallbackController.destroy();

        mNewTabPageManager.onDestroy();
        mTileGroupDelegate.destroy();
        mTemplateUrlService.removeObserver(this);
        mTab.removeObserver(mTabObserver);
        mTabObserver = null;
        mActivityLifecycleDispatcher.unregister(mLifecycleObserver);
        mLifecycleObserver = null;
        mBrowserControlsStateProvider.removeObserver(this);
        FeedReliabilityLogger feedReliabilityLogger = mFeedSurfaceProvider.getReliabilityLogger();
        if (mOmniboxStub != null && feedReliabilityLogger != null) {
            mOmniboxStub.removeUrlFocusChangeListener(feedReliabilityLogger);
        }
        mFeedSurfaceProvider.destroy();
        mTab.getWindowAndroid().removeContextMenuCloseListener(mContextMenuManager);
        if (mVoiceRecognitionHandler != null) {
            mVoiceRecognitionHandler.removeObserver(this);
        }
        if (mSearchResumptionModuleCoordinator != null) {
            mSearchResumptionModuleCoordinator.destroy();
        }
        if (mSingleTabSwitcherCoordinator != null) {
            destroySingleTabCard();
        }
        if (mHomeModulesCoordinator != null) {
            mHomeModulesCoordinator.destroy();
        }
        mIsDestroyed = true;
    }

    @Override
    public String getUrl() {
        return UrlConstants.NTP_URL;
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public int getBackgroundColor() {
        return mBackgroundColor;
    }

    @Override
    public boolean supportsEdgeToEdge() {
        return !EdgeToEdgeUtils.DISABLE_NTP_E2E.getValue();
    }

    @Override
    public @ColorInt int getToolbarTextBoxBackgroundColor(@ColorInt int defaultColor) {
        if (isLocationBarShownInNtp()) {
            if (!isLocationBarScrolledToTopInNtp()) {
                return ChromeColors.getSurfaceColor(
                        mContext, R.dimen.home_surface_background_color_elevation);
            }

            if (mIsInNightMode) {
                return mContext.getColor(R.color.color_primary_with_alpha_20);
            } else {
                return SemanticColorUtils.getColorPrimaryContainer(mContext);
            }
        }
        return defaultColor;
    }

    @Override
    public @ColorInt int getToolbarSceneLayerBackground(@ColorInt int defaultColor) {
        return isLocationBarShownInNtp() ? getBackgroundColor() : defaultColor;
    }

    @Override
    public boolean needsToolbarShadow() {
        return !mSearchProviderHasLogo;
    }

    @Override
    public View getView() {
        return mFeedSurfaceProvider.getView();
    }

    @Override
    public String getHost() {
        return UrlConstants.NTP_HOST;
    }

    @Override
    public void updateForUrl(String url) {}

    @Override
    public int getHeightOverlappedWithTopControls() {
        return mBrowserControlsStateProvider.getTopControlsHeight();
    }

    @Override
    public void reload() {
        mFeedSurfaceProvider.reload();
        mNewTabPageLayout.reload();
    }

    // InvalidationAwareThumbnailProvider

    @Override
    public boolean shouldCaptureThumbnail() {
        return mNewTabPageLayout.shouldCaptureThumbnail()
                || mFeedSurfaceProvider.shouldCaptureThumbnail()
                || mSnapshotSingleTabCardChanged;
    }

    @Override
    public void captureThumbnail(Canvas canvas) {
        mNewTabPageLayout.onPreCaptureThumbnail();
        mFeedSurfaceProvider.captureThumbnail(canvas);
        mSnapshotSingleTabCardChanged = false;
    }

    // Implements FeedSurfaceDelegate
    @Override
    public FeedSurfaceLifecycleManager createStreamLifecycleManager(
            Activity activity, SurfaceCoordinator coordinator, Profile profile) {
        return new NtpFeedSurfaceLifecycleManager(
                activity, mTab, (FeedSurfaceCoordinator) coordinator);
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        return !(mTab != null && DeviceFormFactor.isWindowOnTablet(mTab.getWindowAndroid()))
                && (mOmniboxStub != null && mOmniboxStub.isUrlBarFocused());
    }

    public FeedSurfaceCoordinator getCoordinatorForTesting() {
        return (FeedSurfaceCoordinator) mFeedSurfaceProvider;
    }

    public NewTabPageManager getNewTabPageManagerForTesting() {
        return mNewTabPageManager;
    }

    public TileGroup.Delegate getTileGroupDelegateForTesting() {
        return mTileGroupDelegate;
    }

    public FeedActionDelegate getFeedActionDelegateForTesting() {
        return ((FeedSurfaceCoordinator) mFeedSurfaceProvider)
                .getActionDelegateForTesting(); // IN-TEST
    }

    TabObserver getTabObserverForTesting() {
        return mTabObserver;
    }

    private void mayCreateSearchResumptionModule(Profile profile) {
        // The module is disabled on tablets.
        if (mIsTablet) return;

        mSearchResumptionModuleCoordinator =
                SearchResumptionModuleUtils.mayCreateSearchResumptionModule(
                        mNewTabPageLayout,
                        mTabModelSelector.getCurrentModel(),
                        mTab,
                        profile,
                        R.id.search_resumption_module_container_stub);
    }

    /**
     * Shows the home surface UI on this NTP.
     * TODO(crbug.com/40263286): Investigate better solution to show Home surface UI on NTP upon
     * creation.
     * to show Home surface UI on NTP upon creation.
     */
    public void showHomeSurfaceUi(Tab mostRecentTab) {
        if (mSingleTabSwitcherCoordinator == null) {
            initializeSingleTabCard(mostRecentTab);
        } else {
            mSingleTabSwitcherCoordinator.show(mostRecentTab);
        }
    }

    /**
     * Shows the magic stack on the home surface NTP.
     *
     * @param mostRecentTab The last shown Tab if exists. It is non null for NTP home surface only.
     */
    public void showMagicStack(Tab mostRecentTab) {
        if (mModuleRegistrySupplier.get() == null) {
            return;
        }

        if (mostRecentTab != null && !UrlUtilities.isNtpUrl(mostRecentTab.getUrl())) {
            mMostRecentTabSupplier.set(mostRecentTab);
        }

        if (mHomeModulesCoordinator == null) {
            initializeMagicStack(mostRecentTab);
        }
        mHomeModulesCoordinator.show(this::onMagicStackShown);
    }

    /** Show the module when the current new tab page is been used as the home surface. */
    private void initializeSingleTabCard(Tab mostRecentTab) {
        if (mostRecentTab == null || UrlUtilities.isNtpUrl(mostRecentTab.getUrl())) {
            return;
        }

        mSingleTabCardContainer =
                (FrameLayout)
                        ((ViewStub)
                                        mNewTabPageLayout.findViewById(
                                                R.id.tab_switcher_module_container_stub))
                                .inflate();
        mSingleTabSwitcherCoordinator =
                new SingleTabSwitcherCoordinator(
                        mActivity,
                        mSingleTabCardContainer,
                        mTabModelSelector,
                        mIsTablet,
                        mostRecentTab,
                        this::onSingleTabCardClicked,
                        /* seeMoreLinkClickedCallback= */ null,
                        () -> mSnapshotSingleTabCardChanged = true,
                        mTabContentManagerSupplier.get()
                        /* tabContentManager= */ ,
                        mIsTablet ? mFeedSurfaceProvider.getUiConfig() : null,
                        /* moduleDelegate= */ null);
        mSingleTabSwitcherCoordinator.showModule();
    }

    /**
     * Initializes the magic stack to show home modules on the current new tab page which is used as
     * the home surface.
     */
    private void initializeMagicStack(Tab mostRecentTab) {
        mHomeModulesContainer =
                (ViewGroup)
                        ((ViewStub)
                                        mNewTabPageLayout.findViewById(
                                                R.id.home_modules_recycler_view_stub))
                                .inflate();
        ObservableSupplier<Profile> profileSupplier =
                new ObservableSupplierImpl<>(mTab.getProfile());
        mHomeModulesCoordinator =
                new HomeModulesCoordinator(
                        mActivity,
                        this,
                        mNewTabPageLayout,
                        HomeModulesConfigManager.getInstance(),
                        profileSupplier,
                        mModuleRegistrySupplier.get());
    }

    private void onMagicStackShown(boolean isVisible) {
        mHomeModulesContainer.setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    private void onSingleTabCardClicked(int tabId) {
        onTabClicked(tabId);
    }

    /**
     * Opens the selected Tab and closes the current NTP. If the single Tab card which tracks the
     * last active Tab is selected, updates the mHomeSurfaceTracker too.
     */
    private void onTabClicked(int tabId) {
        TabModelUtils.selectTabById(mTabModelSelector, tabId, TabSelectionType.FROM_USER);

        mTabModelSelector
                .getModel(false)
                .closeTabs(TabClosureParams.closeTab(mTab).allowUndo(false).build());
        if (mHomeSurfaceTracker != null) {
            // Updates the mHomeSurfaceTracker since the Tab of the NTP is closed.
            mHomeSurfaceTracker.updateHomeSurfaceAndTrackingTabs(null, null);
        }
    }

    public boolean isSingleTabCardVisibleForTesting() {
        if (mSingleTabSwitcherCoordinator == null) return false;

        return mSingleTabSwitcherCoordinator.isVisible();
    }

    public boolean isMagicStackVisibleForTesting() {
        if (mHomeModulesContainer == null) return false;

        return mHomeModulesContainer.getVisibility() == View.VISIBLE;
    }

    /* Destroy the single tab card on the {@link NewTabPageLayout}. */
    @VisibleForTesting
    void destroySingleTabCard() {
        mSingleTabCardContainer.removeAllViews();
        mSingleTabSwitcherCoordinator.hide();
        mSingleTabSwitcherCoordinator.destroy();
        mSingleTabSwitcherCoordinator = null;
    }

    public boolean getSnapshotSingleTabCardChangedForTesting() {
        return mSnapshotSingleTabCardChanged;
    }

    @Override
    public Point getContextMenuStartPoint() {
        return mContextMenuStartPosition;
    }

    @Override
    public UiConfig getUiConfig() {
        return mIsTablet ? mFeedSurfaceProvider.getUiConfig() : null;
    }

    @Override
    public void onUrlClicked(GURL gurl) {
        mTab.loadUrl(new LoadUrlParams(gurl));
    }

    @Override
    public void onTabSelected(int tabId) {
        onTabClicked(tabId);
    }

    @Override
    public void onCaptureThumbnailStatusChanged() {
        mSnapshotSingleTabCardChanged = true;
    }

    @Override
    public void customizeSettings() {
        HomeModulesConfigManager.getInstance().onMenuClick(mContext);
    }

    @Override
    public int getStartMargin() {
        boolean isInNarrowWindowOnTablet =
                mIsTablet
                        && NewTabPageLayout.isInNarrowWindowOnTablet(
                                mIsTablet, mFeedSurfaceProvider.getUiConfig());
        int marginResourceId =
                isInNarrowWindowOnTablet
                        ? R.dimen.ntp_search_box_lateral_margin_narrow_window_tablet
                        : R.dimen.mvt_container_lateral_margin;
        return mContext.getResources().getDimensionPixelSize(marginResourceId);
    }

    @Nullable
    @Override
    public Tab getTrackingTab() {
        if (!mMostRecentTabSupplier.hasValue()) {
            return null;
        }

        return mMostRecentTabSupplier.get();
    }

    @Override
    public boolean isHomeSurface() {
        // Can only show a local tab to resume if we we have a tracked tab. The presence of the
        // local tab to resume module is effectively what being a home surface is.
        return mMostRecentTabSupplier.hasValue();
    }

    @Override
    public SmoothTransitionDelegate enableSmoothTransition() {
        if (mSmoothTransitionDelegate == null) {
            mSmoothTransitionDelegate = new BasicSmoothTransitionDelegate(getView());
        }
        return mSmoothTransitionDelegate;
    }

    public SmoothTransitionDelegate getSmoothTransitionDelegateForTesting() {
        return mSmoothTransitionDelegate;
    }
}
