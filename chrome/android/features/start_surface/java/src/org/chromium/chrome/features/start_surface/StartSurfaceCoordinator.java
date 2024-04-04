// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.app.Activity;
import android.content.res.Resources;
import android.os.SystemClock;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.android.material.appbar.AppBarLayout;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.MathUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.jank_tracker.JankTracker;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.feed.FeedSwipeRefreshLayout;
import org.chromium.chrome.browser.feed.ScrollListener;
import org.chromium.chrome.browser.feed.ScrollableContainerDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.init.ChromeActivityNativeDelegate;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.logo.LogoUtils;
import org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager;
import org.chromium.chrome.browser.magic_stack.HomeModulesCoordinator;
import org.chromium.chrome.browser.magic_stack.ModuleRegistry;
import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesCoordinator;
import org.chromium.chrome.browser.suggestions.tile.TileGroupDelegateImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementDelegate.TabSwitcherType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherCustomViewManager;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.toolbar.top.Toolbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.features.tasks.MostVisitedTileNavigationDelegate;
import org.chromium.chrome.features.tasks.SingleTabSwitcherCoordinator;
import org.chromium.chrome.features.tasks.TasksSurfaceProperties;
import org.chromium.chrome.features.tasks.TasksView;
import org.chromium.chrome.features.tasks.TasksViewBinder;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Root coordinator that is responsible for showing start surfaces, like a grid of Tabs, explore
 * surface and the bottom bar to switch between them.
 */
public class StartSurfaceCoordinator implements StartSurface {
    private final Activity mActivity;
    private final StartSurfaceMediator mStartSurfaceMediator;
    private final boolean mIsStartSurfaceEnabled;
    private final BottomSheetController mBottomSheetController;
    private final Supplier<Tab> mParentTabSupplier;
    private final WindowAndroid mWindowAndroid;
    private final JankTracker mJankTracker;
    private ViewGroup mContainerView;
    private final TabModelSelector mTabModelSelector;
    private final BrowserControlsManager mBrowserControlsManager;
    private final SnackbarManager mSnackbarManager;
    private final Supplier<ShareDelegate> mShareDelegateSupplier;
    private final Supplier<OmniboxStub> mOmniboxStubSupplier;
    private final TabContentManager mTabContentManager;
    private final ChromeActivityNativeDelegate mChromeActivityNativeDelegate;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final TabCreatorManager mTabCreatorManager;
    private final Supplier<Toolbar> mToolbarSupplier;
    // TODO(crbug.com/1315676): Directly return the supplier from {@link TabSwitcherCoordinator}.
    private final ObservableSupplierImpl<TabSwitcherCustomViewManager>
            mTabSwitcherCustomViewManagerSupplier;

    @VisibleForTesting
    static final String START_SHOWN_AT_STARTUP_UMA = "Startup.Android.StartSurfaceShownAtStartup";

    private static final String TAG = "StartSurface";

    private final boolean mUseMagicSpace;

    private PropertyModelChangeProcessor mStartSurfaceWithParentViewPropertyModelChangeProcessor;
    private PropertyModelChangeProcessor mStartSurfacePropertyModelChangeProcessor;

    // Non-null in SurfaceMode.SINGLE_PANE modes.
    @Nullable private ExploreSurfaceCoordinatorFactory mExploreSurfaceCoordinatorFactory;

    // Non-null in SurfaceMode.SINGLE_PANE modes.
    // TODO(crbug.com/982018): Get rid of this reference since the mediator keeps a reference to it.
    @Nullable private PropertyModel mPropertyModel;

    // Whether the {@link initWithNative()} is called.
    private boolean mIsInitializedWithNative;

    // A flag of whether there is a pending call to {@link initialize()} but waiting for native's
    // initialization.
    private boolean mIsInitPending;

    // Listeners used by the contained surfaces (e.g., Explore) to listen to the scroll changes on
    // the main scrollable container of the start surface.
    private final ObserverList<ScrollListener> mScrollListeners =
            new ObserverList<ScrollListener>();

    // Time at which constructor started to run. Used for feed reliability logging.
    private final long mConstructedTimeNs;

    @Nullable
    private AppBarLayout.OnOffsetChangedListener mOffsetChangedListenerToGenerateScrollEvents;

    // For pull-to-refresh.
    @Nullable private FeedSwipeRefreshLayout mSwipeRefreshLayout;

    // The single or carousel Tab switcher module on the Start surface. Null when magic stack is
    // enabled.
    @Nullable private TabSwitcher mTabSwitcherModule;
    // The view of Start surface layout.
    private TasksView mView;
    private MostVisitedTilesCoordinator mMostVisitedCoordinator;
    private MostVisitedSuggestionsUiDelegate mSuggestionsUiDelegate;
    private TileGroupDelegateImpl mTileGroupDelegate;
    private ObservableSupplier<Profile> mProfileSupplier;
    private boolean mIsMVTilesInitialized;
    private final boolean mIsSurfacePolishEnabled;
    private final ObservableSupplier<Integer> mTabStripHeightSupplier;
    private final OneshotSupplier<ModuleRegistry> mModuleRegistrySupplier;

    private class ScrollableContainerDelegateImpl implements ScrollableContainerDelegate {
        @Override
        public void addScrollListener(ScrollListener listener) {
            mScrollListeners.addObserver(listener);
        }

        @Override
        public void removeScrollListener(ScrollListener listener) {
            mScrollListeners.removeObserver(listener);
        }

        @Override
        public int getVerticalScrollOffset() {
            // Always return 0 because the offset is directly provided by the observer.
            return 0;
        }

        @Override
        public int getRootViewHeight() {
            return mContainerView.getHeight();
        }

        @Override
        public int getTopPositionRelativeToContainerView(View childView) {
            int[] pos = new int[2];
            ViewUtils.getRelativeLayoutPosition(mContainerView, childView, pos);
            return pos[1];
        }
    }

    /**
     * @param activity The current Android {@link Activity}.
     * @param sheetController Controls the bottom sheet.
     * @param startSurfaceOneshotSupplier Supplies the start surface.
     * @param parentTabSupplier Supplies the current parent {@link Tab}.
     * @param hadWarmStart Whether the application had a warm start.
     * @param windowAndroid The current {@link WindowAndroid}.a
     * @param jankTracker asd
     * @param containerView The container {@link ViewGroup} for this ui, also the root view for
     *     StartSurface.
     * @param tabModelSelector The current {@link TabModelSelector}.
     * @param browserControlsManager Manages the browser controls.
     * @param snackbarManager Manages the snackbar.
     * @param shareDelegateSupplier Supplies the {@link ShareDelegate}.
     * @param omniboxStubSupplier Supplies the {@link OmniboxStub}.
     * @param tabContentManager Manages the tab content.
     * @param chromeActivityNativeDelegate An activity delegate to handle native initialization.
     * @param activityLifecycleDispatcher Allows observation of the activity lifecycle.
     * @param tabCreatorManager Manages {@link Tab} creation.
     * @param toolbarSupplier Supplies the {@link Toolbar}.
     * @param backPressManager {@link BackPressManager} to handle back press.
     * @param profileSupplier Supplies the {@Profile}.
     * @param tabStripHeightSupplier Supplier for the tab strip height.
     * @param moduleRegistrySupplier Supplier of the {@link ModuleRegistry}.
     */
    public StartSurfaceCoordinator(
            @NonNull Activity activity,
            @NonNull BottomSheetController sheetController,
            @NonNull OneshotSupplierImpl<StartSurface> startSurfaceOneshotSupplier,
            @NonNull Supplier<Tab> parentTabSupplier,
            boolean hadWarmStart,
            @NonNull WindowAndroid windowAndroid,
            @NonNull JankTracker jankTracker,
            @NonNull ViewGroup containerView,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull BrowserControlsManager browserControlsManager,
            @NonNull SnackbarManager snackbarManager,
            @NonNull Supplier<ShareDelegate> shareDelegateSupplier,
            @NonNull Supplier<OmniboxStub> omniboxStubSupplier,
            @NonNull TabContentManager tabContentManager,
            @NonNull ChromeActivityNativeDelegate chromeActivityNativeDelegate,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull Supplier<Toolbar> toolbarSupplier,
            BackPressManager backPressManager,
            @NonNull ObservableSupplier<Profile> profileSupplier,
            @NonNull ObservableSupplier<Integer> tabStripHeightSupplier,
            @NonNull OneshotSupplier<ModuleRegistry> moduleRegistrySupplier) {
        mConstructedTimeNs = SystemClock.elapsedRealtimeNanos();
        mActivity = activity;
        mIsStartSurfaceEnabled = ReturnToChromeUtil.isStartSurfaceEnabled(mActivity);
        mBottomSheetController = sheetController;
        mParentTabSupplier = parentTabSupplier;
        mWindowAndroid = windowAndroid;
        mJankTracker = jankTracker;
        mContainerView = containerView;
        mTabModelSelector = tabModelSelector;
        mBrowserControlsManager = browserControlsManager;
        mSnackbarManager = snackbarManager;
        mShareDelegateSupplier = shareDelegateSupplier;
        mOmniboxStubSupplier = omniboxStubSupplier;
        mTabContentManager = tabContentManager;
        mChromeActivityNativeDelegate = chromeActivityNativeDelegate;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mTabCreatorManager = tabCreatorManager;
        mToolbarSupplier = toolbarSupplier;
        mProfileSupplier = profileSupplier;
        mTabStripHeightSupplier = tabStripHeightSupplier;
        mModuleRegistrySupplier = moduleRegistrySupplier;

        mUseMagicSpace = mIsStartSurfaceEnabled && StartSurfaceConfiguration.useMagicStack();
        mTabSwitcherCustomViewManagerSupplier = new ObservableSupplierImpl<>();
        mIsSurfacePolishEnabled = ChromeFeatureList.sSurfacePolish.isEnabled();

        assert mIsStartSurfaceEnabled;

        // createSwipeRefreshLayout has to be called before creating any surface.
        createSwipeRefreshLayout();
        createStartSurface();
        Runnable initializeMVTilesRunnable = this::initializeMVTiles;
        View logoContainerView = mView.findViewById(R.id.logo_container);
        ViewGroup feedPlaceholderParentView = mView.findViewById(R.id.tasks_surface_body);

        mStartSurfaceMediator =
                new StartSurfaceMediator(
                        mTabSwitcherModule,
                        mTabModelSelector,
                        mPropertyModel,
                        mIsStartSurfaceEnabled,
                        mActivity,
                        mBrowserControlsManager,
                        this::isActivityFinishingOrDestroyed,
                        mTabCreatorManager,
                        hadWarmStart,
                        initializeMVTilesRunnable,
                        (moduleDelegateHost) ->
                                new HomeModulesCoordinator(
                                        mActivity,
                                        moduleDelegateHost,
                                        mView.findViewById(R.id.task_surface_header),
                                        HomeModulesConfigManager.getInstance(),
                                        mProfileSupplier,
                                        mModuleRegistrySupplier.get()),
                        mParentTabSupplier,
                        logoContainerView,
                        backPressManager,
                        feedPlaceholderParentView,
                        mActivityLifecycleDispatcher,
                        mProfileSupplier);

        startSurfaceOneshotSupplier.set(this);
    }

    // Implements StartSurface.
    @Override
    public void initialize() {
        // TODO (crbug.com/1041047): Move more stuff from the constructor to here for lazy
        // initialization.
        if (!mIsInitializedWithNative) {
            mIsInitPending = true;
            return;
        }

        mIsInitPending = false;
    }

    @Override
    public void destroy() {
        onHide();
        if (mOffsetChangedListenerToGenerateScrollEvents != null) {
            removeHeaderOffsetChangeListener(mOffsetChangedListenerToGenerateScrollEvents);
            mOffsetChangedListenerToGenerateScrollEvents = null;
        }
        mProfileSupplier = null;
        if (mStartSurfaceMediator != null) {
            mStartSurfaceMediator.destroy();
        }
    }

    @Override
    public void onHide() {
        if (mIsInitializedWithNative) {
            mStartSurfaceMediator.mayRecordHomepageSessionEnd();
            if (mSuggestionsUiDelegate != null) {
                mSuggestionsUiDelegate.onDestroy();
                mSuggestionsUiDelegate = null;
            }
            if (mTileGroupDelegate != null) {
                mTileGroupDelegate.destroy();
                mTileGroupDelegate = null;
            }
            if (mMostVisitedCoordinator != null) {
                mMostVisitedCoordinator.destroyMvtiles();
                mIsMVTilesInitialized = false;
            }
        }
        mStartSurfaceMediator.onHide();
    }

    @Override
    public void show(boolean animate) {
        if (!mUseMagicSpace) {
            getSingleTabListDelegate().prepareTabSwitcherView();
        }
        mStartSurfaceMediator.show(animate);
    }

    @Override
    public void hide(boolean animate) {
        hideTabSwitcherView(false);
        onHide();
    }

    @Override
    public void addHeaderOffsetChangeListener(
            AppBarLayout.OnOffsetChangedListener onOffsetChangedListener) {
        // TODO (crbug.com/1113852): Add a header offset change listener for incognito homepage.
        if (mView != null) {
            mView.addHeaderOffsetChangeListener(onOffsetChangedListener);
        }
    }

    @Override
    public void removeHeaderOffsetChangeListener(
            AppBarLayout.OnOffsetChangedListener onOffsetChangedListener) {
        if (mView != null) {
            mView.removeHeaderOffsetChangeListener(onOffsetChangedListener);
        }
    }

    @Override
    public void addStateChangeObserver(StateObserver observer) {
        mStartSurfaceMediator.addStateChangeObserver(observer);
    }

    @Override
    public void removeStateChangeObserver(StateObserver observer) {
        mStartSurfaceMediator.removeStateChangeObserver(observer);
    }

    @Override
    public void setOnTabSelectingListener(StartSurface.OnTabSelectingListener listener) {
        mStartSurfaceMediator.setOnTabSelectingListener(listener);
        if (mTabSwitcherModule != null) {
            mTabSwitcherModule.setOnTabSelectingListener(mStartSurfaceMediator);
        }
    }

    @Override
    public void initWithNative() {
        if (mIsInitializedWithNative) return;

        mIsInitializedWithNative = true;
        if (mIsStartSurfaceEnabled) {
            ViewGroup parentView = mView.getBodyViewContainer();
            mExploreSurfaceCoordinatorFactory =
                    new ExploreSurfaceCoordinatorFactory(
                            mActivity,
                            parentView,
                            mPropertyModel,
                            mBottomSheetController,
                            mParentTabSupplier,
                            new ScrollableContainerDelegateImpl(),
                            mSnackbarManager,
                            mShareDelegateSupplier,
                            mWindowAndroid,
                            mJankTracker,
                            mTabModelSelector,
                            mToolbarSupplier,
                            mConstructedTimeNs,
                            mSwipeRefreshLayout,
                            mTabStripHeightSupplier);
        }
        mStartSurfaceMediator.initWithNative(
                mIsStartSurfaceEnabled ? mOmniboxStubSupplier.get() : null,
                mExploreSurfaceCoordinatorFactory,
                UserPrefs.get(ProfileManager.getLastUsedRegularProfile()));

        if (mIsInitPending) {
            initialize();
        }
    }

    public StartSurfaceMediator getMediatorForTesting() {
        return mStartSurfaceMediator;
    }

    @Override
    public void hideTabSwitcherView(boolean animate) {
        mStartSurfaceMediator.hideTabSwitcherView(animate);
    }

    @Override
    public void setLaunchOrigin(int launchOrigin) {
        mStartSurfaceMediator.setLaunchOrigin(launchOrigin);
    }

    @Override
    public void resetScrollPosition() {
        mStartSurfaceMediator.resetScrollPosition();
    }

    @Override
    public boolean onBackPressed() {
        return mStartSurfaceMediator.onBackPressed();
    }

    @Override
    public boolean isHomepageShown() {
        return mStartSurfaceMediator.isHomepageShown();
    }

    @Override
    public TabSwitcher.TabListDelegate getSingleTabListDelegate() {
        return mIsStartSurfaceEnabled ? mTabSwitcherModule.getTabListDelegate() : null;
    }

    @Override
    public Supplier<Boolean> getTabGridDialogVisibilitySupplier() {
        assert mTabSwitcherModule != null;
        return () -> mTabSwitcherModule.getTabGridDialogVisibilitySupplier() != null;
    }

    @Override
    public void onOverviewShownAtLaunch(
            boolean isOverviewShownOnStartup, long activityCreationTimeMs) {
        if (isOverviewShownOnStartup) {
            mStartSurfaceMediator.onOverviewShownAtLaunch(activityCreationTimeMs);
        }
        if (ReturnToChromeUtil.isStartSurfaceEnabled(mActivity)) {
            if (isOverviewShownOnStartup) {
                ReturnToChromeUtil.recordHistogramsWhenOverviewIsShownAtLaunch();
            }
            Log.i(TAG, "Recorded %s = %b", START_SHOWN_AT_STARTUP_UMA, isOverviewShownOnStartup);
            RecordHistogram.recordBooleanHistogram(
                    START_SHOWN_AT_STARTUP_UMA, isOverviewShownOnStartup);

            // The segmentation results should only be cached when Start is enabled, for example,
            // not on tablet.
            if (StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_USE_MODEL.getValue()) {
                ReturnToChromeUtil.cacheReturnTimeFromSegmentation();
            }
        }
    }

    @Override
    public @Nullable TasksView getPrimarySurfaceView() {
        return mView;
    }

    @Override
    public ObservableSupplier<TabSwitcherCustomViewManager>
            getTabSwitcherCustomViewManagerSupplier() {
        return mTabSwitcherCustomViewManagerSupplier;
    }

    public boolean isMVTilesCleanedUpForTesting() {
        if (mMostVisitedCoordinator != null) {
            return mMostVisitedCoordinator.isMVTilesCleanedUp();
        }
        return false;
    }

    public boolean isMVTilesInitializedForTesting() {
        return mIsMVTilesInitialized;
    }

    public TileGroupDelegateImpl getTileGroupDelegateForTesting() {
        return mTileGroupDelegate;
    }

    private void createStartSurface() {
        ArrayList<PropertyKey> allProperties =
                new ArrayList<>(Arrays.asList(TasksSurfaceProperties.ALL_KEYS));
        allProperties.addAll(Arrays.asList(StartSurfaceProperties.ALL_KEYS));
        mPropertyModel = new PropertyModel(allProperties);

        assert mIsStartSurfaceEnabled;

        int tabSwitcherType = TabSwitcherType.NONE;
        if (!mUseMagicSpace) {
            tabSwitcherType = TabSwitcherType.SINGLE;
        }

        if (!mIsSurfacePolishEnabled) {
            mView =
                    (TasksView)
                            LayoutInflater.from(mActivity)
                                    .inflate(R.layout.tasks_view_layout, null);
        } else {
            mView =
                    (TasksView)
                            LayoutInflater.from(mActivity)
                                    .inflate(R.layout.tasks_view_layout_polish, null);
        }
        mView.setId(R.id.primary_tasks_surface_view);
        mView.initialize(
                mActivityLifecycleDispatcher,
                mParentTabSupplier.hasValue() && mParentTabSupplier.get().isIncognito(),
                mWindowAndroid);
        if (tabSwitcherType == TabSwitcherType.SINGLE) {
            // We always pass the parameter isTablet to be false here since StartSurfaceCoordinator
            // is only created on phones.
            mTabSwitcherModule =
                    new SingleTabSwitcherCoordinator(
                            mActivity,
                            mView.getCardTabSwitcherContainer(),
                            null,
                            mTabModelSelector,
                            /* isShownOnNtp= */ false,
                            /* isTablet= */ false,
                            /* isScrollableMvtEnabled= */ true,
                            /* mostRecentTab= */ null,
                            /* singleTabCardClickedCallback= */ null,
                            /* snapshotParentViewRunnable= */ null,
                            mTabContentManager,
                            /* uiConfig= */ null,
                            /* moduleDelegate= */ null);
        }
        View mvTilesContainer = mView.findViewById(R.id.mv_tiles_container);
        mMostVisitedCoordinator =
                new MostVisitedTilesCoordinator(
                        mActivity,
                        mActivityLifecycleDispatcher,
                        mvTilesContainer,
                        mWindowAndroid,
                        TabUiFeatureUtilities.supportInstantStart(
                                DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity),
                                mActivity),
                        /* isScrollableMVTEnabled= */ true,
                        Integer.MAX_VALUE,
                        /* snapshotTileGridChangedRunnable= */ null,
                        /* tileCountChangedRunnable= */ null);

        initializeOffsetChangedListener();
        addHeaderOffsetChangeListener(mOffsetChangedListenerToGenerateScrollEvents);

        mStartSurfaceWithParentViewPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mPropertyModel,
                        new StartSurfaceWithParentViewBinder.ViewHolder(
                                mContainerView, mView, mSwipeRefreshLayout),
                        StartSurfaceWithParentViewBinder::bind);

        mStartSurfacePropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(mPropertyModel, mView, TasksViewBinder::bind);
    }

    // TODO(crbug.com/1047488): This is a temporary solution of the issue crbug.com/1047488, which
    // has not been reproduced locally. The crash is because we can not find ChromeTabbedActivity's
    // ActivityInfo in the ApplicationStatus. However, from the code, ActivityInfo is created in
    // ApplicationStatus during AsyncInitializationActivity.onCreate, which happens before
    // ChromeTabbedActivity.startNativeInitialization where creates the Start surface. So one
    // possible reason is the ChromeTabbedActivity is finishing or destroyed when showing overview.
    private boolean isActivityFinishingOrDestroyed() {
        boolean finishingOrDestroyed =
                mChromeActivityNativeDelegate.isActivityFinishingOrDestroyed()
                        || ApplicationStatus.getStateForActivity(mActivity)
                                == ActivityState.DESTROYED;
        // TODO(crbug.com/1047488): Assert false. Do not do that in this CL to keep it small since
        // Start surface is eanbled in the fieldtrial_testing_config.json, which requires update of
        // the other browser tests.
        return finishingOrDestroyed;
    }

    /** Creates a {@link SwipeRefreshLayout} to do a pull-to-refresh. */
    private void createSwipeRefreshLayout() {
        assert mSwipeRefreshLayout == null;
        mSwipeRefreshLayout = FeedSwipeRefreshLayout.create(mActivity, R.id.toolbar_container);

        // If FeedSwipeRefreshLayout is not created because the feature is not enabled, don't create
        // another layer.
        if (mSwipeRefreshLayout == null) return;

        // SwipeRefreshLayout can only support one direct child. So we have to create a FrameLayout
        // as a container of possible more than one task views.
        mContainerView.addView(mSwipeRefreshLayout);
        FrameLayout directChildHolder = new FrameLayout(mActivity);
        mSwipeRefreshLayout.addView(directChildHolder);
        mSwipeRefreshLayout.setVisibility(View.GONE);
        mContainerView = directChildHolder;
    }

    private int getLogoInSurfaceHeight() {
        Resources resources = mActivity.getResources();

        if (StartSurfaceConfiguration.isLogoPolishEnabled()) {
            return LogoUtils.getLogoTotalHeightForLogoPolish(
                    resources, StartSurfaceConfiguration.getLogoSizeForLogoPolish());
        }

        if (mIsSurfacePolishEnabled
                && StartSurfaceConfiguration.SURFACE_POLISH_MOVE_DOWN_LOGO.getValue()) {
            if (StartSurfaceConfiguration.SURFACE_POLISH_LESS_BRAND_SPACE.getValue()) {
                return LogoUtils.getLogoTotalHeightPolishedShort(resources);
            }

            return LogoUtils.getLogoTotalHeightPolished(resources);
        }

        return getPixelSize(R.dimen.ntp_logo_height)
                + getPixelSize(R.dimen.ntp_logo_margin_top)
                + getPixelSize(R.dimen.ntp_logo_margin_bottom);
    }

    private void initializeOffsetChangedListener() {
        // Scroll-dependeng fake box transition of states (e.g., height) undergoes 3 phases:
        //   Phase 1: Fake box is far from top: Keep original states (e.g., large height). Related
        //     variables are fake*BeforeAnimation.
        //   Phase 2: Fake box just reaches top: Interpolate states (e.g., shrink height). Related
        //     variables are fake*ForAnimation.
        //   Phase 3: Fake box is at top: Match real box states (e.g., small height). Related
        //     variables are real*.
        // For Surface Polish, Phase 2 is more complex:
        //   Phase 2A: Fake box *nearing* top: Interpolate height reduction to reach some fixed
        //     intermediate value (fakeHeightForAnimation). The top border also linear reaches the
        //     final at a slower rate. This phase encroaches upon Phase 1. Related variables are
        //     *BeforeRealAnimation or fake*ForAnimation.
        //   Phase 2B: Fake box reaches top: Same as Phase 2; interpolate all states. Related
        //     variables are fake*ForAnimation.

        int realVerticalMargin = getPixelSize(R.dimen.location_bar_vertical_margin);
        int logoInSurfaceHeight = getLogoInSurfaceHeight();

        // The following |fake*| values mean the values of the fake search box; |real*| values
        // mean the values of the real search box.
        int realHeight = getPixelSize(R.dimen.toolbar_height_no_shadow) - realVerticalMargin * 2;
        int fakeHeightForAnimation = getPixelSize(R.dimen.ntp_search_box_height);
        int fakeHeightBeforeAnimation =
                mIsSurfacePolishEnabled
                        ? getPixelSize(R.dimen.ntp_search_box_height_polish)
                        : fakeHeightForAnimation;
        // The gap between the top of fake box and its final position for Phase 2A to start.
        int heightReducedBeforeRealAnimation = fakeHeightBeforeAnimation - fakeHeightForAnimation;

        int fakeEndPadding =
                mIsSurfacePolishEnabled
                        ? getPixelSize(R.dimen.fake_search_box_end_padding)
                        : getPixelSize(R.dimen.search_box_end_padding);
        // realEndPadding is 0 when surface is not polished;
        int realEndPadding =
                mIsSurfacePolishEnabled
                        ? getPixelSize(R.dimen.location_bar_end_padding)
                                + getPixelSize(R.dimen.location_bar_url_action_offset_polish)
                        : 0;
        int endPaddingDiff = fakeEndPadding - realEndPadding;

        // fakeTranslationX is 0;
        int realTranslationX;
        if (mIsSurfacePolishEnabled) {
            realTranslationX =
                    OmniboxResourceProvider.getFocusedStatusViewLeftSpacing(mActivity)
                            + getPixelSize(R.dimen.status_view_highlight_size)
                            + getPixelSize(
                                    OmniboxFeatures.shouldShowModernizeVisualUpdate(mActivity)
                                            ? R.dimen.location_bar_icon_end_padding_focused_smaller
                                            : R.dimen.location_bar_icon_end_padding_focused)
                            - getPixelSize(R.dimen.fake_search_box_start_padding);
        } else {
            realTranslationX =
                    getPixelSize(R.dimen.location_bar_status_icon_width)
                            + getPixelSize(R.dimen.location_bar_icon_end_padding_focused)
                            + (getPixelSize(R.dimen.fake_search_box_lateral_padding)
                                    - (getPixelSize(R.dimen.search_box_start_padding)));
        }

        int fakeButtonSize =
                mIsSurfacePolishEnabled
                        ? getPixelSize(R.dimen.location_bar_action_icon_width)
                        : getPixelSize(R.dimen.tasks_surface_location_bar_url_button_size);
        int realButtonSize = getPixelSize(R.dimen.location_bar_action_icon_width);

        int fakeLensButtonStartMargin =
                mIsSurfacePolishEnabled
                        ? 0
                        : getPixelSize(R.dimen.tasks_surface_location_bar_url_button_start_margin);
        // realLensButtonStartMargin is 0;

        float fakeSearchTextSize =
                mIsSurfacePolishEnabled
                        ? getTextSizeFromDimen(R.dimen.location_bar_url_text_size_polish)
                        : getTextSizeFromDimen(R.dimen.location_bar_url_text_size);
        float realSearchTextSize = getTextSizeFromDimen(R.dimen.location_bar_url_text_size);

        // Explicitly assign fake search box container height, so it won't resizes in Phase 2* along
        // with its interior. This prevents content underneath from shifting, which can have adverse
        // interaction with scroll from swiping.
        mView.updateFakeSearchBoxContainer(fakeHeightBeforeAnimation);

        mOffsetChangedListenerToGenerateScrollEvents =
                (appBarLayout, verticalOffset) -> {
                    for (ScrollListener scrollListener : mScrollListeners) {
                        scrollListener.onHeaderOffsetChanged(verticalOffset);
                    }

                    int fakeSearchBoxToRealSearchBoxTop =
                            mStartSurfaceMediator.getTopToolbarPlaceholderHeight()
                                    + (mStartSurfaceMediator.isLogoVisible()
                                            ? logoInSurfaceHeight
                                            : 0)
                                    - realVerticalMargin;
                    int scrolledHeight = -verticalOffset;
                    int fakeHeight;
                    if (mIsSurfacePolishEnabled) {
                        // Detect and handle Phase 2A. Otherwise reuse the original flow, but tweak
                        // |fakeHeight| for Phase 2B.
                        int startPointToReduceHeight =
                                fakeSearchBoxToRealSearchBoxTop - heightReducedBeforeRealAnimation;

                        if (scrolledHeight < startPointToReduceHeight) {
                            // Phase 1.
                            fakeHeight = fakeHeightBeforeAnimation;

                        } else if (scrolledHeight < fakeSearchBoxToRealSearchBoxTop) {
                            // Phase 2A: Shrink height at the same rate as scrolling.
                            int reducedHeight = scrolledHeight - startPointToReduceHeight;
                            mView.updateFakeSearchBoxHeight(
                                    fakeHeightBeforeAnimation - reducedHeight);
                            return;

                        } else {
                            // Phase 2B and Phase 3.
                            fakeHeight = fakeHeightForAnimation;
                        }
                    } else {
                        fakeHeight = fakeHeightBeforeAnimation;
                    }

                    int fakeAndRealHeightDiff = fakeHeight - realHeight;
                    // When the fake search box top is scrolled to the search box top, start to
                    // reduce fake search box's height until it's the same as the real search box.
                    int reducedHeight =
                            MathUtils.clamp(
                                    scrolledHeight - fakeSearchBoxToRealSearchBoxTop,
                                    0,
                                    fakeAndRealHeightDiff);
                    float expansionFraction = (float) reducedHeight / fakeAndRealHeightDiff;

                    // This function should be called together with
                    // StartSurfaceToolbarMediator#updateTranslationY, which scroll up the start
                    // surface toolbar together with the header.
                    // Note: the logic below may need to be updated if Start is ever showing in
                    // Incognito mode.
                    mView.updateFakeSearchBox(
                            fakeHeight - reducedHeight,
                            reducedHeight,
                            (int) (endPaddingDiff * (1 - expansionFraction) + realEndPadding),
                            realTranslationX * expansionFraction,
                            (int)
                                    (fakeButtonSize
                                            + (realButtonSize - fakeButtonSize)
                                                    * expansionFraction),
                            (int) (fakeLensButtonStartMargin * (1 - expansionFraction)),
                            fakeSearchTextSize
                                    + (realSearchTextSize - fakeSearchTextSize)
                                            * expansionFraction);

                    if (mIsSurfacePolishEnabled && scrolledHeight > appBarLayout.getHeight()) {
                        ViewUtils.requestLayout(
                                appBarLayout,
                                "StartSurfaceCoordinator#initializeOffsetChangedListener "
                                        + "AppBarLayout.OnOffsetChangedListener");
                    }
                };
    }

    private int getPixelSize(int id) {
        return mActivity.getResources().getDimensionPixelSize(id);
    }

    /**
     * Gets the text size based on a dimension resource. The return value is in SP.
     * @param id The resource ID of the dimension value.
     */
    private float getTextSizeFromDimen(int id) {
        TypedValue typedValue = new TypedValue();
        Resources resources = mActivity.getResources();
        resources.getValue(id, typedValue, true);

        if (typedValue.type == TypedValue.TYPE_DIMENSION
                && (typedValue.data & TypedValue.COMPLEX_UNIT_MASK) == TypedValue.COMPLEX_UNIT_SP) {
            return TypedValue.complexToFloat(typedValue.data);
        }

        return -1;
    }

    public void initializeMVTiles() {
        if (!LibraryLoader.getInstance().isInitialized()
                || mIsMVTilesInitialized
                || mMostVisitedCoordinator == null) {
            return;
        }

        Profile profile = ProfileManager.getLastUsedRegularProfile();
        MostVisitedTileNavigationDelegate navigationDelegate =
                new MostVisitedTileNavigationDelegate(mActivity, profile, mParentTabSupplier);
        mSuggestionsUiDelegate =
                new MostVisitedSuggestionsUiDelegate(
                        mView, navigationDelegate, profile, mSnackbarManager);
        mTileGroupDelegate =
                new TileGroupDelegateImpl(
                        mActivity,
                        profile,
                        navigationDelegate,
                        mSnackbarManager,
                        BrowserUiUtils.HostSurface.START_SURFACE);

        mMostVisitedCoordinator.initWithNative(
                mSuggestionsUiDelegate, mTileGroupDelegate, enabled -> {});
        mIsMVTilesInitialized = true;
    }

    /**
     * Called to send the search query and params to omnibox to kick off a search.
     * @param queryText Text of the search query to perform.
     * @param searchParams A list of params to sent along with the search query.
     */
    void performSearchQuery(String queryText, List<String> searchParams) {
        mStartSurfaceMediator.performSearchQuery(queryText, searchParams);
    }

    FeedSwipeRefreshLayout getFeedSwipeRefreshLayoutForTesting() {
        return mSwipeRefreshLayout;
    }

    TasksView getViewForTesting() {
        return mView;
    }
}
