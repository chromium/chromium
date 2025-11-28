// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Point;
import android.graphics.Rect;
import android.os.Handler;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.ContextCompat;
import androidx.core.view.ViewCompat;
import androidx.interpolator.view.animation.FastOutSlowInInterpolator;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.TimeUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.feed.FeedActionDelegateImpl;
import org.chromium.chrome.browser.back_press.BackPressMetrics;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feed.FeedActionDelegate;
import org.chromium.chrome.browser.feed.FeedReliabilityLogger;
import org.chromium.chrome.browser.feed.FeedSurfaceCoordinator;
import org.chromium.chrome.browser.feed.FeedSurfaceDelegate;
import org.chromium.chrome.browser.feed.FeedSurfaceLifecycleManager;
import org.chromium.chrome.browser.feed.FeedSurfaceProvider;
import org.chromium.chrome.browser.feed.FeedSurfaceProvider.RestoringState;
import org.chromium.chrome.browser.feed.FeedSwipeRefreshLayout;
import org.chromium.chrome.browser.feed.NtpFeedSurfaceLifecycleManager;
import org.chromium.chrome.browser.feed.componentinterfaces.SurfaceCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.LifecycleObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager;
import org.chromium.chrome.browser.magic_stack.HomeModulesCoordinator;
import org.chromium.chrome.browser.magic_stack.HomeModulesMetricsUtils;
import org.chromium.chrome.browser.magic_stack.ModuleDelegateHost;
import org.chromium.chrome.browser.magic_stack.ModuleRegistry;
import org.chromium.chrome.browser.metrics.StartupMetricsTracker;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinatorFactory;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
import org.chromium.chrome.browser.ntp_customization.edge_to_edge.TopInsetCoordinator;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.chrome.browser.omnibox.OmniboxFocusReason;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.readaloud.ReadAloudController.Entrypoint;
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
import org.chromium.chrome.browser.toolbar.top.Toolbar;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.chrome.browser.url_constants.UrlConstantResolver;
import org.chromium.chrome.browser.url_constants.UrlConstantResolverFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

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
                ModuleDelegateHost {
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
    private final ObserverList<MostVisitedTileClickObserver> mMostVisitedTileClickObservers;
    private final BottomSheetController mBottomSheetController;
    private FeedSurfaceProvider mFeedSurfaceProvider;

    private NewTabPageLayout mNewTabPageLayout;
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
    private final TabModelSelector mTabModelSelector;
    private final TemplateUrlService mTemplateUrlService;
    private final ObservableSupplier<TabContentManager> mTabContentManagerSupplier;
    private final ObservableSupplier<Integer> mTabStripHeightSupplier;

    private @Nullable SingleTabSwitcherCoordinator mSingleTabSwitcherCoordinator;
    private @Nullable ViewGroup mSingleTabCardContainer;
    private @Nullable HomeModulesCoordinator mHomeModulesCoordinator;
    private @Nullable ViewGroup mHomeModulesContainer;
    private final ObservableSupplierImpl<Tab> mMostRecentTabSupplier =
            new ObservableSupplierImpl<>();
    private @Nullable Point mContextMenuStartPosition;

    private final Activity mActivity;
    private final @Nullable HomeSurfaceTracker mHomeSurfaceTracker;
    private boolean mSnapshotSingleTabCardChanged;
    private final boolean mIsInNightMode;
    private final @Nullable OneshotSupplier<ModuleRegistry> mModuleRegistrySupplier;
    private final boolean mCanSupportEdgeToEdgeForCustomizedTheme;
    private final ObservableSupplier<TopInsetCoordinator> mTopInsetCoordinatorSupplier;
    private @Nullable Callback<TopInsetCoordinator> mTopInsetCoordinatorCallback;

    private TopInsetCoordinator.@org.chromium.build.annotations.Nullable Observer
            mTopInsetChangeObserver;
    private NtpCustomizationConfigManager.@org.chromium.build.annotations.Nullable
            HomepageStateListener
            mHomepageStateListener;
    // A flag to use light tint on toolbar and status bar icons.
    private boolean mUseLightIconTint;

    private @Nullable SearchResumptionModuleCoordinator mSearchResumptionModuleCoordinator;
    private @Nullable NtpSmoothTransitionDelegate mSmoothTransitionDelegate;

    private final CallbackController mCallbackController = new CallbackController();

    @VisibleForTesting
    public static class NtpSmoothTransitionDelegate implements SmoothTransitionDelegate {
        private static final int SMOOTH_TRANSITION_DURATION_MS = 100;

        private final View mView;
        private Animator mAnimator;
        private ObservableSupplier<Integer> mRestoringState;
        private boolean mAnimatorStarted;
        private final Handler mHandler = new Handler();
        final Callback<Integer> mOnScrollStateChanged =
                new Callback<>() {
                    @Override
                    public void onResult(Integer restoreState) {
                        if (restoreState == RestoringState.NO_STATE_TO_RESTORE
                                || restoreState == RestoringState.RESTORED) {
                            mAnimator.start();
                            mRestoringState.removeObserver(this);
                            mAnimatorStarted = true;
                            mHandler.removeCallbacks(mFallback);
                            BackPressMetrics.recordNTPSmoothTransitionMethod(false);
                        }
                    }
                };
        private final Runnable mFallback =
                () -> {
                    if (!mAnimatorStarted) {
                        mAnimator.start();
                        mAnimatorStarted = true;
                        mRestoringState.removeObserver(mOnScrollStateChanged);
                        BackPressMetrics.recordNTPSmoothTransitionMethod(true);
                    }
                };

        public NtpSmoothTransitionDelegate(View view, ObservableSupplier<Integer> restoringState) {
            mView = view;
            mAnimator = buildSmoothTransition(view);
            mRestoringState = restoringState;

            // Fallback added for metric records only.
            restoringState.addObserver(
                    new Callback<>() {
                        long mStart;

                        @Override
                        public void onResult(@Nullable Integer result) {
                            assumeNonNull(result);
                            if (result == RestoringState.WAITING_TO_RESTORE) {
                                mStart = TimeUtils.currentTimeMillis();
                            } else if (result == RestoringState.RESTORED) {
                                BackPressMetrics.recordNTPFeedRestorationDuration(
                                        TimeUtils.currentTimeMillis() - mStart);
                            }
                        }
                    });
        }

        @Override
        public void prepare() {
            assert !mAnimator.isRunning() : "Previous animation should not be running";
            assert !mAnimatorStarted : "Previous animation should not be finished or cancelled.";
            cancel();
            mView.setAlpha(0f);
        }

        @Override
        public void start(Runnable onEnd) {
            assert !mAnimator.isRunning() : "Previous animation have been done or cancelled";
            mAnimatorStarted = false;

            mAnimator.addListener(
                    new AnimatorListenerAdapter() {
                        @Override
                        public void onAnimationEnd(Animator animation) {
                            onEnd.run();
                        }
                    });
            mRestoringState.addObserver(mOnScrollStateChanged);
            mHandler.postDelayed(
                    mFallback, BackPressMetrics.MAX_FALLBACK_DELAY_NTP_SMOOTH_TRANSITION);
        }

        @Override
        public void cancel() {
            mRestoringState.removeObserver(mOnScrollStateChanged);
            mHandler.removeCallbacks(mFallback);
            mAnimator.cancel();
            mView.setAlpha(1f);
        }

        private static Animator buildSmoothTransition(View view) {
            var animator = ObjectAnimator.ofFloat(view, View.ALPHA, 0f, 1f);
            animator.setInterpolator(new FastOutSlowInInterpolator());
            animator.setDuration(SMOOTH_TRANSITION_DURATION_MS);
            return animator;
        }

        public Animator getAnimatorForTesting() {
            return mAnimator;
        }
    }

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
            return isInSingleUrlBarMode() && !mNewTabPageLayout.urlFocusAnimationsDisabled();
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
                @Nullable String pastedText) {
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

                @OmniboxFocusReason
                int focusReason =
                        pastedText == null
                                ? OmniboxFocusReason.FAKE_BOX_TAP
                                : OmniboxFocusReason.FAKE_BOX_LONG_PRESS;
                if (requestType == AutocompleteRequestType.AI_MODE) {
                    focusReason = OmniboxFocusReason.NTP_AI_MODE;
                }

                mOmniboxStub.setUrlBarFocus(true, pastedText, focusReason, requestType);
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
     * @param tabContentManagerSupplier Used to create tab thumbnails.
     * @param tabStripHeightSupplier Supplier for the tab strip height.
     * @param moduleRegistrySupplier Supplier for the {@link ModuleRegistry}.
     * @param edgeToEdgeControllerSupplier Supplier for the {@link EdgeToEdgeController}.
     * @param startupMetricsTracker Used to record NTP startup metric.
     * @param multiInstanceManager multiInstanceManager An instance of the {@link
     *     MultiInstanceManager}.
     */
    public NewTabPage(
            Activity activity,
            BrowserControlsStateProvider browserControlsStateProvider,
            Supplier<@Nullable Tab> activityTabProvider,
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
            Supplier<ShareDelegate> shareDelegateSupplier,
            WindowAndroid windowAndroid,
            Supplier<Toolbar> toolbarSupplier,
            @Nullable HomeSurfaceTracker homeSurfaceTracker,
            ObservableSupplier<TabContentManager> tabContentManagerSupplier,
            ObservableSupplier<Integer> tabStripHeightSupplier,
            OneshotSupplier<ModuleRegistry> moduleRegistrySupplier,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier,
            ObservableSupplier<TopInsetCoordinator> topInsetCoordinatorSupplier,
            StartupMetricsTracker startupMetricsTracker,
            MultiInstanceManager multiInstanceManager) {
        mConstructedTimeNs = System.nanoTime();
        TraceEvent.begin(TAG);

        mActivity = activity;
        mActivityTabProvider = activityTabProvider;
        mActivityLifecycleDispatcher = lifecycleDispatcher;
        mTab = tab;
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
        mTopInsetCoordinatorSupplier = topInsetCoordinatorSupplier;

        Profile profile = mTab.getProfile();

        SuggestionsNavigationDelegate navigationDelegate =
                new SuggestionsNavigationDelegate(
                        activity,
                        profile,
                        nativePageHost,
                        tabModelSelector,
                        mTab,
                        multiInstanceManager);
        mNewTabPageManager =
                new NewTabPageManagerImpl(
                        navigationDelegate, profile, nativePageHost, snackbarManager);
        mTileGroupDelegate =
                new NewTabPageTileGroupDelegate(
                        activity, profile, navigationDelegate, snackbarManager);

        mContext = activity;
        mTitle = activity.getResources().getString(R.string.new_tab_title);

        mBackgroundColor = ContextCompat.getColor(mContext, R.color.home_surface_background_color);

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

        updateSearchProvider();
        initializeMainView(
                activity,
                windowAndroid,
                snackbarManager,
                isInNightMode,
                shareDelegateSupplier,
                url,
                edgeToEdgeControllerSupplier,
                startupMetricsTracker);

        // It is possible that the NewTabPage is created when the Tab model hasn't been initialized.
        // For example, the user changes theme when a NTP is showing, which leads to the recreation
        // of the ChromeTabbedActivity and showing the NTP as the last visited Tab.
        TabModelUtils.runOnTabStateInitialized(
                mTabModelSelector,
                mCallbackController.makeCancelable(
                        unusedTabModelSelector -> mayCreateSearchResumptionModule(profile)));

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

        mCanSupportEdgeToEdgeForCustomizedTheme =
                NtpCustomizationUtils.canEnableEdgeToEdgeForCustomizedTheme(
                        windowAndroid, mIsTablet);
        if (mCanSupportEdgeToEdgeForCustomizedTheme) {
            initTopInsetCoordinatorObserver();
            initHomepageStateListener();
            initUseLightIconTint();
        }

        NewTabPageUma.recordContentSuggestionsDisplayStatus(profile);

        mNewTabPageLayout.initialize(
                mNewTabPageManager,
                activity,
                mTileGroupDelegate,
                mSearchProviderHasLogo,
                mIsDefaultSearchEngineGoogle,
                mFeedSurfaceProvider.getScrollDelegate(),
                mFeedSurfaceProvider.getTouchEnabledDelegate(),
                mFeedSurfaceProvider.getUiConfig(),
                lifecycleDispatcher,
                mTab.getProfile(),
                windowAndroid,
                mIsTablet,
                mTabStripHeightSupplier,
                () -> assumeNonNull(mTemplateUrlService.getComposeplateUrl()));

        initializeHomeModules();

        sTotalCount++;
        NewTabPageUma.recordSimultaneousNtpCount(sTotalCount);

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
     * @param startupMetricsTracker Used to record NTP startup metric.
     */
    @EnsuresNonNull({"mNewTabPageLayout", "mFeedSurfaceProvider"})
    protected void initializeMainView(
            Activity activity,
            WindowAndroid windowAndroid,
            SnackbarManager snackbarManager,
            boolean isInNightMode,
            Supplier<ShareDelegate> shareDelegateSupplier,
            String url,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier,
            StartupMetricsTracker startupMetricsTracker) {
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
        startupMetricsTracker.registerNtpViewObserver(mFeedSurfaceProvider.getView());
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
                assumeNonNull(mHomeSurfaceTracker);
                // Case 2) on home surface NTP via back operations.
                showMagicStack(mHomeSurfaceTracker.getLastActiveTabToTrack());
            } else if (mTab.getLaunchType() != TabLaunchType.FROM_STARTUP) {
                // Case 1) on normal NTP.
                showMagicStack(null);
            }
        } else if (isTrackingTabReady) { // On NTP home surface with magic stack disabled.
            assumeNonNull(mHomeSurfaceTracker);
            showHomeSurfaceUi(mHomeSurfaceTracker.getLastActiveTabToTrack());
        }

        if (isTrackingTabReady) {
            ReturnToChromeUtil.recordHomeSurfaceShown();
        }
    }

    private void initTopInsetCoordinatorObserver() {
        mTopInsetChangeObserver = this::onToEdgeChange;
        var topInsetCoordinator = mTopInsetCoordinatorSupplier.get();
        if (topInsetCoordinator != null) {
            topInsetCoordinator.addObserver(mTopInsetChangeObserver);
            return;
        }

        mTopInsetCoordinatorCallback =
                coordinator -> {
                    coordinator.addObserver(assumeNonNull(mTopInsetChangeObserver));
                    if (mTopInsetCoordinatorCallback != null) {
                        mTopInsetCoordinatorSupplier.removeObserver(mTopInsetCoordinatorCallback);
                        mTopInsetCoordinatorCallback = null;
                    }
                };
        mTopInsetCoordinatorSupplier.addObserver(mTopInsetCoordinatorCallback);
    }

    // Called when ChromeFeatureList.sNewTabPageCustomizationV2 is enabled to add a
    // HomepageStateListener.
    private void initHomepageStateListener() {
        mHomepageStateListener =
                new NtpCustomizationConfigManager.HomepageStateListener() {
                    @Override
                    public void onBackgroundImageChanged(
                            Bitmap originalBitmap,
                            @Nullable BackgroundImageInfo backgroundImageInfo,
                            boolean fromInitialization,
                            @NtpBackgroundImageType int oldType,
                            @NtpBackgroundImageType int newType) {
                        mUseLightIconTint = true;
                        if (NtpCustomizationUtils.shouldApplyWhiteBackgroundOnSearchBox(oldType)) {
                            return;
                        }
                        mNewTabPageLayout.onCustomizedBackgroundChanged(
                                /* applyWhiteBackgroundOnSearchBox= */ true);
                    }

                    @Override
                    public void onBackgroundColorChanged(
                            @Nullable NtpThemeColorInfo ntpThemeColorInfo,
                            @ColorInt int backgroundColor,
                            boolean fromInitialization,
                            @NtpBackgroundImageType int oldType,
                            @NtpBackgroundImageType int newType) {
                        mUseLightIconTint = false;

                        if (!NtpCustomizationUtils.shouldApplyWhiteBackgroundOnSearchBox(oldType)) {
                            return;
                        }
                        // Resets the fake search box's background.
                        mNewTabPageLayout.onCustomizedBackgroundChanged(
                                /* applyWhiteBackgroundOnSearchBox= */ false);
                    }
                };
        NtpCustomizationConfigManager.getInstance().addListener(mHomepageStateListener, mContext);
    }

    /** Initializes whether to use a light tint color on icons of toolbar and status bar. */
    private void initUseLightIconTint() {
        if (mIsTablet) return;

        @NtpBackgroundImageType
        int imageType = NtpCustomizationConfigManager.getInstance().getBackgroundImageType();
        if (imageType == NtpBackgroundImageType.IMAGE_FROM_DISK
                || imageType == NtpBackgroundImageType.THEME_COLLECTION) {
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
     */
    void onToEdgeChange(int systemTopInset, boolean consumeTopInset) {
        // When consumeTopInset is false, it is possible: 1) the next Tab isn't NTP and 2) the next
        // Tab is NTP while NTP should show regular toolbar. NewTabPageLayout should only be
        // adjusted based on supportsEdgeToEdgeOnTop(), not the parent view's decision.
        mNewTabPageLayout.onToEdgeChange(systemTopInset, supportsEdgeToEdgeOnTop());
    }

    /**
     * @param isTablet Whether the activity is running in tablet mode.
     * @param searchProviderHasLogo Whether the default search engine has logo.
     * @return Whether the NTP is in single url bar mode, i.e. the url bar is shown in-line on the
     *     NTP.
     */
    public static boolean isInSingleUrlBarMode(boolean isTablet, boolean searchProviderHasLogo) {
        return !isTablet
                && (searchProviderHasLogo
                        || OmniboxFeatures.sOmniboxMobileParityUpdateV2.isEnabled());
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

    /**
     * @return The view container for the new tab layout.
     */
    @VisibleForTesting
    public NewTabPageLayout getNewTabPageLayout() {
        return mNewTabPageLayout;
    }

    /**
     * Updates whether the NewTabPage should animate on URL focus changes.
     *
     * @param disable Whether to disable the animations.
     */
    public void setUrlFocusAnimationsDisabled(boolean disable) {
        mNewTabPageLayout.setUrlFocusAnimationsDisabled(disable);
    }

    private boolean isInSingleUrlBarMode() {
        return isInSingleUrlBarMode(mIsTablet, mSearchProviderHasLogo);
    }

    /**
     * Updates the search provider params.
     *
     * @return Whether any of the search provider params changed.
     */
    private boolean updateSearchProvider() {
        boolean searchProviderHasLogo = mTemplateUrlService.doesDefaultSearchEngineHaveLogo();
        boolean isDefaultSearchEngineGoogle = mTemplateUrlService.isDefaultSearchEngineGoogle();
        boolean isChanged =
                mSearchProviderHasLogo != searchProviderHasLogo
                        || mIsDefaultSearchEngineGoogle != isDefaultSearchEngineGoogle;

        mSearchProviderHasLogo = searchProviderHasLogo;
        mIsDefaultSearchEngineGoogle = isDefaultSearchEngineGoogle;
        return isChanged;
    }

    private void onSearchEngineUpdated() {
        boolean isChanged = updateSearchProvider();

        mNewTabPageLayout.setSearchProviderInfo(
                mSearchProviderHasLogo, mIsDefaultSearchEngineGoogle);
        // TODO(crbug.com/40226731): Remove this call when the Feed position experiment is
        // cleaned up.
        updateMargins();

        if (isChanged && !OmniboxFeatures.sOmniboxMobileParityUpdateV2.isEnabled()) {
            NtpCustomizationConfigManager.getInstance()
                    .notifyRefreshWindowInsets(isInSingleUrlBarMode());
        }
    }

    /**
     * Specifies the percentage the URL is focused during an animation. 1.0 specifies that the URL
     * bar has focus and has completed the focus animation. 0 is when the URL bar is does not have
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
     *     to the NewTabPage view.
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
     * @return Whether the location bar is shown in the NTP.
     */
    public boolean isLocationBarShownInNtp() {
        return mNewTabPageManager.isLocationBarShownInNtp();
    }

    /**
     * @see org.chromium.chrome.browser.omnibox.NewTabPageDelegate#hasCompletedFirstLayout().
     */
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
     *
     * @param listener The listener to be notified on changes.
     */
    public void setSearchBoxScrollListener(@Nullable OnSearchBoxScrollListener listener) {
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
        SuggestionsMetrics.recordSurfaceVisible();
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
        if (mSearchResumptionModuleCoordinator != null) {
            mSearchResumptionModuleCoordinator.destroy();
        }
        if (mSingleTabSwitcherCoordinator != null) {
            destroySingleTabCard();
        }
        if (mHomeModulesCoordinator != null) {
            mHomeModulesCoordinator.destroy();
        }

        var topInsetCoordinator = mTopInsetCoordinatorSupplier.get();
        if (topInsetCoordinator != null && mTopInsetChangeObserver != null) {
            topInsetCoordinator.removeObserver(mTopInsetChangeObserver);
            mTopInsetChangeObserver = null;
        }

        if (mHomepageStateListener != null) {
            NtpCustomizationConfigManager.getInstance().removeListener(mHomepageStateListener);
            mHomepageStateListener = null;
        }

        if (mTopInsetCoordinatorCallback != null) {
            mTopInsetCoordinatorSupplier.removeObserver(mTopInsetCoordinatorCallback);
            mTopInsetCoordinatorCallback = null;
        }

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
        return mCanSupportEdgeToEdgeForCustomizedTheme
                && !mIsTablet
                && isInSingleUrlBarMode()
                && NtpCustomizationConfigManager.getInstance().getBackgroundImageType()
                        != NtpBackgroundImageType.DEFAULT;
    }

    @Override
    public @ColorInt int getToolbarTextBoxBackgroundColor(@ColorInt int defaultColor) {
        if (isLocationBarShownInNtp()) {
            if (!isLocationBarScrolledToTopInNtp()) {
                return ContextCompat.getColor(mContext, R.color.home_surface_background_color);
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

    @Override
    public int getTopInset() {
        return mNewTabPageLayout.getTopInset();
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

    public void listenToFeed(Supplier<ReadAloudController> readAloudControllerSupplier) {
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

    public TileGroup.Delegate getTileGroupDelegateForTesting() {
        return mTileGroupDelegate;
    }

    public FeedActionDelegate getFeedActionDelegateForTesting() {
        return ((FeedSurfaceCoordinator) mFeedSurfaceProvider)
                .getActionDelegateForTesting(); // IN-TEST
    }

    @Nullable TabObserver getTabObserverForTesting() {
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
     * Shows the home surface UI on this NTP. TODO(crbug.com/40263286): Investigate better solution
     * to show Home surface UI on NTP upon creation. to show Home surface UI on NTP upon creation.
     */
    public void showHomeSurfaceUi(@Nullable Tab mostRecentTab) {
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
    public void showMagicStack(@Nullable Tab mostRecentTab) {
        if (mModuleRegistrySupplier == null || mModuleRegistrySupplier.get() == null) {
            return;
        }

        if (mostRecentTab != null && !UrlUtilities.isNtpUrl(mostRecentTab.getUrl())) {
            mMostRecentTabSupplier.set(mostRecentTab);
        }

        if (mHomeModulesCoordinator == null) {
            initializeMagicStack();
        }
        mHomeModulesCoordinator.show(this::onMagicStackShown);
    }

    /** Show the module when the current new tab page is been used as the home surface. */
    private void initializeSingleTabCard(@Nullable Tab mostRecentTab) {
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
    @EnsuresNonNull({"mHomeModulesContainer", "mHomeModulesCoordinator"})
    private void initializeMagicStack() {
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
                        assertNonNull(assumeNonNull(mModuleRegistrySupplier).get()));
    }

    private void onMagicStackShown(boolean isVisible) {
        assumeNonNull(mHomeModulesContainer);
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
                .getTabRemover()
                .closeTabs(
                        TabClosureParams.closeTab(mTab).allowUndo(false).build(),
                        /* allowDialog= */ false);
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

    /** Destroy the single tab card on the {@link NewTabPageLayout}. */
    @VisibleForTesting
    void destroySingleTabCard() {
        if (mSingleTabCardContainer != null) mSingleTabCardContainer.removeAllViews();
        if (mSingleTabSwitcherCoordinator != null) {
            mSingleTabSwitcherCoordinator.hide();
            mSingleTabSwitcherCoordinator.destroy();
        }
        mSingleTabSwitcherCoordinator = null;
    }

    public boolean getSnapshotSingleTabCardChangedForTesting() {
        return mSnapshotSingleTabCardChanged;
    }

    @Override
    public @Nullable Point getContextMenuStartPoint() {
        return mContextMenuStartPosition;
    }

    @Override
    public @Nullable UiConfig getUiConfig() {
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
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION)) {
            NtpCustomizationCoordinatorFactory.getInstance()
                    .create(
                            mContext,
                            mBottomSheetController,
                            mTab::getProfile,
                            NtpCustomizationCoordinator.BottomSheetType.NTP_CARDS)
                    .showBottomSheet();
        } else {
            HomeModulesConfigManager.getInstance().onMenuClick(mContext);
        }
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
        var mostRecentTab = mMostRecentTabSupplier.get();
        if (mostRecentTab == null) {
            return null;
        }

        return mMostRecentTabSupplier.get();
    }

    @Override
    public boolean isHomeSurface() {
        // Can only show a local tab to resume if we we have a tracked tab. The presence of the
        // local tab to resume module is effectively what being a home surface is.
        return mMostRecentTabSupplier.get() != null;
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
        mNewTabPageLayout.enableSearchBoxEditText(enable);
    }
}
