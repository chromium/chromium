// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.app.Activity;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
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
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.feed.FeedSwipeRefreshLayout;
import org.chromium.chrome.browser.feed.ScrollListener;
import org.chromium.chrome.browser.feed.ScrollableContainerDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.init.ChromeActivityNativeDelegate;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.SearchEngineLogoUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.OriginalProfileSupplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.query_tiles.QueryTileSection;
import org.chromium.chrome.browser.query_tiles.QueryTileUtils;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.crow.CrowButtonDelegate;
import org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesCoordinator;
import org.chromium.chrome.browser.suggestions.tile.TileGroupDelegateImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementDelegate.TabSwitcherType;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherCustomViewManager;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.toolbar.top.Toolbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.features.tasks.MostVisitedTileNavigationDelegate;
import org.chromium.chrome.features.tasks.SingleTabSwitcherCoordinator;
import org.chromium.chrome.features.tasks.TasksSurface;
import org.chromium.chrome.features.tasks.TasksSurfaceCoordinator;
import org.chromium.chrome.features.tasks.TasksSurfaceProperties;
import org.chromium.chrome.features.tasks.TasksView;
import org.chromium.chrome.features.tasks.TasksViewBinder;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Root coordinator that is responsible for showing start surfaces, like a grid of Tabs, explore
 * surface and the bottom bar to switch between them.
 */
public class StartSurfaceCoordinator implements StartSurface {
    private final Activity mActivity;
    private final ScrimCoordinator mScrimCoordinator;
    private final StartSurfaceMediator mStartSurfaceMediator;
    private final boolean mIsStartSurfaceEnabled;
    private final BottomSheetController mBottomSheetController;
    private final Supplier<Tab> mParentTabSupplier;
    private final WindowAndroid mWindowAndroid;
    private ViewGroup mContainerView;
    private final Supplier<DynamicResourceLoader> mDynamicResourceLoaderSupplier;
    private final TabModelSelector mTabModelSelector;
    private final BrowserControlsManager mBrowserControlsManager;
    private final SnackbarManager mSnackbarManager;
    private final Supplier<ShareDelegate> mShareDelegateSupplier;
    private final Supplier<OmniboxStub> mOmniboxStubSupplier;
    private final TabContentManager mTabContentManager;
    private final ModalDialogManager mModalDialogManager;
    private final ChromeActivityNativeDelegate mChromeActivityNativeDelegate;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final TabCreatorManager mTabCreatorManager;
    private final MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    private final MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    private final Supplier<Toolbar> mToolbarSupplier;
    // TODO(crbug.com/1315676): Directly return the supplier from {@link TabSwitcherCoordinator}.
    private final ObservableSupplierImpl<TabSwitcherCustomViewManager>
            mTabSwitcherCustomViewManagerSupplier;
    private final CrowButtonDelegate mCrowButtonDelegate;
    private final OneshotSupplier<IncognitoReauthController> mIncognitoReauthControllerSupplier;

    @VisibleForTesting
    static final String START_SHOWN_AT_STARTUP_UMA = "Startup.Android.StartSurfaceShownAtStartup";
    private static final String TAG = "StartSurface";
    private static final int MAX_TILE_ROWS_FOR_GRID_MVT = 2;

    // Non-null in SurfaceMode.SINGLE_PANE mode.
    @Nullable
    private TasksSurface mTasksSurface;

    // Non-null in SurfaceMode.SINGLE_PANE mode.
    @Nullable
    private PropertyModelChangeProcessor mTasksSurfacePropertyModelChangeProcessor;

    private PropertyModelChangeProcessor mStartSurfaceWithParentViewPropertyModelChangeProcessor;
    private PropertyModelChangeProcessor mStartSurfacePropertyModelChangeProcessor;

    // TODO(crbug.com/1315676): Remove this once the start surface refactoring is done, since the
    // secondary tasks surface will go away.
    // Non-null in SurfaceMode.SINGLE_PANE mode to show more tabs.
    @Nullable
    private TasksSurface mSecondaryTasksSurface;

    // Non-null in SurfaceMode.SINGLE_PANE mode to show more tabs.
    @Nullable
    private PropertyModelChangeProcessor mSecondaryTasksSurfacePropertyModelChangeProcessor;

    // Non-null in SurfaceMode.NO_START_SURFACE to show the tabs.
    @Nullable
    private TabSwitcher mGridTabSwitcher;

    // Non-null in SurfaceMode.SINGLE_PANE modes.
    @Nullable
    private ExploreSurfaceCoordinatorFactory mExploreSurfaceCoordinatorFactory;

    // Non-null in SurfaceMode.SINGLE_PANE modes.
    // TODO(crbug.com/982018): Get rid of this reference since the mediator keeps a reference to it.
    @Nullable
    private PropertyModel mPropertyModel;

    // Used to remember TabSwitcher.OnTabSelectingListener in SurfaceMode.SINGLE_PANE mode for more
    // tabs surface if necessary.
    @Nullable
    private TabSwitcher.OnTabSelectingListener mOnTabSelectingListener;

    // Whether the {@link initWithNative()} is called.
    private boolean mIsInitializedWithNative;

    // A flag of whether there is a pending call to {@link initialize()} but waiting for native's
    // initialization.
    private boolean mIsInitPending;

    private boolean mIsSecondaryTaskInitPending;

    // Listeners used by the contained surfaces (e.g., Explore) to listen to the scroll changes on
    // the main scrollable container of the start surface.
    private final ObserverList<ScrollListener> mScrollListeners =
            new ObserverList<ScrollListener>();

    // Time at which constructor started to run. Used for feed reliability logging.
    private final long mConstructedTimeNs;

    private final boolean mIsStartSurfaceRefactorEnabled;

    @Nullable
    private AppBarLayout.OnOffsetChangedListener mOffsetChangedListenerToGenerateScrollEvents;

    // For pull-to-refresh.
    @Nullable
    private FeedSwipeRefreshLayout mSwipeRefreshLayout;

    // The single or carousel Tab switcher module on the Start surface.
    // None-null when the Start surface refactoring is enabled.
    @Nullable
    private TabSwitcher mTabSwitcherModule;
    // The view of Start surface layout.
    // None-null when the Start surface refactoring is enabled.
    @Nullable
    private TasksView mView;
    private MostVisitedTilesCoordinator mMostVisitedCoordinator;
    private MostVisitedSuggestionsUiDelegate mSuggestionsUiDelegate;
    private TileGroupDelegateImpl mTileGroupDelegate;
    private OneshotSupplier<Profile> mQueryTileProfileSupplier;
    private QueryTileSection mQueryTileSection;
    private boolean mIsMVTilesInitialized;

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
            // Always return a zero dummy value because the offset is directly provided
            // by the observer.
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
     * @param scrimCoordinator The coordinator for the scrim widget.
     * @param sheetController Controls the bottom sheet.
     * @param startSurfaceOneshotSupplier Supplies the start surface.
     * @param parentTabSupplier Supplies the current parent {@link Tab}.
     * @param hadWarmStart Whether the application had a warm start.
     * @param windowAndroid The current {@link WindowAndroid}.
     * @param containerView The container {@link ViewGroup} for this ui, also the root view for
     *         StartSurface.
     * @param dynamicResourceLoaderSupplier Supplies the current {@link DynamicResourceLoader}.
     * @param tabModelSelector The current {@link TabModelSelector}.
     * @param browserControlsManager Manages the browser controls.
     * @param snackbarManager Manages the snackbar.
     * @param shareDelegateSupplier Supplies the {@link ShareDelegate}.
     * @param omniboxStubSupplier Supplies the {@link OmniboxStub}.
     * @param tabContentManager Manages the tab content.
     * @param modalDialogManager Manages modal dialogs.
     * @param chromeActivityNativeDelegate An activity delegate to handle native initialization.
     * @param activityLifecycleDispatcher Allows observation of the activity lifecycle.
     * @param tabCreatorManager Manages {@link Tab} creation.
     * @param menuOrKeyboardActionController allows access to menu or keyboard actions.
     * @param multiWindowModeStateDispatcher Gives access to the multi window mode state.
     * @param jankTracker Measures jank while feed or tab switcher are visible.
     * @param toolbarSupplier Supplies the {@link Toolbar}.
     * @param crowButtonDelegate The {@link CrowButtonDelegate} to handle Crow click events.
     * @param backPressManager {@link BackPressManager} to handle back press.
     * @param incognitoReauthControllerSupplier {@link OneshotSupplier<IncognitoReauthController>}
     *         to detect pending re-auth when tab switcher is shown.
     * @param tabSwitcherClickHandler The {@link OnClickListener} for the tab switcher button.
     */
    public StartSurfaceCoordinator(@NonNull Activity activity,
            @NonNull ScrimCoordinator scrimCoordinator,
            @NonNull BottomSheetController sheetController,
            @NonNull OneshotSupplierImpl<StartSurface> startSurfaceOneshotSupplier,
            @NonNull Supplier<Tab> parentTabSupplier, boolean hadWarmStart,
            @NonNull WindowAndroid windowAndroid, @NonNull ViewGroup containerView,
            @NonNull Supplier<DynamicResourceLoader> dynamicResourceLoaderSupplier,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull BrowserControlsManager browserControlsManager,
            @NonNull SnackbarManager snackbarManager,
            @NonNull Supplier<ShareDelegate> shareDelegateSupplier,
            @NonNull Supplier<OmniboxStub> omniboxStubSupplier,
            @NonNull TabContentManager tabContentManager,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull ChromeActivityNativeDelegate chromeActivityNativeDelegate,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull MenuOrKeyboardActionController menuOrKeyboardActionController,
            @NonNull MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            @NonNull JankTracker jankTracker, @NonNull Supplier<Toolbar> toolbarSupplier,
            @NonNull CrowButtonDelegate crowButtonDelegate, BackPressManager backPressManager,
            @NonNull OneshotSupplier<IncognitoReauthController> incognitoReauthControllerSupplier,
            @NonNull OnClickListener tabSwitcherClickHandler) {
        mConstructedTimeNs = SystemClock.elapsedRealtimeNanos();
        mActivity = activity;
        mScrimCoordinator = scrimCoordinator;
        mIsStartSurfaceEnabled = ReturnToChromeUtil.isStartSurfaceEnabled(mActivity);
        mBottomSheetController = sheetController;
        mParentTabSupplier = parentTabSupplier;
        mWindowAndroid = windowAndroid;
        mContainerView = containerView;
        mDynamicResourceLoaderSupplier = dynamicResourceLoaderSupplier;
        mTabModelSelector = tabModelSelector;
        mBrowserControlsManager = browserControlsManager;
        mSnackbarManager = snackbarManager;
        mShareDelegateSupplier = shareDelegateSupplier;
        mOmniboxStubSupplier = omniboxStubSupplier;
        mTabContentManager = tabContentManager;
        mModalDialogManager = modalDialogManager;
        mChromeActivityNativeDelegate = chromeActivityNativeDelegate;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mTabCreatorManager = tabCreatorManager;
        mMenuOrKeyboardActionController = menuOrKeyboardActionController;
        mMultiWindowModeStateDispatcher = multiWindowModeStateDispatcher;
        mToolbarSupplier = toolbarSupplier;
        mCrowButtonDelegate = crowButtonDelegate;
        mIncognitoReauthControllerSupplier = incognitoReauthControllerSupplier;

        mTabSwitcherCustomViewManagerSupplier = new ObservableSupplierImpl<>();
        boolean excludeQueryTiles = !mIsStartSurfaceEnabled
                || !ChromeFeatureList.sQueryTilesOnStart.isEnabled();
        mIsStartSurfaceRefactorEnabled =
                ReturnToChromeUtil.isStartSurfaceRefactorEnabled(mActivity);
        TabSwitcher.Controller controller = null;
        Runnable initializeMVTilesRunnable = null;
        View logoContainerView = null;
        ViewGroup feedPlaceholderParentView = null;
        if (!mIsStartSurfaceEnabled && !mIsStartSurfaceRefactorEnabled) {
            // Create Tab switcher directly to save one layer in the view hierarchy.
            mGridTabSwitcher = TabManagementModuleProvider.getDelegate().createGridTabSwitcher(
                    activity, activityLifecycleDispatcher, tabModelSelector, tabContentManager,
                    browserControlsManager, tabCreatorManager, menuOrKeyboardActionController,
                    containerView, shareDelegateSupplier, multiWindowModeStateDispatcher,
                    scrimCoordinator, /* rootView= */ containerView, dynamicResourceLoaderSupplier,
                    snackbarManager, modalDialogManager, incognitoReauthControllerSupplier,
                    backPressManager);
            mTabSwitcherCustomViewManagerSupplier.set(
                    mGridTabSwitcher.getTabSwitcherCustomViewManager());
            controller = mGridTabSwitcher.getController();
        } else if (!mIsStartSurfaceRefactorEnabled) {
            assert mIsStartSurfaceEnabled;

            // createSwipeRefreshLayout has to be called before creating any surface.
            createSwipeRefreshLayout();
            createAndSetStartSurface(excludeQueryTiles);
            controller = mTasksSurface.getController();
            initializeMVTilesRunnable = mTasksSurface::initializeMVTiles;
            logoContainerView = mTasksSurface.getView().findViewById(R.id.logo_container);
            feedPlaceholderParentView = mTasksSurface.getBodyViewContainer();
        } else {
            assert mIsStartSurfaceEnabled && mIsStartSurfaceRefactorEnabled;

            // createSwipeRefreshLayout has to be called before creating any surface.
            createSwipeRefreshLayout();
            createStartSurfaceWithoutTasksSurface(excludeQueryTiles);
            initializeMVTilesRunnable = this::initializeMVTiles;
            logoContainerView = mView.findViewById(R.id.logo_container);
            feedPlaceholderParentView = mView.findViewById(R.id.tasks_surface_body);
        }
        mStartSurfaceMediator = new StartSurfaceMediator(controller, containerView,
                mTabSwitcherModule, mTabModelSelector, mPropertyModel,
                mTasksSurface != null ? this::initializeSecondaryTasksSurface : null,
                mIsStartSurfaceEnabled, mActivity, mBrowserControlsManager,
                this::isActivityFinishingOrDestroyed, excludeQueryTiles,
                startSurfaceOneshotSupplier, hadWarmStart, jankTracker, initializeMVTilesRunnable,
                mParentTabSupplier, logoContainerView,
                mGridTabSwitcher == null ? backPressManager : null, feedPlaceholderParentView,
                mActivityLifecycleDispatcher, tabSwitcherClickHandler);

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
        if (mTasksSurface != null) {
            mTasksSurface.initialize();
        }
    }

    @Override
    public void destroy() {
        onHide();
        if (mOffsetChangedListenerToGenerateScrollEvents != null) {
            removeHeaderOffsetChangeListener(mOffsetChangedListenerToGenerateScrollEvents);
            mOffsetChangedListenerToGenerateScrollEvents = null;
        }
        if (mStartSurfaceMediator != null) {
            mStartSurfaceMediator.destroy();
        }
    }

    @Override
    public void onHide() {
        if (mIsInitializedWithNative) {
            if (mTasksSurface != null) {
                mStartSurfaceMediator.mayRecordHomepageSessionEnd();
                mTasksSurface.onHide();
            }
            if (mSecondaryTasksSurface != null) {
                assert !mIsStartSurfaceRefactorEnabled;
                mSecondaryTasksSurface.onHide();
            }
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
        getCarouselOrSingleTabListDelegate().prepareTabSwitcherView();
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
        if (mTasksSurface != null) {
            mTasksSurface.addHeaderOffsetChangeListener(onOffsetChangedListener);
        } else if (mView != null) {
            mView.addHeaderOffsetChangeListener(onOffsetChangedListener);
        }
    }

    @Override
    public void removeHeaderOffsetChangeListener(
            AppBarLayout.OnOffsetChangedListener onOffsetChangedListener) {
        if (mTasksSurface != null) {
            mTasksSurface.removeHeaderOffsetChangeListener(onOffsetChangedListener);
        } else if (mView != null) {
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
        if (mTasksSurface != null) {
            mTasksSurface.setOnTabSelectingListener(mStartSurfaceMediator);
        } else if (mGridTabSwitcher != null) {
            mGridTabSwitcher.setOnTabSelectingListener(mStartSurfaceMediator);
        } else {
            mTabSwitcherModule.setOnTabSelectingListener(mStartSurfaceMediator);
        }

        if (mIsStartSurfaceRefactorEnabled) return;

        // Set OnTabSelectingListener to the more tabs tasks surface as well if it has been
        // instantiated, otherwise remember it for the future instantiation.
        if (mIsStartSurfaceEnabled) {
            if (mSecondaryTasksSurface == null) {
                mOnTabSelectingListener = mStartSurfaceMediator;
            } else {
                mSecondaryTasksSurface.setOnTabSelectingListener(mStartSurfaceMediator);
            }
        }
    }

    @Override
    public void initWithNative() {
        if (mIsInitializedWithNative) return;

        mIsInitializedWithNative = true;
        if (mIsStartSurfaceEnabled) {
            ViewGroup parentView = mView != null ? mView.getBodyViewContainer()
                                                 : mTasksSurface.getBodyViewContainer();
            mExploreSurfaceCoordinatorFactory = new ExploreSurfaceCoordinatorFactory(mActivity,
                    parentView, mPropertyModel, mBottomSheetController, mParentTabSupplier,
                    new ScrollableContainerDelegateImpl(), mSnackbarManager, mShareDelegateSupplier,
                    mWindowAndroid, mTabModelSelector, mToolbarSupplier, mConstructedTimeNs,
                    mSwipeRefreshLayout, mCrowButtonDelegate);
        }
        mStartSurfaceMediator.initWithNative(
                mIsStartSurfaceEnabled ? mOmniboxStubSupplier.get() : null,
                mExploreSurfaceCoordinatorFactory,
                UserPrefs.get(Profile.getLastUsedRegularProfile()), mSnackbarManager);

        if (mGridTabSwitcher != null) {
            mGridTabSwitcher.initWithNative();
        }
        if (mTasksSurface != null) {
            mTasksSurface.onFinishNativeInitialization(mActivity, mOmniboxStubSupplier.get(),
                    mStartSurfaceMediator.getFeedReliabilityLogger());
        }

        if (mIsInitPending) {
            initialize();
        }

        if (mIsStartSurfaceRefactorEnabled) return;

        if (mIsSecondaryTaskInitPending) {
            mIsSecondaryTaskInitPending = false;
            mSecondaryTasksSurface.onFinishNativeInitialization(
                    mActivity, mOmniboxStubSupplier.get(), /*feedReliabilityLogger=*/null);
            mSecondaryTasksSurface.initialize();
        }
    }

    @VisibleForTesting
    public StartSurfaceMediator getMediatorForTesting() {
        return mStartSurfaceMediator;
    }

    @Override
    public void addTabSwitcherViewObserver(TabSwitcherViewObserver observer) {
        mStartSurfaceMediator.addTabSwitcherViewObserver(observer);
    }

    @Override
    public void removeTabSwitcherViewObserver(TabSwitcherViewObserver listener) {
        mStartSurfaceMediator.removeTabSwitcherViewObserver(listener);
    }

    @Override
    public void hideTabSwitcherView(boolean animate) {
        mStartSurfaceMediator.hideTabSwitcherView(animate);
    }

    @Override
    public void beforeShowTabSwitcherView() {
        // TODO(crbug/1386265): This is a temporary workaround to ensure the layout can run
        // offscreen while invisible for animation purposes. We should refactor this so that the
        // visibility is controlled independently of IS_SHOWING_OVERVIEW. The final state is
        // View.VISIBLE after the animation so we don't need to restore the state after the
        // animation changes.
        if (mSwipeRefreshLayout != null) {
            mSwipeRefreshLayout.setVisibility(View.VISIBLE);
        }
    }

    @Override
    public void beforeHideTabSwitcherView() {
        mStartSurfaceMediator.beforeHideTabSwitcherView();
    }

    @Override
    public void showOverview(boolean animate) {
        mStartSurfaceMediator.showOverview(animate);
    }

    @Override
    public void setStartSurfaceState(int state, int launchOrigin) {
        mStartSurfaceMediator.setStartSurfaceState(state, launchOrigin);
    }

    @Override
    public void setStartSurfaceState(int state) {
        mStartSurfaceMediator.setStartSurfaceState(state);
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
        if (mGridTabSwitcher != null) {
            return mGridTabSwitcher.onBackPressed();
        }
        return mStartSurfaceMediator.onBackPressed();
    }

    @Override
    public int getStartSurfaceState() {
        return mStartSurfaceMediator.getStartSurfaceState();
    }

    @Override
    public int getPreviousStartSurfaceState() {
        return mStartSurfaceMediator.getPreviousStartSurfaceState();
    }

    @Override
    public ViewGroup getTabSwitcherContainer() {
        return mStartSurfaceMediator.getTabSwitcherContainer();
    }

    @Override
    public void setSnackbarParentView(ViewGroup parentView) {
        mStartSurfaceMediator.setSnackbarParentView(parentView);
    }

    @Override
    public boolean isShowingStartSurfaceHomepage() {
        return mStartSurfaceMediator.isShowingStartSurfaceHomepage();
    }

    @Override
    public boolean isHomepageShown() {
        return mStartSurfaceMediator.isHomepageShown();
    }

    @Override
    public TabSwitcher.TabListDelegate getGridTabListDelegate() {
        if (mIsStartSurfaceEnabled) {
            if (mIsStartSurfaceRefactorEnabled) {
                // Unreachable.
                return null;
            }
            if (mSecondaryTasksSurface == null) {
                mStartSurfaceMediator.setSecondaryTasksSurfaceController(
                        initializeSecondaryTasksSurface());
            }
            return mSecondaryTasksSurface.getTabListDelegate();
        } else {
            return mGridTabSwitcher.getTabListDelegate();
        }
    }

    @Override
    public TabSwitcher.TabListDelegate getCarouselOrSingleTabListDelegate() {
        if (mIsStartSurfaceEnabled) {
            if (mIsStartSurfaceRefactorEnabled) {
                assert mTabSwitcherModule != null;
                return mTabSwitcherModule.getTabListDelegate();
            } else {
                assert mTasksSurface != null;
                return mTasksSurface.getTabListDelegate();
            }
        } else {
            return null;
        }
    }

    @Override
    public Supplier<Boolean> getTabGridDialogVisibilitySupplier() {
        // If TabSwitcher has been created directly, use the TabGridDialogVisibilitySupplier from
        // TabSwitcher.
        if (mGridTabSwitcher != null) {
            return mGridTabSwitcher.getTabGridDialogVisibilitySupplier();
        } else if (mTabSwitcherModule != null) {
            return () -> mTabSwitcherModule.getTabGridDialogVisibilitySupplier() != null;
        }
        return () -> {
            // Return true if either mTasksSurface or mSecondaryTasksSurface has a visible dialog.
            assert mTasksSurface != null;
            if (mTasksSurface.getTabGridDialogVisibilitySupplier() != null) {
                if (mTasksSurface.getTabGridDialogVisibilitySupplier().get()) return true;
            }
            if (mSecondaryTasksSurface != null
                    && mSecondaryTasksSurface.getTabGridDialogVisibilitySupplier() != null) {
                assert !mIsStartSurfaceRefactorEnabled;
                if (mSecondaryTasksSurface.getTabGridDialogVisibilitySupplier().get()) return true;
            }
            return false;
        };
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
            if (ChromeFeatureList.sStartSurfaceReturnTime.isEnabled()) {
                ReturnToChromeUtil.cacheReturnTimeFromSegmentation();
            }
            if (!TextUtils.isEmpty(StartSurfaceConfiguration.BEHAVIOURAL_TARGETING.getValue())) {
                ReturnToChromeUtil.cacheSegmentationResult();
            }
        }
    }

    @Override
    @Nullable
    public TasksView getPrimarySurfaceView() {
        if (mTasksSurface != null) {
            return (TasksView) mTasksSurface.getView();
        }
        return mView;
    }

    @Override
    public ObservableSupplier<TabSwitcherCustomViewManager>
    getTabSwitcherCustomViewManagerSupplier() {
        return mTabSwitcherCustomViewManagerSupplier;
    }

    /**
     * Create the {@link TasksSurface}
     * @param activity The {@link Activity} that creates this surface.
     * @param scrimCoordinator The {@link ScrimCoordinator} that controls scrim view.
     * @param propertyModel The {@link PropertyModel} contains the {@link TasksSurfaceProperties}
     *         to communicate with this surface.
     * @param tabSwitcherType The type of the tab switcher to show.
     * @param parentTabSupplier {@link Supplier} to provide parent tab for the
     *         TasksSurface.
     * @param hasMVTiles whether has MV tiles on the surface.
     * @param windowAndroid An instance of a {@link WindowAndroid}.
     * @param activityLifecycleDispatcher Allows observation of the activity lifecycle.
     * @param tabModelSelector Gives access to the current set of {@TabModel}.
     * @param snackbarManager Manages the display of snackbars.
     * @param dynamicResourceLoaderSupplier Supplies the current {@link DynamicResourceLoader}.
     * @param tabContentManager Gives access to the tab content.
     * @param modalDialogManager Manages the display of modal dialogs.
     * @param browserControlsStateProvider Gives access to the state of the browser controls.
     * @param tabCreatorManager Manages creation of tabs.
     * @param menuOrKeyboardActionController allows access to menu or keyboard actions.
     * @param shareDelegateSupplier Supplies the current {@link ShareDelegate}.
     * @param multiWindowModeStateDispatcher Gives access to the multi window mode state.
     * @param rootView The root view of the app.
     * @param incognitoReauthControllerSupplier {@link OneshotSupplier<IncognitoReauthController>}
     *         to detect pending re-auth when tab switcher is shown.
     * @return The {@link TasksSurface}.
     */
    TasksSurface createTasksSurface(@NonNull Activity activity,
            @NonNull ScrimCoordinator scrimCoordinator, @NonNull PropertyModel propertyModel,
            @TabSwitcherType int tabSwitcherType, @NonNull Supplier<Tab> parentTabSupplier,
            boolean hasMVTiles, boolean hasQueryTiles, @NonNull WindowAndroid windowAndroid,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull TabModelSelector tabModelSelector, @NonNull SnackbarManager snackbarManager,
            @NonNull Supplier<DynamicResourceLoader> dynamicResourceLoaderSupplier,
            @NonNull TabContentManager tabContentManager,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull MenuOrKeyboardActionController menuOrKeyboardActionController,
            @NonNull Supplier<ShareDelegate> shareDelegateSupplier,
            @NonNull MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            @NonNull ViewGroup rootView,
            @Nullable OneshotSupplier<IncognitoReauthController>
                    incognitoReauthControllerSupplier) {
        return new TasksSurfaceCoordinator(activity, scrimCoordinator, propertyModel,
                tabSwitcherType, parentTabSupplier, hasMVTiles, hasQueryTiles, windowAndroid,
                activityLifecycleDispatcher, tabModelSelector, snackbarManager,
                dynamicResourceLoaderSupplier, tabContentManager, modalDialogManager,
                browserControlsStateProvider, tabCreatorManager, menuOrKeyboardActionController,
                shareDelegateSupplier, multiWindowModeStateDispatcher, rootView,
                incognitoReauthControllerSupplier);
    }

    @VisibleForTesting
    public boolean isInitPendingForTesting() {
        return mIsInitPending;
    }

    @VisibleForTesting
    public boolean isInitializedWithNativeForTesting() {
        return mIsInitializedWithNative;
    }

    @VisibleForTesting
    public boolean isSecondaryTaskInitPendingForTesting() {
        return mIsSecondaryTaskInitPending;
    }

    @VisibleForTesting
    public boolean isMVTilesCleanedUpForTesting() {
        if (mTasksSurface != null) {
            return mTasksSurface.isMVTilesCleanedUp();
        }
        if (mMostVisitedCoordinator != null) {
            return mMostVisitedCoordinator.isMVTilesCleanedUp();
        }
        return false;
    }

    @VisibleForTesting
    public boolean isMVTilesInitializedForTesting() {
        if (mTasksSurface != null) {
            return mTasksSurface.isMVTilesInitialized();
        }
        return mIsMVTilesInitialized;
    }

    @VisibleForTesting
    public TileGroupDelegateImpl getTileGroupDelegateForTesting() {
        if (mTasksSurface != null) {
            return mTasksSurface.getTileGroupDelegate();
        }
        return mTileGroupDelegate;
    }

    /**
     * Called only when Start Surface is enabled.
     */
    private void createAndSetStartSurface(boolean excludeQueryTiles) {
        ArrayList<PropertyKey> allProperties =
                new ArrayList<>(Arrays.asList(TasksSurfaceProperties.ALL_KEYS));
        allProperties.addAll(Arrays.asList(StartSurfaceProperties.ALL_KEYS));
        mPropertyModel = new PropertyModel(allProperties);

        assert mIsStartSurfaceEnabled;

        int tabSwitcherType =
                StartSurfaceConfiguration.START_SURFACE_LAST_ACTIVE_TAB_ONLY.getValue()
                ? TabSwitcherType.SINGLE
                : TabSwitcherType.CAROUSEL;
        mTasksSurface = createTasksSurface(mActivity, mScrimCoordinator, mPropertyModel,
                tabSwitcherType, mParentTabSupplier, true, !excludeQueryTiles, mWindowAndroid,
                mActivityLifecycleDispatcher, mTabModelSelector, mSnackbarManager,
                mDynamicResourceLoaderSupplier, mTabContentManager, mModalDialogManager,
                mBrowserControlsManager, mTabCreatorManager, mMenuOrKeyboardActionController,
                mShareDelegateSupplier, mMultiWindowModeStateDispatcher, mContainerView, null);
        mTasksSurface.getView().setId(R.id.primary_tasks_surface_view);
        initializeOffsetChangedListener();
        addHeaderOffsetChangeListener(mOffsetChangedListenerToGenerateScrollEvents);

        mTasksSurfacePropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(mPropertyModel,
                        new StartSurfaceWithParentViewBinder.ViewHolder(
                                mContainerView, mTasksSurface.getView(), mSwipeRefreshLayout),
                        StartSurfaceWithParentViewBinder::bind);
    }

    private void createStartSurfaceWithoutTasksSurface(boolean excludeQueryTiles) {
        ArrayList<PropertyKey> allProperties =
                new ArrayList<>(Arrays.asList(TasksSurfaceProperties.ALL_KEYS));
        allProperties.addAll(Arrays.asList(StartSurfaceProperties.ALL_KEYS));
        mPropertyModel = new PropertyModel(allProperties);

        assert mIsStartSurfaceEnabled;

        int tabSwitcherType =
                StartSurfaceConfiguration.START_SURFACE_LAST_ACTIVE_TAB_ONLY.getValue()
                ? TabSwitcherType.SINGLE
                : TabSwitcherType.CAROUSEL;

        mView = (TasksView) LayoutInflater.from(mActivity).inflate(
                R.layout.tasks_view_layout, null);
        mView.setId(R.id.primary_tasks_surface_view);
        mView.initialize(mActivityLifecycleDispatcher,
                mParentTabSupplier.hasValue() && mParentTabSupplier.get().isIncognito(),
                mWindowAndroid);
        if (tabSwitcherType == TabSwitcherType.CAROUSEL) {
            mTabSwitcherModule =
                    TabManagementModuleProvider.getDelegate().createCarouselTabSwitcher(mActivity,
                            mActivityLifecycleDispatcher, mTabModelSelector, mTabContentManager,
                            mBrowserControlsManager, mTabCreatorManager,
                            mMenuOrKeyboardActionController,
                            mView.getCarouselTabSwitcherContainer(), mShareDelegateSupplier,
                            mMultiWindowModeStateDispatcher, mScrimCoordinator, mView,
                            mDynamicResourceLoaderSupplier, mSnackbarManager, mModalDialogManager);
        } else {
            mTabSwitcherModule = new SingleTabSwitcherCoordinator(
                    mActivity, mView.getCarouselTabSwitcherContainer(), mTabModelSelector);
        }
        boolean isScrollableMVTEnabled =
                !ReturnToChromeUtil.shouldImproveStartWhenFeedIsDisabled(mActivity);
        int maxRowsForGridMVT = getQueryTilesVisibility()
                ? QueryTileSection.getMaxRowsForMostVisitedTiles(mActivity)
                : MAX_TILE_ROWS_FOR_GRID_MVT;
        View mvTilesContainer = mView.findViewById(R.id.mv_tiles_container);
        mMostVisitedCoordinator = new MostVisitedTilesCoordinator(mActivity,
                mActivityLifecycleDispatcher, mvTilesContainer, mWindowAndroid,
                TabUiFeatureUtilities.supportInstantStart(
                        DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity), mActivity),
                isScrollableMVTEnabled,
                isScrollableMVTEnabled ? Integer.MAX_VALUE : maxRowsForGridMVT,
                /*snapshotTileGridChangedRunnable=*/null,
                /*tileCountChangedRunnable=*/null);

        if (!excludeQueryTiles) {
            if (ProfileManager.isInitialized()) {
                initializeQueryTileSection(Profile.getLastUsedRegularProfile());
            } else {
                mQueryTileProfileSupplier = new OriginalProfileSupplier();
                mQueryTileProfileSupplier.onAvailable(this::initializeQueryTileSection);
            }
        } else {
            storeQueryTilesVisibility(false);
        }
        initializeOffsetChangedListener();
        addHeaderOffsetChangeListener(mOffsetChangedListenerToGenerateScrollEvents);

        mStartSurfaceWithParentViewPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(mPropertyModel,
                        new StartSurfaceWithParentViewBinder.ViewHolder(
                                mContainerView, mView, mSwipeRefreshLayout),
                        StartSurfaceWithParentViewBinder::bind);

        mStartSurfacePropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(mPropertyModel, mView, TasksViewBinder::bind);
    }

    // TODO(crbug.com/1315676): Remove this function once the start surface refactoring is done,
    //  since the secondary tasks surface will go away.
    private TabSwitcher.Controller initializeSecondaryTasksSurface() {
        assert mIsStartSurfaceEnabled;
        assert mSecondaryTasksSurface == null;

        PropertyModel propertyModel = new PropertyModel(TasksSurfaceProperties.ALL_KEYS);
        mStartSurfaceMediator.setSecondaryTasksSurfacePropertyModel(propertyModel);
        mSecondaryTasksSurface = createTasksSurface(mActivity, mScrimCoordinator, propertyModel,
                TabSwitcherType.GRID, mParentTabSupplier,
                /* hasMVTiles= */ false, /* hasQueryTiles= */ false, mWindowAndroid,
                mActivityLifecycleDispatcher, mTabModelSelector, mSnackbarManager,
                mDynamicResourceLoaderSupplier, mTabContentManager, mModalDialogManager,
                mBrowserControlsManager, mTabCreatorManager, mMenuOrKeyboardActionController,
                mShareDelegateSupplier, mMultiWindowModeStateDispatcher, mContainerView,
                mIncognitoReauthControllerSupplier);
        if (mIsInitializedWithNative) {
            mSecondaryTasksSurface.onFinishNativeInitialization(
                    mActivity, mOmniboxStubSupplier.get(), /*feedReliabilityLogger=*/null);
            mSecondaryTasksSurface.initialize();
        } else {
            mIsSecondaryTaskInitPending = true;
        }

        mSecondaryTasksSurface.getView().setId(R.id.secondary_tasks_surface_view);
        mSecondaryTasksSurfacePropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(mPropertyModel,
                        new StartSurfaceWithParentViewBinder.ViewHolder(
                                mContainerView, mSecondaryTasksSurface.getView(), null),
                        SecondaryTasksSurfaceViewBinder::bind);
        if (mOnTabSelectingListener != null) {
            mSecondaryTasksSurface.setOnTabSelectingListener(mOnTabSelectingListener);
            mOnTabSelectingListener = null;
        }

        if (!mTabSwitcherCustomViewManagerSupplier.hasValue()) {
            mTabSwitcherCustomViewManagerSupplier.set(
                    mSecondaryTasksSurface.getTabSwitcherCustomViewManager());
        }
        return mSecondaryTasksSurface.getController();
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
                || ApplicationStatus.getStateForActivity(mActivity) == ActivityState.DESTROYED;
        // TODO(crbug.com/1047488): Assert false. Do not do that in this CL to keep it small since
        // Start surface is eanbled in the fieldtrial_testing_config.json, which requires update of
        // the other browser tests.
        return finishingOrDestroyed;
    }

    /**
     * Creates a {@link SwipeRefreshLayout} to do a pull-to-refresh.
     */
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

    private void initializeOffsetChangedListener() {
        int realVerticalMargin = getPixelSize(R.dimen.location_bar_vertical_margin);
        int logoInSurfaceHeight = getPixelSize(R.dimen.ntp_logo_height)
                + getPixelSize(R.dimen.ntp_logo_margin_top)
                + getPixelSize(R.dimen.ntp_logo_margin_bottom);

        // The following |fake*| values mean the values of the fake search box; |real*| values
        // mean the values of the real search box.
        int fakeHeight = getPixelSize(R.dimen.ntp_search_box_height);
        int realHeight = getPixelSize(R.dimen.toolbar_height_no_shadow) - realVerticalMargin * 2;
        int fakeAndRealHeightDiff = fakeHeight - realHeight;

        int fakeEndPadding = getPixelSize(R.dimen.search_box_end_padding);
        // realEndPadding is 0;

        // fakeTranslationX is 0;
        int realTranslationX = getPixelSize(R.dimen.location_bar_status_icon_width)
                + getPixelSize(R.dimen.location_bar_icon_end_padding_focused)
                + (getPixelSize(R.dimen.fake_search_box_lateral_padding)
                        - getPixelSize(R.dimen.search_box_start_padding));

        int fakeButtonSize = getPixelSize(R.dimen.tasks_surface_location_bar_url_button_size);
        int realButtonSize = getPixelSize(R.dimen.location_bar_action_icon_width);

        int fakeLensButtonStartMargin =
                getPixelSize(R.dimen.tasks_surface_location_bar_url_button_start_margin);
        // realLensButtonStartMargin is 0;

        mOffsetChangedListenerToGenerateScrollEvents = (appBarLayout, verticalOffset) -> {
            for (ScrollListener scrollListener : mScrollListeners) {
                scrollListener.onHeaderOffsetChanged(verticalOffset);
            }

            int fakeSearchBoxToRealSearchBoxTop =
                    mStartSurfaceMediator.getTopToolbarPlaceholderHeight()
                    + (mStartSurfaceMediator.isLogoVisible() ? logoInSurfaceHeight : 0)
                    - realVerticalMargin;
            int scrolledHeight = -verticalOffset;
            // When the fake search box top is scrolled to the search box top, start to reduce
            // fake search box's height until it's the same as the real search box.
            int reducedHeight = MathUtils.clamp(
                    scrolledHeight - fakeSearchBoxToRealSearchBoxTop, 0, fakeAndRealHeightDiff);
            float expansionFraction = (float) reducedHeight / fakeAndRealHeightDiff;

            // This function should be called together with
            // StartSurfaceToolbarMediator#updateTranslationY, which scroll up the start surface
            // toolbar together with the header.
            TasksView tasksView =
                    mTasksSurface != null ? (TasksView) mTasksSurface.getView() : mView;
            tasksView.updateFakeSearchBox(fakeHeight - reducedHeight, reducedHeight,
                    (int) (fakeEndPadding * (1 - expansionFraction)),
                    SearchEngineLogoUtils.getInstance().shouldShowSearchEngineLogo(false)
                            ? realTranslationX * expansionFraction
                            : 0,
                    (int) (fakeButtonSize + (realButtonSize - fakeButtonSize) * expansionFraction),
                    (int) (fakeLensButtonStartMargin * (1 - expansionFraction)));
        };
    }

    private int getPixelSize(int id) {
        return mActivity.getResources().getDimensionPixelSize(id);
    }

    public void initializeMVTiles() {
        if (!LibraryLoader.getInstance().isInitialized() || mIsMVTilesInitialized
                || mMostVisitedCoordinator == null) {
            return;
        }

        Profile profile = Profile.getLastUsedRegularProfile();
        MostVisitedTileNavigationDelegate navigationDelegate =
                new MostVisitedTileNavigationDelegate(mActivity, profile, mParentTabSupplier);
        mSuggestionsUiDelegate = new MostVisitedSuggestionsUiDelegate(
                mView, navigationDelegate, profile, mSnackbarManager);
        mTileGroupDelegate = new TileGroupDelegateImpl(mActivity, profile, navigationDelegate,
                mSnackbarManager, BrowserUiUtils.HostSurface.START_SURFACE);

        mMostVisitedCoordinator.initWithNative(
                mSuggestionsUiDelegate, mTileGroupDelegate, enabled -> {});
        mIsMVTilesInitialized = true;
    }

    private void storeQueryTilesVisibility(boolean isShown) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.QUERY_TILES_SHOWN_ON_START_SURFACE, isShown);
    }

    private boolean getQueryTilesVisibility() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.QUERY_TILES_SHOWN_ON_START_SURFACE, false);
    }

    private void initializeQueryTileSection(Profile profile) {
        assert profile != null;
        if (!QueryTileUtils.isQueryTilesEnabledOnStartSurface()) {
            storeQueryTilesVisibility(false);
            return;
        }
        mQueryTileSection = new QueryTileSection(mView.findViewById(R.id.query_tiles_layout),
                profile, query -> performSearchQuery(query.queryText, query.searchParams));
        storeQueryTilesVisibility(true);
        mQueryTileProfileSupplier = null;
    }

    /**
     * Called to send the search query and params to omnibox to kick off a search.
     * @param queryText Text of the search query to perform.
     * @param searchParams A list of params to sent along with the search query.
     */
    void performSearchQuery(String queryText, List<String> searchParams) {
        mStartSurfaceMediator.performSearchQuery(queryText, searchParams);
    }

    @VisibleForTesting
    boolean isSecondaryTasksSurfaceEmptyForTesting() {
        return mSecondaryTasksSurface == null;
    }

    @VisibleForTesting
    FeedSwipeRefreshLayout getFeedSwipeRefreshLayoutForTesting() {
        return mSwipeRefreshLayout;
    }

    TasksSurface getTasksSurfaceForTesting() {
        return mTasksSurface;
    }

    TasksView getViewForTesting() {
        return mView;
    }
}
