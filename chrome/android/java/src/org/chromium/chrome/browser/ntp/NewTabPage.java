// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Point;
import android.graphics.Rect;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.CallbackController;
import org.chromium.base.CallbackUtils;
import org.chromium.base.Log;
import org.chromium.base.TimeUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.feed.FeedActionDelegateImpl;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.device_lock.DeviceLockActivityLauncherImpl;
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
import org.chromium.chrome.browser.glic.GlicHelper;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.LifecycleObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.magic_stack.ModuleRegistry;
import org.chromium.chrome.browser.metrics.StartupMetricsTracker;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager.HomepageStateListener;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionIntentHandler;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.readaloud.ReadAloudController.Entrypoint;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.suggestions.SuggestionsMetrics;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegateImpl;
import org.chromium.chrome.browser.suggestions.tile.Tile;
import org.chromium.chrome.browser.suggestions.tile.TileGroup;
import org.chromium.chrome.browser.suggestions.tile.TileGroupDelegateImpl;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.InvalidationAwareThumbnailProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.HomeSurfaceTracker;
import org.chromium.chrome.browser.toolbar.top.Toolbar;
import org.chromium.chrome.browser.ui.bottombar.BottomBarConfigUtils;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.TopInsetProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.chrome.browser.ui.theme.ChromeSemanticColorUtils;
import org.chromium.chrome.browser.url_constants.UrlConstantResolver;
import org.chromium.chrome.browser.url_constants.UrlConstantResolverFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteInput.AutocompleteState;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFocusReason;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.ref.WeakReference;
import java.util.List;
import java.util.function.Supplier;

/** Provides functionality when the user interacts with the NTP. */
@NullMarked
public class NewTabPage
        implements NativePage,
                InvalidationAwareThumbnailProvider,
                TemplateUrlServiceObserver,
                BrowserControlsStateProvider.Observer,
                FeedSurfaceDelegate,
                VoiceRecognitionHandler.Observer,
                SnackbarManageable {
    private static final String TAG = "NewTabPage";

    // Key for the scroll position data that may be stored in a navigation entry.
    public static final String CONTEXT_MENU_USER_ACTION_PREFIX = "Suggestions";

    // This is to count simultaneous NTP for the "NewTabPage.Count" UMA metric. This is
    // incremented/decremented on the UI thread.
    private static int sTotalCount;

    protected final Tab mTab;
    private final Supplier<@Nullable Tab> mActivityTabProvider;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    private final String mTitle;
    private final Point mLastTouchPosition = new Point(-1, -1);
    private final Context mContext;
    private final int mBackgroundColor;
    protected final NewTabPageManagerImpl mNewTabPageManager;
    protected final TileGroup.Delegate mTileGroupDelegate;
    private final boolean mIsTablet;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final BottomSheetController mBottomSheetController;
    private final NewTabPageLayout mNewTabPageLayout;
    private final NewTabPageCoordinator mNewTabPageCoordinator;

    private FeedSurfaceProvider mFeedSurfaceProvider;
    private @Nullable TabObserver mTabObserver;
    private @Nullable LifecycleObserver mLifecycleObserver;
    protected boolean mSearchProviderHasLogo;
    protected boolean mIsDefaultSearchEngineGoogle;

    protected @Nullable OmniboxStub mOmniboxStub;
    private @Nullable VoiceRecognitionHandler mVoiceRecognitionHandler;

    // The timestamp at which the constructor was called.
    protected final long mConstructedTimeNs;

    // The timestamp at which this NTP was last shown to the user.
    private long mLastShownTimeNs;

    private boolean mIsLoaded;

    // Whether destroy() has been called.
    private boolean mIsDestroyed;

    private final int mToolbarHeight;

    private final Supplier<Toolbar> mToolbarSupplier;
    private final TemplateUrlService mTemplateUrlService;
    private final NonNullObservableSupplier<Integer> mTabStripHeightSupplier;

    private final Activity mActivity;
    private boolean mSnapshotSingleTabCardChanged;
    private final boolean mIsInNightMode;
    private final boolean mSupportsEnableEdgeToEdgeOnTop;
    private final TopInsetProvider mTopInsetProvider;
    private TopInsetProvider.@Nullable Observer mTopInsetChangeObserver;
    private boolean mIsUseEdgeToEdgeForCustomizedTheme;

    private @Nullable HomepageStateListener mHomepageStateListener;

    // A flag to use light tint on toolbar and status bar icons. The light tint isn't applied on
    // tablet mode.
    private boolean mUseLightIconTint;

    private @Nullable NtpSmoothTransitionDelegate mSmoothTransitionDelegate;

    private RecyclerView.@Nullable OnScrollListener mNtpScrollListener;

    private final CallbackController mCallbackController = new CallbackController();

    @Override
    public void onControlsOffsetChanged(
            int topOffset,
            int topControlsMinHeightOffset,
            boolean topControlsMinHeightChanged,
            int bottomOffset,
            int bottomControlsMinHeightOffset,
            boolean bottomControlsMinHeightChanged,
            boolean requestNewFrame,
            boolean isVisibilityForced) {
        updateMargins();
    }

    @Override
    public void onBottomControlsHeightChanged(
            int bottomControlsHeight, int bottomControlsMinHeight) {
        updateMargins();
    }

    /** Allows clients to listen for updates to the scroll changes of the search box on the NTP. */
    public interface OnSearchBoxScrollListener {
        /**
         * Callback to be notified when the scroll position of the search box on the NTP has
         * changed. A scroll percentage of 0, means the search box has no scroll applied and is in
         * it's natural resting position. A value of 1 means the search box is scrolled entirely to
         * the top of the screen viewport.
         *
         * @param scrollPercentage The percentage the search box has been scrolled off the page.
         */
        void onNtpScrollChanged(float scrollPercentage);
    }

    /** An observer for most visited tile clicks. */
    public interface MostVisitedTileClickObserver {
        /**
         * Called when a most visited tile is clicked.
         *
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
            return isInSingleUrlBarMode() && !mNewTabPageCoordinator.urlFocusAnimationsDisabled();
        }

        @Override
        public boolean isVoiceSearchEnabled() {
            return mVoiceRecognitionHandler != null
                    && mVoiceRecognitionHandler.isVoiceSearchEnabled();
        }

        @Override
        public void focusSearchBox(
                boolean beginVoiceSearch,
                @AutocompleteRequestType int requestType,
                boolean showFuseboxPopup,
                @Nullable String pastedText) {
            if (mIsDestroyed) return;
            FeedReliabilityLogger feedReliabilityLogger =
                    mFeedSurfaceProvider.getReliabilityLogger();
            if (mVoiceRecognitionHandler != null && beginVoiceSearch) {
                if (feedReliabilityLogger != null) {
                    feedReliabilityLogger.onVoiceSearch();
                }
                mVoiceRecognitionHandler.startVoiceRecognition(
                        VoiceRecognitionIntentHandler.VoiceInteractionSource.NTP,
                        CallbackUtils.emptyRunnable());
                mTracker.notifyEvent(EventConstants.NTP_VOICE_SEARCH_BUTTON_CLICKED);
            } else if (mOmniboxStub != null) {
                if (feedReliabilityLogger != null) {
                    feedReliabilityLogger.onOmniboxFocused();
                }

                @OmniboxFocusReason
                int focusReason =
                        pastedText == null
                                ? OmniboxFocusReason.FAKE_BOX_TAP
                                : OmniboxFocusReason.FAKE_BOX_LONG_PRESS;
                @AutocompleteState int autocompleteState = AutocompleteState.ENABLED;
                if (requestType == AutocompleteRequestType.AI_MODE) {
                    focusReason = OmniboxFocusReason.NTP_AI_MODE;
                } else if (showFuseboxPopup) {
                    focusReason = OmniboxFocusReason.FAKE_BOX_PLUS_BUTTON_TAP;
                    autocompleteState = AutocompleteState.STANDBY_NO_FOCUS;
                }

                mOmniboxStub.beginInput(
                        new AutocompleteInput()
                                .setUserText(pastedText)
                                .setFocusReason(focusReason)
                                .setRequestType(requestType)
                                .setAutocompleteState(autocompleteState));
            }
        }

        @Override
        public boolean isCurrentPage() {
            if (mIsDestroyed) return false;
            if (mOmniboxStub == null) return false;
            return getNewTabPageForCurrentTab() == NewTabPage.this;
        }

        private @Nullable NewTabPage getNewTabPageForCurrentTab() {
            Tab currentTab = mActivityTabProvider.get();
            if (currentTab == null) return null;
            NativePage nativePage = currentTab.getNativePage();
            return (nativePage instanceof NewTabPage) ? (NewTabPage) nativePage : null;
        }

        @Override
        public void onLoadingComplete() {
            if (mIsDestroyed) return;
            mIsLoaded = true;
            NewTabPageUma.recordNtpImpression(NewTabPageUma.NTP_IMPRESSION_REGULAR);

            var state = NewTabPageCreationState.from(mTab);
            if (state != null) state.onNtpLoaded(this);

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
            mNewTabPageCoordinator.onTilesLoaded();
        }

        @Override
        public void openMostVisitedItem(int windowDisposition, Tile tile) {
            if (mIsDestroyed) return;

            super.openMostVisitedItem(windowDisposition, tile);
        }
    }

    /**
     * Constructs a NewTabPage.
     *
     * @param activity The activity used for context to create the new tab page's View.
     * @param browserControlsStateProvider {@link BrowserControlsStateProvider} to observe for
     *     offset changes.
     * @param activityTabProvider Provides the current active tab.
     * @param modalDialogManager The {@link ModalDialogManager}.
     * @param snackbarManager {@link SnackbarManager} object.
     * @param lifecycleDispatcher Activity lifecycle dispatcher.
     * @param tabModelSelector {@link TabModelSelector} object.
     * @param isTablet {@code true} if running on a Tablet device.
     * @param tabCreationTracker {@link NewTabPageCreationTracker} object recording user metrics.
     * @param isInNightMode {@code true} if the night mode setting is on.
     * @param nativePageHost The host that is showing this new tab page.
     * @param tab The {@link Tab} that contains this new tab page.
     * @param url The URL that launched this new tab page.
     * @param bottomSheetController The controller for bottom sheets, used by the feed.
     * @param shareDelegateSupplier Supplies the Delegate used to open SharingHub.
     * @param windowAndroid The containing window of this page.
     * @param toolbarSupplier Supplies the {@link Toolbar}.
     * @param homeSurfaceTracker Used to decide whether we are the home surface.
     * @param activityResultTracker Tracker of activity results.
     * @param tabStripHeightSupplier Supplier for the tab strip height.
     * @param moduleRegistrySupplier Supplier for the {@link ModuleRegistry}.
     * @param edgeToEdgeControllerSupplier Supplier for the {@link EdgeToEdgeController}.
     * @param topInsetProvider Provider for top insets.
     * @param startupMetricsTracker Used to record NTP startup metric.
     * @param backPressManager Manages back press dispatching.
     */
    public NewTabPage(
            Activity activity,
            BrowserControlsStateProvider browserControlsStateProvider,
            Supplier<@Nullable Tab> activityTabProvider,
            ModalDialogManager modalDialogManager,
            SnackbarManager snackbarManager,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            TabModelSelector tabModelSelector,
            boolean isTablet,
            NewTabPageCreationTracker tabCreationTracker,
            boolean isInNightMode,
            NativePageHost nativePageHost,
            Tab tab,
            String url,
            BottomSheetController bottomSheetController,
            Supplier<@Nullable ShareDelegate> shareDelegateSupplier,
            WindowAndroid windowAndroid,
            Supplier<Toolbar> toolbarSupplier,
            @Nullable HomeSurfaceTracker homeSurfaceTracker,
            ActivityResultTracker activityResultTracker,
            NonNullObservableSupplier<Integer> tabStripHeightSupplier,
            OneshotSupplier<ModuleRegistry> moduleRegistrySupplier,
            MonotonicObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier,
            TopInsetProvider topInsetProvider,
            StartupMetricsTracker startupMetricsTracker,
            BackPressManager backPressManager) {
        mConstructedTimeNs = System.nanoTime();
        TraceEvent.begin(TAG);

        mActivity = activity;
        mActivityTabProvider = activityTabProvider;
        mActivityLifecycleDispatcher = lifecycleDispatcher;
        mTab = tab;
        mToolbarSupplier = toolbarSupplier;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mBottomSheetController = bottomSheetController;
        mIsInNightMode = isInNightMode;
        mTabStripHeightSupplier = tabStripHeightSupplier;
        mTopInsetProvider = topInsetProvider;

        Profile profile = mTab.getProfile();

        SuggestionsNavigationDelegate navigationDelegate =
                new SuggestionsNavigationDelegate(
                        activity, profile, nativePageHost, tabModelSelector, mTab);
        mNewTabPageManager =
                new NewTabPageManagerImpl(
                        navigationDelegate, profile, nativePageHost, snackbarManager);
        mTileGroupDelegate =
                new NewTabPageTileGroupDelegate(
                        activity, profile, navigationDelegate, snackbarManager);

        mContext = activity;
        mTitle = activity.getResources().getString(R.string.new_tab_title);

        mBackgroundColor = ChromeSemanticColorUtils.getHomeSurfaceBackgroundColor(activity);

        mIsTablet = isTablet;
        mTemplateUrlService = TemplateUrlServiceFactory.getForProfile(profile);
        mTemplateUrlService.addObserver(this);

        mTabObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onShown(Tab tab, @TabSelectionType int type) {
                        // Showing the NTP is only meaningful when the page has been loaded already.
                        if (mIsLoaded) {
                            recordNtpShown();
                        }
                        mNewTabPageCoordinator.maybeUpdateHomeModules(mIsLoaded);
                        mNewTabPageCoordinator.onSwitchToForeground();
                    }

                    @Override
                    public void onHidden(Tab tab, @TabHidingType int type) {
                        if (mIsLoaded) recordNtpHidden();
                    }

                    @Override
                    public void onContentChanged(Tab tab) {
                        updateNtpScrollListener(true);
                    }
                };
        mTab.addObserver(mTabObserver);

        mLifecycleObserver =
                new PauseResumeWithNativeObserver() {
                    @Override
                    public void onResumeWithNative() {
                        mNewTabPageCoordinator.maybeUpdateHomeModules(mIsLoaded);
                    }

                    @Override
                    public void onPauseWithNative() {
                        // Only record when this tab is the current tab.
                        if (mActivityTabProvider.get() == mTab) {
                            RecordUserAction.record("MobileNTPPaused");
                        }
                    }
                };
        mActivityLifecycleDispatcher.register(mLifecycleObserver);

        updateSearchProvider();

        LayoutInflater inflater = LayoutInflater.from(activity);
        // TODO(crbug.com/347509698): Remove the log statements after fixing the bug.
        Log.d(TAG, "NewTabPageLayout inflate");
        mNewTabPageLayout = (NewTabPageLayout) inflater.inflate(R.layout.new_tab_page_layout, null);
        mNewTabPageCoordinator =
                new NewTabPageCoordinator(
                        mNewTabPageManager,
                        activity,
                        mNewTabPageLayout,
                        mTab,
                        tabModelSelector,
                        moduleRegistrySupplier,
                        mTab.getProfile(),
                        windowAndroid,
                        activityResultTracker,
                        bottomSheetController,
                        modalDialogManager,
                        snackbarManager,
                        mIsTablet,
                        mTabStripHeightSupplier,
                        homeSurfaceTracker,
                        backPressManager);

        initializeFeedSurfaceProvider(
                activity,
                windowAndroid,
                activityResultTracker,
                snackbarManager,
                isInNightMode,
                shareDelegateSupplier,
                modalDialogManager,
                url,
                edgeToEdgeControllerSupplier,
                startupMetricsTracker,
                tabModelSelector,
                moduleRegistrySupplier);

        View view = getView();
        view.addOnAttachStateChangeListener(
                new View.OnAttachStateChangeListener() {

                    @Override
                    public void onViewAttachedToWindow(View view) {
                        updateMargins();
                        view.removeOnAttachStateChangeListener(this);
                    }

                    @Override
                    public void onViewDetachedFromWindow(View view) {}
                });
        mBrowserControlsStateProvider.addObserver(this);

        mToolbarHeight =
                activity.getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow);

        mSupportsEnableEdgeToEdgeOnTop =
                NtpCustomizationUtils.supportsEnableEdgeToEdgeOnTop(windowAndroid, mIsTablet);
        if (mSupportsEnableEdgeToEdgeOnTop) {
            // Apply edge-to-edge adjustments exclusively to phones. These are not required for LFF
            // devices.
            initTopInsetProviderObserver();
            initUseLightIconTint();
        }

        // The listener to observe custom background changes are added in all form factors as long
        // as the feature is enabled.
        if (NtpCustomizationUtils.isNtpThemeCustomizationEnabled()) {
            setIsUseEdgeToEdgeForCustomizedTheme();
            initHomepageStateListener();
        }

        NewTabPageUma.recordContentSuggestionsDisplayStatus(profile);

        mNewTabPageCoordinator.initialize(
                mTileGroupDelegate,
                mSearchProviderHasLogo,
                mIsDefaultSearchEngineGoogle,
                mFeedSurfaceProvider.getScrollDelegate(),
                mFeedSurfaceProvider.getTouchEnabledDelegate(),
                mFeedSurfaceProvider.getUiConfig(),
                lifecycleDispatcher,
                () -> assumeNonNull(mTemplateUrlService.getComposeplateUrl()));

        sTotalCount++;
        NewTabPageUma.recordSimultaneousNtpCount(sTotalCount);

        updateNtpScrollListener(true);

        TraceEvent.end(TAG);
    }

    /**
     * Create and initialize the FeedSurfaceProvider of this NewTabPage.
     *
     * @param activity The activity used to initialize the view.
     * @param windowAndroid Provides the current active tab.
     * @param activityResultTracker Tracker of activity results.
     * @param snackbarManager {@link SnackbarManager} object.
     * @param isInNightMode {@code true} if the night mode setting is on.
     * @param shareDelegateSupplier Supplies a delegate used to open SharingHub.
     * @param url The URL used to identify NTP's launch origin
     * @param edgeToEdgeControllerSupplier The supplier to {@link EdgeToEdgeController}.
     * @param startupMetricsTracker Used to record NTP startup metric.
     */
    @EnsuresNonNull({"mFeedSurfaceProvider"})
    protected void initializeFeedSurfaceProvider(
            Activity activity,
            WindowAndroid windowAndroid,
            ActivityResultTracker activityResultTracker,
            SnackbarManager snackbarManager,
            boolean isInNightMode,
            Supplier<@Nullable ShareDelegate> shareDelegateSupplier,
            ModalDialogManager modalDialogManager,
            String url,
            MonotonicObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier,
            StartupMetricsTracker startupMetricsTracker,
            TabModelSelector tabModelSelector,
            OneshotSupplier<ModuleRegistry> moduleRegistrySupplier) {
        Profile profile = mTab.getProfile();

        FeedSurfaceCoordinator.ActionDelegateFactory createActionDelegate =
                () ->
                        new FeedActionDelegateImpl(
                                activity,
                                windowAndroid,
                                activityResultTracker,
                                SigninAndHistorySyncActivityLauncherImpl.get(),
                                DeviceLockActivityLauncherImpl.get(),
                                snackbarManager,
                                modalDialogManager,
                                mNewTabPageManager.getNavigationDelegate(),
                                BookmarkModel.getForProfile(profile),
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
                        new SnapScrollHelperImpl(mNewTabPageManager, mNewTabPageCoordinator),
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
                        createActionDelegate,
                        mTabStripHeightSupplier,
                        edgeToEdgeControllerSupplier,
                        assumeNonNull(moduleRegistrySupplier).get());
        startupMetricsTracker.registerNtpViewObserver(mFeedSurfaceProvider.getView());
    }

    /** Initialize the single tab card on home surface NTP or magic stack. */
    private void initTopInsetProviderObserver() {
        mTopInsetChangeObserver = this::onToEdgeChange;
        mTopInsetProvider.addObserver(mTopInsetChangeObserver);
    }

    // Called when ChromeFeatureList.sNewTabPageCustomizationV2 is enabled to add a
    // HomepageStateListener.
    private void initHomepageStateListener() {
        mHomepageStateListener =
                new HomepageStateListener() {
                    @Override
                    public void onBackgroundImageChanged(
                            Bitmap originalBitmap,
                            BackgroundImageInfo backgroundImageInfo,
                            boolean fromInitialization,
                            @NtpBackgroundType int oldType,
                            @NtpBackgroundType int newType) {
                        onBackgroundChangedImpl(/* applyWhiteBackgroundOnSearchBox= */ true);
                    }

                    @Override
                    public void onBackgroundColorChanged(
                            @Nullable NtpThemeColorInfo ntpThemeColorInfo,
                            @ColorInt int backgroundColor,
                            boolean fromInitialization,
                            @NtpBackgroundType int oldType,
                            @NtpBackgroundType int newType) {
                        onBackgroundChangedImpl(/* applyWhiteBackgroundOnSearchBox= */ false);
                    }

                    @Override
                    public void onBackgroundReset(@NtpBackgroundType int oldType) {
                        onBackgroundChangedImpl(/* applyWhiteBackgroundOnSearchBox= */ false);
                    }
                };
        NtpCustomizationConfigManager.getInstance()
                .addListener(mHomepageStateListener, mContext, /* skipNotify= */ false);
    }

    private void onBackgroundChangedImpl(boolean applyWhiteBackgroundOnSearchBox) {
        setIsUseEdgeToEdgeForCustomizedTheme();

        if (!mIsTablet) {
            mUseLightIconTint = applyWhiteBackgroundOnSearchBox;
        }
        mNewTabPageCoordinator.onCustomizedBackgroundChanged(applyWhiteBackgroundOnSearchBox);
    }

    /** Initializes whether to use a light tint color on icons of toolbar and status bar. */
    private void initUseLightIconTint() {
        if (mIsTablet) return;

        @NtpBackgroundType
        int imageType = NtpCustomizationConfigManager.getInstance().getBackgroundType();
        if (imageType == NtpBackgroundType.IMAGE_FROM_DISK
                || imageType == NtpBackgroundType.THEME_COLLECTION) {
            mUseLightIconTint = true;
        } else {
            mUseLightIconTint = false;
        }
    }

    /**
     * Called when the layout changes between edge-to-edge and standard.
     *
     * @param systemTopInset The system's top inset, i.e., the height of the Status bar. While
     *     usually greater than zero, it can be zero in split-screen mode.
     * @param consumeTopInset Whether the parent layout will consume the top inset.
     * @param layoutType The current active layout type from {@link LayoutType}.
     */
    void onToEdgeChange(int systemTopInset, boolean consumeTopInset, @LayoutType int layoutType) {
        // When consumeTopInset is false, it is possible: 1) the next Tab isn't NTP and 2) the next
        // Tab is NTP while NTP should show regular toolbar. NewTabPageLayout should only be
        // adjusted based on supportsEdgeToEdgeOnTop(), not the parent view's decision.
        // However, consumeTopInset being false takes priority — if the parent is not consuming top
        // insets (e.g. status indicator is visible), the NTP must not apply its own top inset
        // either, since the content view already has the system top padding.
        mNewTabPageCoordinator.onToEdgeChange(
                systemTopInset, consumeTopInset && supportsEdgeToEdgeOnTop());
    }

    /**
     * @param isTablet Whether the activity is running in tablet mode.
     * @return Whether the NTP is in single url bar mode, i.e. the url bar is shown in-line on the
     *     NTP.
     */
    public static boolean isInSingleUrlBarMode(boolean isTablet) {
        return !isTablet;
    }

    /**
     * Update the margins for the content when browser controls constraints or bottom control height
     * are changed.
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

    /** Returns the instance of {@link NewTabPageCoordinator}. */
    @VisibleForTesting
    public NewTabPageCoordinator getNewTabPageCoordinator() {
        return mNewTabPageCoordinator;
    }

    /** Returns the view container for the new tab layout. */
    @VisibleForTesting
    public View getLayout() {
        return mNewTabPageCoordinator.getNewTabPageLayout();
    }

    /**
     * Updates whether the NewTabPage should animate on URL focus changes.
     *
     * @param disable Whether to disable the animations.
     */
    public void setUrlFocusAnimationsDisabled(boolean disable) {
        mNewTabPageCoordinator.setUrlFocusAnimationsDisabled(disable);
    }

    private boolean isInSingleUrlBarMode() {
        return isInSingleUrlBarMode(mIsTablet);
    }

    /** Updates the search provider params. */
    private void updateSearchProvider() {
        mSearchProviderHasLogo = mTemplateUrlService.doesDefaultSearchEngineHaveLogo();
        mIsDefaultSearchEngineGoogle = mTemplateUrlService.isDefaultSearchEngineGoogle();
    }

    private void onSearchEngineUpdated() {
        updateSearchProvider();

        mNewTabPageCoordinator.setSearchProviderInfo(
                mSearchProviderHasLogo, mIsDefaultSearchEngineGoogle);
        // TODO(crbug.com/40226731): Remove this call when the Feed position experiment is
        // cleaned up.
        updateMargins();
    }

    /**
     * Specifies the percentage the URL is focused during an animation. 1.0 specifies that the URL
     * bar has focus and has completed the focus animation. 0 is when the URL bar is does not have
     * any focus.
     *
     * @param percent The percentage of the URL bar focus animation.
     */
    public void setUrlFocusChangeAnimationPercent(float percent) {
        mNewTabPageCoordinator.setUrlFocusChangeAnimationPercent(percent);
    }

    /**
     * Get the bounds of the search box in relation to the top level NewTabPage view.
     *
     * @param bounds The current drawing location of the search box.
     * @param translation The translation applied to the search box by the parent view hierarchy up
     *     to the NewTabPage view.
     */
    public void getSearchBoxBounds(Rect bounds, Point translation) {
        mNewTabPageCoordinator.getSearchBoxBounds(bounds, translation, getView());
    }

    /**
     * Get the vertical inset applied to the search box bounds.
     *
     * @return The vertical inset in pixels.
     */
    public int getSearchBoxBoundsVerticalInset() {
        return mNewTabPageCoordinator.getSearchBoxBoundsVerticalInset();
    }

    /**
     * Updates the opacity of the search box when scrolling.
     *
     * @param alpha opacity (alpha) value to use.
     */
    public void setSearchBoxAlpha(float alpha) {
        mNewTabPageCoordinator.setSearchBoxAlpha(alpha);
    }

    /**
     * Updates the opacity of the search provider logo when scrolling.
     *
     * @param alpha opacity (alpha) value to use.
     */
    public void setSearchProviderLogoAlpha(float alpha) {
        mNewTabPageCoordinator.setSearchProviderLogoAlpha(alpha);
    }

    /**
     * @return Whether the location bar is shown in the NTP.
     */
    public boolean isLocationBarShownInNtp() {
        return mNewTabPageManager.isLocationBarShownInNtp();
    }

    /**
     * @see org.chromium.chrome.browser.omnibox.NewTabPageDelegate#hasCompletedFirstLayout().
     */
    public boolean hasCompletedFirstLayout() {
        return mNewTabPageCoordinator.getNewTabPageLayout().getHeight() > 0;
    }

    /**
     * @return Whether the location bar has been scrolled to top in the NTP.
     */
    public boolean isLocationBarScrolledToTopInNtp() {
        return mNewTabPageCoordinator.getToolbarTransitionPercentage() == 1;
    }

    /**
     * Sets the listener for search box scroll changes.
     *
     * @param listener The listener to be notified on changes.
     */
    public void setSearchBoxScrollListener(@Nullable OnSearchBoxScrollListener listener) {
        mNewTabPageCoordinator.setSearchBoxScrollListener(listener);
    }

    /** Sets the OmniboxStub that this page interacts with. */
    public void setOmniboxStub(OmniboxStub omniboxStub) {
        mOmniboxStub = omniboxStub;
        if (mOmniboxStub != null) {
            // The toolbar can't get the reference to the native page until its initialization is
            // finished, so we can't cache it here and transfer it to the view later. We pull that
            // state from the location bar when we get a reference to it as a workaround.
            mNewTabPageCoordinator.setUrlFocusChangeAnimationPercent(
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
            mNewTabPageCoordinator.updateActionButtonVisibility();
        }
    }

    /**
     * Returns the last touch position in the view. It will be (-1, -1) if no touches have been
     * received.
     */
    public Point getLastTouchPosition() {
        return mLastTouchPosition;
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
        mNewTabPageCoordinator.updateActionButtonVisibility();
    }

    @Override
    public SnackbarManager getSnackbarManager() {
        return mNewTabPageManager.getSnackbarManager();
    }

    /**
     * Records UMA for the NTP being shown. This includes a fresh page load or being brought to the
     * foreground.
     */
    private void recordNtpShown() {
        mLastShownTimeNs = System.nanoTime();
        RecordUserAction.record("MobileNTPShown");
        SuggestionsMetrics.recordSurfaceVisible();
        GlicHelper.maybeShowGlicTaskInProgressSnackbar(
                this, mTab.getProfile(), mActivity, GlicHelper.Caller.NEW_TAB_PAGE);
    }

    /** Records UMA for the NTP being hidden and the time spent on it. */
    private void recordNtpHidden() {
        RecordHistogram.deprecatedRecordMediumTimesHistogram(
                "NewTabPage.TimeSpent",
                (System.nanoTime() - mLastShownTimeNs) / TimeUtils.NANOSECONDS_PER_MILLISECOND);
        SuggestionsMetrics.recordSurfaceHidden();
    }

    /**
     * Returns an arbitrary string value stored in the last committed navigation entry. If the look
     * up fails, an empty string is returned.
     *
     * @param tab A tab that is used to access the NavigationController and the NavigationEntry
     *     extras.
     * @param key The string previously used to tag this piece of data.
     * @return The value previously stored with the given key.
     */
    public static String getStringFromNavigationEntry(Tab tab, String key) {
        if (tab.getWebContents() == null) return "";
        NavigationController controller = tab.getWebContents().getNavigationController();
        int index = controller.getLastCommittedEntryIndex();
        String value = controller.getEntryExtraData(index, key);
        return value != null ? value : "";
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
    @SuppressWarnings("NullAway")
    public void destroy() {
        assert !mIsDestroyed;
        assert !ViewCompat.isAttachedToWindow(getView())
                : "Destroy called before removed from window";
        if (mIsLoaded && !mTab.isHidden()) recordNtpHidden();

        mCallbackController.destroy();

        mNewTabPageCoordinator.destroy();
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
        if (mVoiceRecognitionHandler != null) {
            mVoiceRecognitionHandler.removeObserver(this);
        }

        if (mTopInsetChangeObserver != null) {
            mTopInsetProvider.removeObserver(mTopInsetChangeObserver);
            mTopInsetChangeObserver = null;
        }

        if (mHomepageStateListener != null) {
            NtpCustomizationConfigManager.getInstance().removeListener(mHomepageStateListener);
            mHomepageStateListener = null;
        }

        updateNtpScrollListener(false);

        sTotalCount--;
        mIsDestroyed = true;
    }

    @Override
    public String getUrl() {
        Profile currentProfile = mTab.getProfile();
        UrlConstantResolver urlConstantResolver =
                UrlConstantResolverFactory.getForProfile(currentProfile);
        return urlConstantResolver.getNtpUrl();
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
    public boolean useLightIconTint() {
        return mUseLightIconTint;
    }

    @Override
    public boolean supportsEdgeToEdge() {
        return true;
    }

    @Override
    public boolean supportsEdgeToEdgeOnTop() {
        return mIsUseEdgeToEdgeForCustomizedTheme;
    }

    @Override
    public @ColorInt int getToolbarTextBoxBackgroundColor(@ColorInt int defaultColor) {
        if (isLocationBarShownInNtp()) {
            if (!isLocationBarScrolledToTopInNtp()) {
                return getBackgroundColor();
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
        mNewTabPageCoordinator.reload();
    }

    @Override
    public int getTopInset() {
        return mNewTabPageCoordinator.getTopInset();
    }

    // InvalidationAwareThumbnailProvider

    @Override
    public boolean shouldCaptureThumbnail() {
        return mNewTabPageCoordinator.shouldCaptureThumbnail()
                || mFeedSurfaceProvider.shouldCaptureThumbnail()
                || mSnapshotSingleTabCardChanged;
    }

    @Override
    public void captureThumbnail(Canvas canvas) {
        mNewTabPageCoordinator.onPreCaptureThumbnail();
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
    public void sendMotionEventForInputTracking(MotionEvent ev) {
        if (ev.getAction() == MotionEvent.ACTION_DOWN) {
            mLastTouchPosition.x = Math.round(ev.getX());
            mLastTouchPosition.y = Math.round(ev.getY());
        }
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        return !(mTab != null && DeviceFormFactor.isWindowOnTablet(mTab.getWindowAndroidChecked()))
                && (mOmniboxStub != null && mOmniboxStub.isUrlBarFocused());
    }

    public void listenToFeed(
            MonotonicObservableSupplier<ReadAloudController> readAloudControllerSupplier) {
        ReadAloudController readAloudController = readAloudControllerSupplier.get();
        if (readAloudController == null) return;

        List<String> feedUrls = mFeedSurfaceProvider.getFeedUrls();
        if (feedUrls == null || feedUrls.isEmpty()) return;

        readAloudController.playOverviewForUrls(feedUrls, Entrypoint.FEED_PLAYBACK);
    }

    public FeedSurfaceCoordinator getCoordinatorForTesting() {
        return (FeedSurfaceCoordinator) mFeedSurfaceProvider;
    }

    public NewTabPageManager getNewTabPageManagerForTesting() {
        return mNewTabPageManager;
    }

    public RecyclerView.@Nullable OnScrollListener getScrollListenerForTesting() {
        return mNtpScrollListener;
    }

    public TileGroup.Delegate getTileGroupDelegateForTesting() {
        return mTileGroupDelegate;
    }

    public FeedActionDelegate getFeedActionDelegateForTesting() {
        return ((FeedSurfaceCoordinator) mFeedSurfaceProvider)
                .getActionDelegateForTesting(); // IN-TEST
    }

    /**
     * Shows the magic stack with the last active Tab if exists on the home surface NTP.
     *
     * @param mostRecentTab The last shown Tab if exists. It is non null for NTP home surface only.
     */
    public void showHomeSurfaceUiOnNtp(@Nullable Tab mostRecentTab) {
        mNewTabPageCoordinator.showHomeSurfaceUiOnNtp(mostRecentTab);
    }

    /** Sets whether the NTP is currently set as edge-to-edge. */
    private void setIsUseEdgeToEdgeForCustomizedTheme() {
        mIsUseEdgeToEdgeForCustomizedTheme =
                mSupportsEnableEdgeToEdgeOnTop
                        && !mIsTablet
                        && NtpCustomizationConfigManager.getInstance().getBackgroundType()
                                != NtpBackgroundType.DEFAULT;
    }

    public boolean isMagicStackVisibleForTesting() {
        return mNewTabPageCoordinator.isMagicStackVisibleForTesting(); // IN-TEST
    }

    public boolean getSnapshotSingleTabCardChangedForTesting() {
        return mNewTabPageCoordinator.getSnapshotSingleTabCardChangedForTesting(); // IN-TEST
    }

    @Override
    public SmoothTransitionDelegate enableSmoothTransition() {
        if (mSmoothTransitionDelegate == null) {
            mSmoothTransitionDelegate =
                    new NtpSmoothTransitionDelegate(
                            getView(), mFeedSurfaceProvider.getRestoringStateSupplier());
        }
        return mSmoothTransitionDelegate;
    }

    public @Nullable SmoothTransitionDelegate getSmoothTransitionDelegateForTesting() {
        return mSmoothTransitionDelegate;
    }

    public void enableSearchBoxEditText(boolean enable) {
        mNewTabPageCoordinator.enableSearchBoxEditText(enable);
    }

    private void updateNtpScrollListener(boolean attach) {
        if (!(mFeedSurfaceProvider instanceof FeedSurfaceCoordinator)) return;

        if (!BottomBarConfigUtils.isNtpScrollOffEnabled(mTab, mContext)) return;

        RecyclerView recyclerView =
                ((FeedSurfaceCoordinator) mFeedSurfaceProvider).getRecyclerView();
        if (recyclerView == null) {
            if (mNtpScrollListener != null) {
                mNtpScrollListener = null;
            }
            return;
        }

        if (attach && mNtpScrollListener == null) {
            mNtpScrollListener = new NtpScrollListener(mBrowserControlsStateProvider, mContext);
            recyclerView.addOnScrollListener(mNtpScrollListener);
        } else if (!attach && mNtpScrollListener != null) {
            recyclerView.removeOnScrollListener(mNtpScrollListener);
            mNtpScrollListener = null;
        }
    }

    private static class NtpScrollListener extends RecyclerView.OnScrollListener {
        private static final int SCROLL_THRESHOLD_DP = 20;

        private final WeakReference<BrowserControlsStateProvider> mControlsProviderRef;
        private final WeakReference<Context> mContextRef;

        private int mAccumulatedScrollY;

        NtpScrollListener(BrowserControlsStateProvider controlsProvider, Context context) {
            mControlsProviderRef = new WeakReference<>(controlsProvider);
            mContextRef = new WeakReference<>(context);
        }

        @Override
        public void onScrollStateChanged(RecyclerView recyclerView, int newState) {
            if (newState == RecyclerView.SCROLL_STATE_IDLE) {
                mAccumulatedScrollY = 0;
            }
        }

        @Override
        public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
            BrowserControlsStateProvider provider = mControlsProviderRef.get();
            Context context = mContextRef.get();
            if (provider == null || context == null) return;
            if (!(provider instanceof BrowserControlsVisibilityManager)) return;

            BrowserControlsVisibilityManager manager = (BrowserControlsVisibilityManager) provider;
            int bottomControlsHeight = manager.getBottomControlsHeight();
            if (bottomControlsHeight <= 0) return;

            float density = context.getResources().getDisplayMetrics().density;
            int thresholdPx = (int) (SCROLL_THRESHOLD_DP * density);

            if (Integer.signum(dy) != Integer.signum(mAccumulatedScrollY)
                    && dy != 0
                    && mAccumulatedScrollY != 0) {
                mAccumulatedScrollY = 0;
            }

            mAccumulatedScrollY += dy;

            if (mAccumulatedScrollY > thresholdPx) {
                if (!BrowserControlsUtils.areBottomControlsOffScreen(manager)) {
                    manager.hideAndroidControls(true);
                }
                mAccumulatedScrollY = 0;
            } else if (mAccumulatedScrollY < -thresholdPx) {
                if (!BrowserControlsUtils.areBottomControlsFullyVisible(manager)) {
                    manager.showAndroidControls(true);
                }
                mAccumulatedScrollY = 0;
            }
        }
    }
}
