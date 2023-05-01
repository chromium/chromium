// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_HEIGHT;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.EXPLORE_SURFACE_COORDINATOR;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_EXPLORE_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SECONDARY_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SHOWING_OVERVIEW;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.RESET_FEED_SURFACE_SCROLL_POSITION;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.TOP_MARGIN;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.FAKE_SEARCH_BOX_CLICK_LISTENER;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.FAKE_SEARCH_BOX_TEXT_WATCHER;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_FAKE_SEARCH_BOX_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_INCOGNITO;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_INCOGNITO_DESCRIPTION_INITIALIZED;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_INCOGNITO_DESCRIPTION_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_LENS_BUTTON_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_SURFACE_BODY_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_TAB_CAROUSEL_TITLE_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_TAB_CAROUSEL_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_VOICE_RECOGNITION_BUTTON_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.LENS_BUTTON_CLICK_LISTENER;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.MORE_TABS_CLICK_LISTENER;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.MV_TILES_CONTAINER_LEFT_RIGHT_MARGIN;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.MV_TILES_CONTAINER_TOP_MARGIN;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.MV_TILES_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.QUERY_TILES_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.RESET_TASK_SURFACE_HEADER_SCROLL_POSITION;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.SINGLE_TAB_TOP_MARGIN;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.TAB_SWITCHER_TITLE_TOP_MARGIN;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.TASKS_SURFACE_BODY_TOP_MARGIN;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.TOP_TOOLBAR_PLACEHOLDER_HEIGHT;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.VOICE_SEARCH_BUTTON_CLICK_LISTENER;

import android.content.Context;
import android.content.res.Resources;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.feed.FeedActionDelegate;
import org.chromium.chrome.browser.feed.FeedReliabilityLogger;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensMetrics;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.logo.LogoCoordinator;
import org.chromium.chrome.browser.ntp.NewTabPageLaunchOrigin;
import org.chromium.chrome.browser.omnibox.OmniboxFocusReason;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.ActiveTabState;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementDelegate.TabSwitcherType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher.Controller;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.features.start_surface.StartSurface.TabSwitcherViewObserver;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.prefs.PrefService;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.ColorUtils;

import java.util.List;

/** The mediator implements the logic to interact with the surfaces and caller. */
class StartSurfaceMediator implements TabSwitcher.TabSwitcherViewObserver, View.OnClickListener,
                                      StartSurface.OnTabSelectingListener, BackPressHandler,
                                      LogoCoordinator.VisibilityObserver,
                                      PauseResumeWithNativeObserver {
    /** Interface to initialize a secondary tasks surface for more tabs. */
    interface SecondaryTasksSurfaceInitializer {
        /**
         * Initialize the secondary tasks surface and return the surface controller, which is
         * TabSwitcher.Controller.
         * @return The {@link TabSwitcher.Controller} of the secondary tasks surface.
         */
        TabSwitcher.Controller initialize();
    }

    /**
     * Interface to check the associated activity state.
     */
    interface ActivityStateChecker {
        /**
         * @return Whether the associated activity is finishing or destroyed.
         */
        boolean isFinishingOrDestroyed();
    }

    private final ObserverList<TabSwitcherViewObserver> mObservers = new ObserverList<>();
    private final TabSwitcher.Controller mController;
    private final TabModelSelector mTabModelSelector;
    @Nullable
    private final PropertyModel mPropertyModel;
    @Nullable
    private final SecondaryTasksSurfaceInitializer mSecondaryTasksSurfaceInitializer;
    private final boolean mIsStartSurfaceEnabled;
    private final ObserverList<StartSurface.StateObserver> mStateObservers = new ObserverList<>();
    private final boolean mHadWarmStart;
    private final boolean mExcludeQueryTiles;
    private final Runnable mInitializeMVTilesRunnable;
    private final Supplier<Tab> mParentTabSupplier;
    private final ObservableSupplierImpl<Boolean> mBackPressChangedSupplier =
            new ObservableSupplierImpl<>();
    private final CallbackController mCallbackController = new CallbackController();
    private final View mLogoContainerView;
    private final boolean mIsFeedGoneImprovementEnabled;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final TabCreatorManager mTabCreatorManager;

    // Boolean histogram used to record whether cached
    // ChromePreferenceKeys.FEED_ARTICLES_LIST_VISIBLE is consistent with
    // Pref.ARTICLES_LIST_VISIBLE.
    @VisibleForTesting
    static final String FEED_VISIBILITY_CONSISTENCY =
            "Startup.Android.CachedFeedVisibilityConsistency";
    private static final int LAST_SHOW_TIME_NOT_SET = -1;
    @Nullable
    private ExploreSurfaceCoordinatorFactory mExploreSurfaceCoordinatorFactory;
    @Nullable
    private TabSwitcher.Controller mSecondaryTasksSurfaceController;
    @Nullable
    private PropertyModel mSecondaryTasksSurfacePropertyModel;
    // Non-null when ReturnToChromeUtils#shouldImproveStartWhenFeedIsDisabled is enabled and
    // homepage is shown.
    @Nullable
    private LogoCoordinator mLogoCoordinator;
    private boolean mIsIncognito;
    @Nullable
    private OmniboxStub mOmniboxStub;
    private Context mContext;
    @Nullable
    UrlFocusChangeListener mUrlFocusChangeListener;
    @StartSurfaceState
    private int mStartSurfaceState;
    @StartSurfaceState
    private int mPreviousStartSurfaceState;
    @NewTabPageLaunchOrigin
    private int mLaunchOrigin;
    @Nullable
    private TabModel mNormalTabModel;
    @Nullable
    private TabModelObserver mNormalTabModelObserver;
    @Nullable
    private TabModelSelectorObserver mTabModelSelectorObserver;
    private BrowserControlsStateProvider mBrowserControlsStateProvider;
    private BrowserControlsStateProvider.Observer mBrowserControlsObserver;
    private ActivityStateChecker mActivityStateChecker;
    private OneshotSupplier<StartSurface> mStartSurfaceSupplier;

    // Only used when the start surface refactoring is enabled. It indicates whether the Start
    // surface homepage is showing when we no longer calculate StartSurfaceState.
    private boolean mIsHomepageShown;
    /**
     * Whether a pending observer needed be added to the normal TabModel after the TabModel is
     * initialized.
     */
    private boolean mPendingObserver;
    /**
     * The value of {@link Pref#ARTICLES_LIST_VISIBLE} on Startup. Getting this value for recording
     * the consistency of {@link ChromePreferenceKeys#FEED_ARTICLES_LIST_VISIBLE} with {@link
     * Pref#ARTICLES_LIST_VISIBLE}.
     */
    private Boolean mFeedVisibilityPrefOnStartUp;
    /**
     * The value of {@link ChromePreferenceKeys#FEED_ARTICLES_LIST_VISIBLE} on Startup. Getting this
     * value for recording the consistency with {@link Pref#ARTICLES_LIST_VISIBLE}.
     */
    @Nullable
    private Boolean mFeedVisibilityInSharedPreferenceOnStartUp;
    private FeedPlaceholderCoordinator mFeedPlaceholderCoordinator;
    private boolean mHasFeedPlaceholderShown;
    private boolean mHideOverviewOnTabSelecting = true;
    private StartSurface.OnTabSelectingListener mOnTabSelectingListener;
    private ViewGroup mTabSwitcherContainer;
    // The single or carousel Tab switcher module on the Start surface.
    // None-null when the Start surface refactoring is enabled.
    @Nullable
    private TabSwitcher mTabSwitcherModule;
    private SnackbarManager mSnackbarManager;
    private boolean mIsNativeInitialized;
    // The timestamp at which the Start Surface was last shown to the user.
    private long mLastShownTimeMs = LAST_SHOW_TIME_NOT_SET;
    private boolean mIsStartSurfaceRefactorEnabled;
    private OnClickListener mTabSwitcherClickHandler;
    private ObservableSupplier<Profile> mProfileSupplier;

    // TODO(crbug.com/1315676): Clean up TabSwitcher#Controller once the start surface refactoring
    // is done.
    StartSurfaceMediator(@Nullable Controller controller, ViewGroup tabSwitcherContainer,
            @Nullable TabSwitcher tabSwitcherModule, TabModelSelector tabModelSelector,
            @Nullable PropertyModel propertyModel,
            @Nullable SecondaryTasksSurfaceInitializer secondaryTasksSurfaceInitializer,
            boolean isStartSurfaceEnabled, Context context,
            BrowserControlsStateProvider browserControlsStateProvider,
            ActivityStateChecker activityStateChecker,
            @Nullable TabCreatorManager tabCreatorManager, boolean excludeQueryTiles,
            OneshotSupplier<StartSurface> startSurfaceSupplier, boolean hadWarmStart,
            Runnable initializeMVTilesRunnable, Supplier<Tab> parentTabSupplier,
            View logoContainerView, @Nullable BackPressManager backPressManager,
            ViewGroup feedPlaceholderParentView,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            OnClickListener tabSwitcherClickHandler, ObservableSupplier<Profile> profileSupplier) {
        mTabSwitcherContainer = tabSwitcherContainer;
        mTabSwitcherModule = tabSwitcherModule;
        mController = mTabSwitcherModule != null ? mTabSwitcherModule.getController() : controller;
        assert mController != null;
        mTabModelSelector = tabModelSelector;
        mPropertyModel = propertyModel;
        mSecondaryTasksSurfaceInitializer = secondaryTasksSurfaceInitializer;
        mIsStartSurfaceEnabled = isStartSurfaceEnabled;
        mContext = context;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mActivityStateChecker = activityStateChecker;
        mTabCreatorManager = tabCreatorManager;
        mExcludeQueryTiles = excludeQueryTiles;
        mStartSurfaceSupplier = startSurfaceSupplier;
        mHadWarmStart = hadWarmStart;
        mLaunchOrigin = NewTabPageLaunchOrigin.UNKNOWN;
        mInitializeMVTilesRunnable = initializeMVTilesRunnable;
        mParentTabSupplier = parentTabSupplier;
        mLogoContainerView = logoContainerView;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);
        // We need to check #shouldImproveStartWhenFeedIsDisabled and save it in the constructor
        // here to keep consistent with toolbar's check. This cannot be moved to other places, since
        // FEED_ARTICLES_LIST_VISIBLE may be changed after feed header is rendered, which then
        // causes inconsistency with toolbar's check.
        mIsFeedGoneImprovementEnabled =
                ReturnToChromeUtil.shouldImproveStartWhenFeedIsDisabled(context);
        mIsStartSurfaceRefactorEnabled = ReturnToChromeUtil.isStartSurfaceRefactorEnabled(context);
        mTabSwitcherClickHandler = tabSwitcherClickHandler;
        mProfileSupplier = profileSupplier;
        mProfileSupplier.addObserver(this::onProfileAvailable);

        if (mPropertyModel != null) {
            assert mIsStartSurfaceEnabled;

            if (mTabSwitcherModule != null) {
                boolean isTabCarousel =
                        mController.getTabSwitcherType() == TabSwitcherType.CAROUSEL;
                mPropertyModel.set(IS_TAB_CAROUSEL_VISIBLE, isTabCarousel);
                mPropertyModel.set(IS_TAB_CAROUSEL_TITLE_VISIBLE, isTabCarousel);

                // Set the initial state.
                mPropertyModel.set(IS_SURFACE_BODY_VISIBLE, true);
                mPropertyModel.set(IS_FAKE_SEARCH_BOX_VISIBLE, true);
                mPropertyModel.set(IS_VOICE_RECOGNITION_BUTTON_VISIBLE, false);
                mPropertyModel.set(IS_LENS_BUTTON_VISIBLE, false);
            }

            // Show feed loading image if necessary.
            if (shouldShowFeedPlaceholder()) {
                assert feedPlaceholderParentView != null;
                mFeedPlaceholderCoordinator =
                        new FeedPlaceholderCoordinator(context, feedPlaceholderParentView, false);
                mHasFeedPlaceholderShown = true;
            }

            mIsIncognito = mTabModelSelector.isIncognitoSelected();

            mTabModelSelectorObserver = new TabModelSelectorObserver() {
                @Override
                public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                    // TODO(crbug.com/982018): Optimize to not listen for selected Tab model change
                    // when overview is not shown.
                    updateIncognitoMode(newModel.isIncognito());
                }
            };
            mPropertyModel.set(IS_INCOGNITO, mIsIncognito);

            mPropertyModel.set(MORE_TABS_CLICK_LISTENER, this);

            // Hide tab carousel, which does not exist in incognito mode, when closing all
            // normal tabs.
            mNormalTabModelObserver = new TabModelObserver() {
                @Override
                public void willCloseTab(Tab tab, boolean animate, boolean didCloseAlone) {
                    if (isHomepageShown() && mTabModelSelector.getModel(false).getCount() <= 1) {
                        setTabCarouselVisibility(false);
                    }
                }
                @Override
                public void tabClosureUndone(Tab tab) {
                    if (isHomepageShown()) {
                        setTabCarouselVisibility(true);
                    }
                }

                @Override
                public void restoreCompleted() {
                    if (!(mPropertyModel.get(IS_SHOWING_OVERVIEW) && isHomepageShown())) {
                        return;
                    }
                    setTabCarouselVisibility(
                            mTabModelSelector.getModel(false).getCount() > 0 && !mIsIncognito);
                }

                @Override
                public void willAddTab(Tab tab, @TabLaunchType int type) {
                    if (isHomepageShown() && type != TabLaunchType.FROM_LONGPRESS_BACKGROUND) {
                        // Log if the creation of this tab will hide the surface and there is an
                        // ongoing feed launch. If the tab creation is due to a feed card tap, "card
                        // tapped" should already have been logged marking the end of the launch.
                        FeedReliabilityLogger logger = getFeedReliabilityLogger();
                        if (logger != null) {
                            logger.onPageLoadStarted();
                        }
                    }

                    // When the tab model is empty and a new background tab is added, it is
                    // immediately selected, which normally causes the overview to hide. We
                    // don't want to hide the overview when creating a tab in the background, so
                    // when a background tab is added to an empty tab model, we should skip the next
                    // onTabSelecting().
                    mHideOverviewOnTabSelecting = mTabModelSelector.getModel(false).getCount() != 0
                            || type != TabLaunchType.FROM_LONGPRESS_BACKGROUND;
                }

                @Override
                public void didSelectTab(Tab tab, int type, int lastId) {
                    if (type == TabSelectionType.FROM_CLOSE
                            && UrlUtilities.isNTPUrl(tab.getUrl())) {
                        setTabCarouselVisibility(false);
                    }
                }
            };
            if (mTabModelSelector.getModels().isEmpty()) {
                TabModelSelectorObserver selectorObserver = new TabModelSelectorObserver() {
                    @Override
                    public void onChange() {
                        assert !mTabModelSelector.getModels().isEmpty();
                        assert mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(
                                false)
                                != null;
                        assert mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(true)
                                != null;
                        mTabModelSelector.removeObserver(this);
                        mNormalTabModel = mTabModelSelector.getModel(false);
                        if (mPendingObserver) {
                            mPendingObserver = false;
                            mNormalTabModel.addObserver(mNormalTabModelObserver);
                        }
                    }
                };
                mTabModelSelector.addObserver(selectorObserver);
            } else {
                mNormalTabModel = mTabModelSelector.getModel(false);
            }

            mBrowserControlsObserver = new BrowserControlsStateProvider.Observer() {
                @Override
                public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
                        int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
                    if (isHomepageShown()) {
                        // Set the top margin to the top controls min height (indicator height if
                        // it's shown) since the toolbar height as extra margin is handled by top
                        // toolbar placeholder.
                        setTopMargin(mBrowserControlsStateProvider.getTopControlsMinHeightOffset());
                    } else if (mStartSurfaceState == StartSurfaceState.SHOWN_TABSWITCHER) {
                        // Set the top margin to the top controls offset (toolbar height + indicator
                        // height).
                        setTopMargin(mBrowserControlsStateProvider.getContentOffset());
                    } else {
                        setTopMargin(0);
                    }
                }

                @Override
                public void onBottomControlsHeightChanged(
                        int bottomControlsHeight, int bottomControlsMinHeight) {
                    // Only pad single pane home page since tabs grid has already been
                    // padded for the bottom bar.
                    if (isHomepageShown()) {
                        setBottomMargin(bottomControlsHeight);
                    } else {
                        setBottomMargin(0);
                    }
                }
            };

            mUrlFocusChangeListener = new UrlFocusChangeListener() {
                @Override
                public void onUrlFocusChange(boolean hasFocus) {
                    if (hasFakeSearchBox()) {
                        setFakeBoxVisibility(!hasFocus);
                        // TODO(crbug.com/1365694): We should call setLogoVisibility(!hasFocus)
                        // here.
                        // However, AppBarLayout's getTotalScrollRange() eliminates the gone
                        // child view's heights. Therefore, when focus is got, the AppBarLayout's
                        // scroll offset (based on getTotalScrollRange()) doesn't count the logo's
                        // height; when focus is cleared, this wrong (smaller than actual) scroll
                        // offset is restored, causing AppBarLayout to show partially.
                        // Actually setting fake box gone also causes similar offset problem, but we
                        // decided to keep fake box as-is for now for two reasons:
                        // 1. The fake box's height is small enough. Although AppBarLayout still
                        // shows partial blank bottom part when focus is cleared, it's small enough
                        // and not noticeable.
                        // 2. It would be confusing if both real and fake search boxes are visible
                        // to users.
                        // We should find a way to set both views gone when search box is
                        // focused without causing offset issues, but right now it's unclear what
                        // the plan could be regarding this stuff.
                    }
                    notifyStateChange();
                }
            };

            tweakMarginsBetweenSections();
        }

        mController.addTabSwitcherViewObserver(this);
        mPreviousStartSurfaceState = StartSurfaceState.NOT_SHOWN;
        mStartSurfaceState = StartSurfaceState.NOT_SHOWN;

        if (backPressManager != null && BackPressManager.isEnabled()) {
            backPressManager.addHandler(this, Type.START_SURFACE);
            if (mPropertyModel != null) {
                mPropertyModel.addObserver((source, key) -> {
                    if (key == IS_INCOGNITO) notifyBackPressStateChanged();
                });
            }
            mController.getHandleBackPressChangedSupplier().addObserver(
                    (v) -> notifyBackPressStateChanged());
            mController.isDialogVisibleSupplier().addObserver((v) -> notifyBackPressStateChanged());
            notifyBackPressStateChanged();
        }
    }

    void initWithNative(@Nullable OmniboxStub omniboxStub,
            @Nullable ExploreSurfaceCoordinatorFactory exploreSurfaceCoordinatorFactory,
            PrefService prefService, @Nullable SnackbarManager snackbarManager) {
        mIsNativeInitialized = true;
        mOmniboxStub = omniboxStub;
        mExploreSurfaceCoordinatorFactory = exploreSurfaceCoordinatorFactory;
        mSnackbarManager = snackbarManager;
        if (mPropertyModel != null) {
            assert mOmniboxStub != null;

            // Initialize
            // Note that isVoiceSearchEnabled will return false in incognito mode.
            mPropertyModel.set(IS_VOICE_RECOGNITION_BUTTON_VISIBLE,
                    mOmniboxStub.getVoiceRecognitionHandler().isVoiceSearchEnabled());
            updateLensVisibility();

            // This is for Instant Start when overview is already visible while the omnibox, Feed
            // and MV tiles haven't been set.
            if (mController.overviewVisible()) {
                mOmniboxStub.addUrlFocusChangeListener(mUrlFocusChangeListener);
                if (isHomepageShown()) {
                    if (mExploreSurfaceCoordinatorFactory != null) {
                        setExploreSurfaceVisibility(!mIsIncognito);
                    }
                    if (mInitializeMVTilesRunnable != null) mInitializeMVTilesRunnable.run();
                    if (mLogoCoordinator != null) mLogoCoordinator.initWithNative();
                }
            }

            if (mTabSwitcherModule != null) {
                mPropertyModel.set(FAKE_SEARCH_BOX_CLICK_LISTENER, v -> {
                    mOmniboxStub.setUrlBarFocus(
                            true, null, OmniboxFocusReason.TASKS_SURFACE_FAKE_BOX_TAP);
                    RecordUserAction.record("TasksSurface.FakeBox.Tapped");
                });
                mPropertyModel.set(FAKE_SEARCH_BOX_TEXT_WATCHER, new TextWatcher() {
                    @Override
                    public void beforeTextChanged(CharSequence s, int start, int count, int after) {
                    }

                    @Override
                    public void onTextChanged(CharSequence s, int start, int before, int count) {}

                    @Override
                    public void afterTextChanged(Editable s) {
                        if (s.length() == 0) return;
                        mOmniboxStub.setUrlBarFocus(true, s.toString(),
                                OmniboxFocusReason.TASKS_SURFACE_FAKE_BOX_LONG_PRESS);
                        RecordUserAction.record("TasksSurface.FakeBox.LongPressed");

                        // This won't cause infinite loop since we checked s.length() == 0 above.
                        s.clear();
                    }
                });
                mPropertyModel.set(VOICE_SEARCH_BUTTON_CLICK_LISTENER, v -> {
                    FeedReliabilityLogger feedReliabilityLogger = getFeedReliabilityLogger();
                    if (feedReliabilityLogger != null) {
                        feedReliabilityLogger.onVoiceSearch();
                    }
                    mOmniboxStub.getVoiceRecognitionHandler().startVoiceRecognition(
                            VoiceRecognitionHandler.VoiceInteractionSource.TASKS_SURFACE);
                    RecordUserAction.record("TasksSurface.FakeBox.VoiceSearch");
                });

                mPropertyModel.set(LENS_BUTTON_CLICK_LISTENER, v -> {
                    LensMetrics.recordClicked(LensEntryPoint.TASKS_SURFACE);
                    mOmniboxStub.startLens(LensEntryPoint.TASKS_SURFACE);
                });
            }
        }
        if (mTabSwitcherModule != null) {
            mTabSwitcherModule.initWithNative();
        }
        mFeedVisibilityPrefOnStartUp = prefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE);

        // Trigger the creation of spare tab for StartSurface after the native is initialized to
        // speed up navigation from start.
        maybeScheduleSpareTabCreation();
    }

    void onProfileAvailable(Profile profile) {
        if (profile.isOffTheRecord()) return;

        TemplateUrlServiceFactory.getForProfile(profile).addObserver(this::updateLensVisibility);
        mProfileSupplier.removeObserver(this::onProfileAvailable);
    }

    private void updateLensVisibility() {
        if (mOmniboxStub == null) return;

        boolean shouldShowLensButton = mOmniboxStub.isLensEnabled(LensEntryPoint.TASKS_SURFACE);
        LensMetrics.recordShown(LensEntryPoint.TASKS_SURFACE, shouldShowLensButton);
        mPropertyModel.set(IS_LENS_BUTTON_VISIBLE, shouldShowLensButton);
    }

    void destroy() {
        if (mLogoCoordinator != null) {
            mLogoCoordinator.destroy();
            mLogoCoordinator = null;
        }
        if (mCallbackController != null) {
            mCallbackController.destroy();
        }
        if (mProfileSupplier.get() != null) {
            TemplateUrlServiceFactory.getForProfile(mProfileSupplier.get())
                    .removeObserver(this::updateLensVisibility);
        }
        mProfileSupplier.removeObserver(this::onProfileAvailable);
        mayRecordHomepageSessionEnd();
        mActivityLifecycleDispatcher.unregister(this);
    }

    /**
     * Returns true if START_SURFACE_SPARE_TAB feature is enabled.
     */
    private static boolean isStartSurfaceSpareTabEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.START_SURFACE_SPARE_TAB);
    }

    /**
     * Schedules creating a spare tab when native is initialized and when start surface is shown.
     */
    private void maybeScheduleSpareTabCreation() {
        // Only create spare tab when native is initialized. If start surface is shown before native
        // is initialized, this will be invoked later.
        if (!mIsNativeInitialized) return;

        // Only create a spare tab if tab creator exists.
        if (mTabCreatorManager == null) return;
        TabCreator tabCreator = mTabCreatorManager.getTabCreator(mIsIncognito);
        // Don't create a spare tab when no tab creator is present.
        if (tabCreator == null) return;

        // Only create a spare tab if the StartSurfaceSpareTab feature is enabled.
        if (!isStartSurfaceSpareTabEnabled()) return;

        // Only create a spare tab when start surface is shown.
        if (!isHomepageShown()) return;

        recordTimeBetweenShowAndCreate();
        // We use UI_DEFAULT priority to not slow down high priority tasks such as user input.
        // As this is behavior is behind a feature flag, based on the results we will deviate to
        // lower priority if needed.
        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT,
                ()
                        -> WarmupManager.getInstance().createSpareTab(
                                tabCreator, TabLaunchType.FROM_START_SURFACE));
    }

    /**
     * Show Start Surface home view. Note: this should be called only when refactor flag is enabled.
     * @param animate Whether to play an entry animation.
     */
    void show(boolean animate) {
        assert ReturnToChromeUtil.isStartSurfaceEnabled(mContext) && mIsStartSurfaceRefactorEnabled;

        // This null check is for testing.
        if (mPropertyModel == null) return;

        mIsHomepageShown = true;
        notifyShowStateChange();

        mIsIncognito = mTabModelSelector.isIncognitoSelected();
        mPropertyModel.set(IS_INCOGNITO, mIsIncognito);
        setMVTilesVisibility(!mIsIncognito);
        setLogoVisibility(!mIsIncognito);
        setTabCarouselVisibility(getNormalTabCount() > 0 && !mIsIncognito);
        setExploreSurfaceVisibility(!mIsIncognito && mExploreSurfaceCoordinatorFactory != null);
        // TODO(qinmin): show query tiles when flag is enabled.
        setQueryTilesVisibility(false);
        setFakeBoxVisibility(!mIsIncognito);
        updateTopToolbarPlaceholderHeight();
        // Set the top margin to the top controls min height (indicator height if it's shown)
        // since the toolbar height as extra margin is handled by top toolbar placeholder.
        setTopMargin(mBrowserControlsStateProvider.getTopControlsMinHeight());
        // Only pad single pane home page since tabs grid has already been padding for the
        // bottom bar.
        setBottomMargin(mBrowserControlsStateProvider.getBottomControlsHeight());
        setIncognitoModeDescriptionVisibility(
                mIsIncognito && (mTabModelSelector.getModel(true).getCount() <= 0));

        // Make sure ExploreSurfaceCoordinator is built before the explore surface is showing
        // by default.
        if (mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)
                && mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR) == null
                && !mActivityStateChecker.isFinishingOrDestroyed()
                && mExploreSurfaceCoordinatorFactory != null) {
            createAndSetExploreSurfaceCoordinator();
        }

        // TODO(crbug.com/1315676): Remove this property key since overview should always be visible
        // when show() is called.
        mPropertyModel.set(IS_SHOWING_OVERVIEW, true);

        if (mNormalTabModel != null) {
            mNormalTabModel.addObserver(mNormalTabModelObserver);
        } else {
            mPendingObserver = true;
        }

        mTabModelSelector.addObserver(mTabModelSelectorObserver);

        if (mBrowserControlsObserver != null) {
            mBrowserControlsStateProvider.addObserver(mBrowserControlsObserver);
        }

        if (mOmniboxStub != null) {
            mOmniboxStub.addUrlFocusChangeListener(mUrlFocusChangeListener);
        }

        // This should only be called for single or carousel tab switcher.
        mController.showTabSwitcherView(animate);

        RecordUserAction.record("StartSurface.Shown");
        RecordUserAction.record("StartSurface.SinglePane.Home");
        mayRecordHomepageSessionBegin();

        maybeScheduleSpareTabCreation();
    }

    void setSecondaryTasksSurfacePropertyModel(PropertyModel propertyModel) {
        mSecondaryTasksSurfacePropertyModel = propertyModel;
        mSecondaryTasksSurfacePropertyModel.set(IS_INCOGNITO, mIsIncognito);

        // Secondary tasks surface is used for more Tabs or incognito mode single pane, where MV
        // tiles and voice recognition button should be invisible.
        mSecondaryTasksSurfacePropertyModel.set(MV_TILES_VISIBLE, false);
        mSecondaryTasksSurfacePropertyModel.set(QUERY_TILES_VISIBLE, false);
        mSecondaryTasksSurfacePropertyModel.set(IS_VOICE_RECOGNITION_BUTTON_VISIBLE, false);
        mSecondaryTasksSurfacePropertyModel.set(IS_LENS_BUTTON_VISIBLE, false);
    }

    void addStateChangeObserver(StartSurface.StateObserver observer) {
        mStateObservers.addObserver(observer);
    }

    void removeStateChangeObserver(StartSurface.StateObserver observer) {
        mStateObservers.removeObserver(observer);
    }

    // Implements StartSurface.Controller
    // TODO(crbug.com/1115757): After crrev.com/c/2315823, Overview state and Startsurface state are
    // two different things, audit the wording usage and see if we can rename this method to
    // setStartSurfaceState.
    void setStartSurfaceState(
            @StartSurfaceState int state, @NewTabPageLaunchOrigin int launchOrigin) {
        // TODO(crbug.com/1039691): Refactor into state and trigger to separate SHOWING and SHOWN
        // states.

        if (mPropertyModel == null || state == mStartSurfaceState) return;

        // Cache previous state.
        int cachedPreviousState = mPreviousStartSurfaceState;
        if (mStartSurfaceState != StartSurfaceState.NOT_SHOWN) {
            mPreviousStartSurfaceState = mStartSurfaceState;
        }

        mStartSurfaceState = state;
        setOverviewStateInternal();

        // Immediately transition from SHOWING to SHOWN state if overview is visible but state not
        // SHOWN. This is only necessary when the new state is a SHOWING state.
        if (mPropertyModel.get(IS_SHOWING_OVERVIEW)
                && mStartSurfaceState != StartSurfaceState.NOT_SHOWN
                && !isShownState(mStartSurfaceState)) {
            // Compute SHOWN state before updating previous state, because the previous state is
            // still needed to compute the shown state.
            @StartSurfaceState
            int shownState = computeOverviewStateShown();

            mStartSurfaceState = shownState;
            setOverviewStateInternal();
        }
        notifyStateChange();

        setLaunchOrigin(launchOrigin);
        // Metrics collection
        if (mStartSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE) {
            RecordUserAction.record("StartSurface.SinglePane.Home");
        } else if (mStartSurfaceState == StartSurfaceState.SHOWN_TABSWITCHER) {
            RecordUserAction.record("StartSurface.SinglePane.Tabswitcher");
        } else if (mStartSurfaceState == StartSurfaceState.SHOWING_PREVIOUS
                && cachedPreviousState == StartSurfaceState.SHOWN_HOMEPAGE) {
            ReturnToChromeUtil.recordBackNavigationToStart("FromTab");
        }
    }

    void setStartSurfaceState(@StartSurfaceState int state) {
        setStartSurfaceState(state, mLaunchOrigin);
    }

    void setLaunchOrigin(@NewTabPageLaunchOrigin int launchOrigin) {
        if (launchOrigin == NewTabPageLaunchOrigin.WEB_FEED) {
            StartSurfaceUserData.getInstance().saveFeedInstanceState(null);
        }
        mLaunchOrigin = launchOrigin;
        // If the ExploreSurfaceCoordinator is already initialized, set the TabId.
        if (mPropertyModel == null) return;
        ExploreSurfaceCoordinator exploreSurfaceCoordinator =
                mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR);
        if (exploreSurfaceCoordinator != null) {
            exploreSurfaceCoordinator.setTabIdFromLaunchOrigin(mLaunchOrigin);
        }
    }

    void resetScrollPosition() {
        if (mPropertyModel == null) return;

        mPropertyModel.set(RESET_TASK_SURFACE_HEADER_SCROLL_POSITION, true);
        mPropertyModel.set(RESET_FEED_SURFACE_SCROLL_POSITION, true);
        StartSurfaceUserData.getInstance().saveFeedInstanceState(null);
    }

    // TODO(crbug.com/1115757): After crrev.com/c/2315823, Overview state and Startsurface state are
    // two different things, audit the wording usage and see if we can rename this method to
    // setStartSurfaceStateInternal.
    private void setOverviewStateInternal() {
        if (mStartSurfaceState == StartSurfaceState.SHOWING_HOMEPAGE
                || mStartSurfaceState == StartSurfaceState.SHOWING_START) {
            // When entering the Start surface by tapping home button or new tab page, we need to
            // reset the scrolling position.
            resetScrollPosition();
        } else if (mStartSurfaceState == StartSurfaceState.SHOWING_TABSWITCHER) {
            onHide();
            // Set secondary surface visible to make sure tab list recyclerview is updated in time
            // (before GTS animations start). We need to skip
            // mSecondaryTasksSurfaceController#showOverview here since it will hide GTS animations.
            setSecondaryTasksSurfaceVisibility(
                    /* isVisible= */ true, /* skipUpdateController = */ true);
        } else if (mStartSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE) {
            if (mPreviousStartSurfaceState == StartSurfaceState.SHOWN_TABSWITCHER) {
                mayRecordHomepageSessionBegin();
            }
            boolean hasNormalTab = getNormalTabCount() > 0;

            // If new home surface for home button is enabled, MV tiles and carousel tab switcher
            // will not show.
            setMVTilesVisibility(!mIsIncognito);
            setLogoVisibility(!mIsIncognito);
            setTabCarouselVisibility(hasNormalTab && !mIsIncognito);
            setExploreSurfaceVisibility(!mIsIncognito && mExploreSurfaceCoordinatorFactory != null);
            setQueryTilesVisibility(!mIsIncognito);
            setFakeBoxVisibility(!mIsIncognito);
            setSecondaryTasksSurfaceVisibility(mIsIncognito, /* skipUpdateController = */ false);
            updateTopToolbarPlaceholderHeight();
            // Set the top margin to the top controls min height (indicator height if it's shown)
            // since the toolbar height as extra margin is handled by top toolbar placeholder.
            setTopMargin(mBrowserControlsStateProvider.getTopControlsMinHeight());
            // Only pad single pane home page since tabs grid has already been padding for the
            // bottom bar.
            setBottomMargin(mBrowserControlsStateProvider.getBottomControlsHeight());
            if (mNormalTabModel != null) {
                mNormalTabModel.addObserver(mNormalTabModelObserver);
            } else {
                mPendingObserver = true;
            }
        } else if (mStartSurfaceState == StartSurfaceState.SHOWN_TABSWITCHER) {
            if (mPreviousStartSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE) {
                mayRecordHomepageSessionEnd();
            }
            setTabCarouselVisibility(false);
            setMVTilesVisibility(false);
            setLogoVisibility(false);
            setQueryTilesVisibility(false);
            setFakeBoxVisibility(false);
            setSecondaryTasksSurfaceVisibility(
                    /* isVisible= */ true, /* skipUpdateController = */ false);
            setExploreSurfaceVisibility(false);
            updateTopToolbarPlaceholderHeight();
            // Set the top margin to the top controls height (toolbar height + indicator height).
            setTopMargin(mBrowserControlsStateProvider.getTopControlsHeight());
            setBottomMargin(0);
        } else if (mStartSurfaceState == StartSurfaceState.NOT_SHOWN) {
            if (mSecondaryTasksSurfacePropertyModel != null) {
                setSecondaryTasksSurfaceVisibility(
                        /* isVisible= */ false, /* skipUpdateController = */ false);
            }
        }

        if (isShownState(mStartSurfaceState)) {
            setIncognitoModeDescriptionVisibility(
                    mIsIncognito && (mTabModelSelector.getModel(true).getCount() <= 0));
        }
    }

    @StartSurfaceState
    int getStartSurfaceState() {
        return mStartSurfaceState;
    }

    int getPreviousStartSurfaceState() {
        return mPreviousStartSurfaceState;
    }

    ViewGroup getTabSwitcherContainer() {
        return mTabSwitcherContainer;
    }

    @Nullable
    TabSwitcher.Controller getTabSwitcherController() {
        return mSecondaryTasksSurfaceController;
    }

    void setSnackbarParentView(ViewGroup parentView) {
        if (mSnackbarManager == null) return;
        mSnackbarManager.setParentView(parentView);
    }

    void addTabSwitcherViewObserver(TabSwitcherViewObserver observer) {
        mObservers.addObserver(observer);
    }

    void removeTabSwitcherViewObserver(TabSwitcherViewObserver observer) {
        mObservers.removeObserver(observer);
    }

    void hideTabSwitcherView(boolean animate) {
        mController.hideTabSwitcherView(animate);
    }

    void beforeHideTabSwitcherView() {
        mController.prepareHideTabSwitcherView();
    }

    void showOverview(boolean animate) {
        // TODO(crbug.com/982018): Animate the bottom bar together with the Tab Grid view.
        if (mPropertyModel != null) {
            RecordUserAction.record("StartSurface.Shown");

            // update incognito
            mIsIncognito = mTabModelSelector.isIncognitoSelected();
            mPropertyModel.set(IS_INCOGNITO, mIsIncognito);

            // if OverviewModeState is NOT_SHOWN, default to SHOWING_TABSWITCHER. This should only
            // happen when entering Start through SwipeDown gesture on URL bar.
            if (mStartSurfaceState == StartSurfaceState.NOT_SHOWN) {
                mStartSurfaceState = StartSurfaceState.SHOWING_TABSWITCHER;
            }

            // set OverviewModeState
            @StartSurfaceState
            int shownState = computeOverviewStateShown();
            assert (isShownState(shownState));
            setStartSurfaceState(shownState);

            // Make sure ExploreSurfaceCoordinator is built before the explore surface is showing
            // by default.
            if (mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)
                    && mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR) == null
                    && !mActivityStateChecker.isFinishingOrDestroyed()
                    && mExploreSurfaceCoordinatorFactory != null) {
                createAndSetExploreSurfaceCoordinator();
            }
            mTabModelSelector.addObserver(mTabModelSelectorObserver);

            if (mBrowserControlsObserver != null) {
                mBrowserControlsStateProvider.addObserver(mBrowserControlsObserver);
            }

            mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
            if (mOmniboxStub != null) {
                mOmniboxStub.addUrlFocusChangeListener(mUrlFocusChangeListener);
            }
        }
        mayRecordHomepageSessionBegin();
        mController.showTabSwitcherView(animate);

        maybeScheduleSpareTabCreation();
    }

    /**
     * This function no longer handles the case when Start is disabled. Instead, the back operations
     * of the grid tab switcher is handled by TabSwitcherMediator.
     */
    boolean onBackPressed() {
        boolean ret = onBackPressedInternal();
        if (ret) {
            BackPressManager.record(BackPressHandler.Type.START_SURFACE);
        }
        return ret;
    }

    /**
     * This function handles the following cases:
     * 1) Start surface is showing, including with/without refactoring enabled;
     * 2) Grid tab switcher is showing, but only when Start surface is enabled and refactoring is
     *    disabled. This is because the transitions between Start surface and tab switcher
     *    (secondary tasks view) is handled by the same Layout via state changes. So we have to
     *    handle the two surfaces together.
     *    In the ideal scenarios: when a) Start is disabled and b) Start surface refactoring is
     *    enabled, the back operations of the grid tab switcher is handled by TabSwitcherMediator.
     */
    private boolean onBackPressedInternal() {
        boolean isOnHomepage = isHomepageShown();

        // When the SecondaryTasksSurface is shown, the TabGridDialog is controlled by
        // mSecondaryTasksSurfaceController, while the TabSelectionEditor dialog is controlled
        // by mController. Therefore, we need to check both controllers whether any dialog is
        // visible. If so, the corresponding controller will handle the back button.
        // When the Start surface is shown, tapping "Group Tabs" from menu will also show the
        // the TabSelectionEditor dialog. Therefore, we need to check both controllers as well.
        if (mSecondaryTasksSurfaceController != null
                && mSecondaryTasksSurfaceController.isDialogVisible()) {
            boolean ret = mSecondaryTasksSurfaceController.onBackPressed();
            assert !BackPressManager.isEnabled()
                    || ret : String.format("Wrong back press state: %s, start surface: %s",
                                     mSecondaryTasksSurfaceController.getClass().getName(),
                                     mStartSurfaceState);
            return ret;
        } else if (mController.isDialogVisible()) {
            boolean ret = mController.onBackPressed();
            assert !BackPressManager.isEnabled()
                    || ret : String.format("Wrong back press state: %s, start surface: %s",
                                     mController.getClass().getName(), mStartSurfaceState);
            return ret;
        }

        if (mStartSurfaceState == StartSurfaceState.SHOWN_TABSWITCHER) {
            if (mPreviousStartSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE && !mIsIncognito) {
                // Secondary tasks surface is used as the main surface in incognito mode.
                // If we reached Tab switcher from HomePage, and there isn't any dialog shown,
                // updates the state, and ChromeTabbedActivity will handle the back button.
                setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);
                ReturnToChromeUtil.recordBackNavigationToStart("FromTabSwitcher");
                return true;
            } else {
                boolean ret = mSecondaryTasksSurfaceController.onBackPressed();
                assert !BackPressManager.isEnabled()
                        || ret : String.format("Wrong back press state: %s, start surface: %s",
                                         mSecondaryTasksSurfaceController.getClass().getName(),
                                         mStartSurfaceState);
                return ret;
            }
        }

        if (isOnHomepage) {
            FeedReliabilityLogger feedReliabilityLogger = getFeedReliabilityLogger();
            if (feedReliabilityLogger != null) {
                feedReliabilityLogger.onNavigateBack();
            }
        }

        // crbug.com/1420410: secondary task surface might be doing animations when transiting
        // to/from tab switcher and then intercept back press to wait for animation to be finished.
        boolean ret = mController.onBackPressed()
                || (mSecondaryTasksSurfaceController != null
                        && mSecondaryTasksSurfaceController.onBackPressed());
        assert !BackPressManager.isEnabled()
                || ret : String.format("Wrong back press state: %s, start surface: %s",
                                 mController.getClass().getName(), mStartSurfaceState);
        return ret;
    }

    void onHide() {
        if (mFeedPlaceholderCoordinator != null) {
            mFeedPlaceholderCoordinator.destroy();
            mFeedPlaceholderCoordinator = null;
        }
        if (mTabSwitcherModule != null) {
            mTabSwitcherModule.getTabListDelegate().postHiding();
        }
    }

    @Override
    public @BackPressResult int handleBackPress() {
        boolean ret = onBackPressedInternal();
        notifyBackPressStateChanged();
        return ret ? BackPressResult.SUCCESS : BackPressResult.FAILURE;
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressChangedSupplier;
    }

    void onOverviewShownAtLaunch(long activityCreationTimeMs) {
        mController.onOverviewShownAtLaunch(activityCreationTimeMs);
        if (mPropertyModel != null) {
            ExploreSurfaceCoordinator exploreSurfaceCoordinator =
                    mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR);
            if (exploreSurfaceCoordinator != null) {
                exploreSurfaceCoordinator.onOverviewShownAtLaunch(activityCreationTimeMs);
            }
        }

        assert mPropertyModel == null || mFeedVisibilityInSharedPreferenceOnStartUp != null;
        if (mFeedVisibilityPrefOnStartUp != null) {
            RecordHistogram.recordBooleanHistogram(FEED_VISIBILITY_CONSISTENCY,
                    mFeedVisibilityPrefOnStartUp.equals(
                            mFeedVisibilityInSharedPreferenceOnStartUp));
        }
        if (mFeedPlaceholderCoordinator != null) {
            mFeedPlaceholderCoordinator.onOverviewShownAtLaunch(activityCreationTimeMs);
        }
    }

    @Deprecated
    // TODO(1347089): Removes this test after the refactoring is enabled by default. This is because
    // the StartSurfaceState will go away.
    boolean isShowingStartSurfaceHomepage() {
        // When state is SHOWN_HOMEPAGE or SHOWING_HOMEPAGE or SHOWING_START, state surface homepage
        // is showing. When state is StartSurfaceState.SHOWING_PREVIOUS and the previous state is
        // SHOWN_HOMEPAGE or NOT_SHOWN, homepage is showing.
        return mStartSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE
                || mStartSurfaceState == StartSurfaceState.SHOWING_HOMEPAGE
                || mStartSurfaceState == StartSurfaceState.SHOWING_START
                || (mStartSurfaceState == StartSurfaceState.SHOWING_PREVIOUS
                        && (mPreviousStartSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE
                                || mPreviousStartSurfaceState == StartSurfaceState.NOT_SHOWN));
    }

    // Implements TabSwitcher.TabSwitcherViewObserver.
    @Override
    public void startedShowing() {
        for (TabSwitcherViewObserver observer : mObservers) {
            observer.startedShowing();
        }
    }

    @Override
    public void finishedShowing() {
        for (TabSwitcherViewObserver observer : mObservers) {
            observer.finishedShowing();
        }
    }

    @Override
    public void startedHiding() {
        if (mPropertyModel != null) {
            if (mOmniboxStub != null) {
                mOmniboxStub.removeUrlFocusChangeListener(mUrlFocusChangeListener);
            }
            mPropertyModel.set(IS_SHOWING_OVERVIEW, false);

            destroyExploreSurfaceCoordinator();
            if (mNormalTabModelObserver != null) {
                if (mNormalTabModel != null) {
                    mNormalTabModel.removeObserver(mNormalTabModelObserver);
                } else if (mPendingObserver) {
                    mPendingObserver = false;
                }
            }
            if (mTabModelSelectorObserver != null) {
                mTabModelSelector.removeObserver(mTabModelSelectorObserver);
            }
            if (mBrowserControlsObserver != null) {
                mBrowserControlsStateProvider.removeObserver(mBrowserControlsObserver);
            }
            setStartSurfaceState(StartSurfaceState.NOT_SHOWN);
            RecordUserAction.record("StartSurface.Hidden");
            mIsHomepageShown = false;
        }

        // Since the start surface is hidden, destroy any spare tabs created.
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT, () -> WarmupManager.getInstance().destroySpareTab());

        for (TabSwitcherViewObserver observer : mObservers) {
            observer.startedHiding();
        }
    }

    @Override
    public void finishedHiding() {
        for (TabSwitcherViewObserver observer : mObservers) {
            observer.finishedHiding();
        }
    }

    private void destroyExploreSurfaceCoordinator() {
        ExploreSurfaceCoordinator exploreSurfaceCoordinator =
                mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR);
        FeedReliabilityLogger logger = getFeedReliabilityLogger();
        if (logger != null) {
            mOmniboxStub.removeUrlFocusChangeListener(logger);
        }
        if (exploreSurfaceCoordinator != null) exploreSurfaceCoordinator.destroy();
        mPropertyModel.set(EXPLORE_SURFACE_COORDINATOR, null);
    }

    // TODO(crbug.com/982018): turn into onClickMoreTabs() and hide the OnClickListener signature
    // inside. Implements View.OnClickListener, which listens for the more tabs button.
    @Override
    public void onClick(View v) {
        assert isHomepageShown();

        if (mIsStartSurfaceRefactorEnabled) {
            mTabSwitcherClickHandler.onClick(v);
        } else {
            if (mSecondaryTasksSurfacePropertyModel == null) {
                TabSwitcher.Controller controller = mSecondaryTasksSurfaceInitializer.initialize();
                assert mSecondaryTasksSurfacePropertyModel != null;
                setSecondaryTasksSurfaceController(controller);
            }

            setStartSurfaceState(StartSurfaceState.SHOWN_TABSWITCHER);
        }
        RecordUserAction.record("StartSurface.SinglePane.MoreTabs");
    }

    // StartSurface.OnTabSelectingListener
    @Override
    public void onTabSelecting(long time, int tabId) {
        if (!mHideOverviewOnTabSelecting) {
            mHideOverviewOnTabSelecting = true;
            return;
        }
        assert mOnTabSelectingListener != null;
        mOnTabSelectingListener.onTabSelecting(time, tabId);
    }

    // LogoCoordinator.VisibilityObserver
    @Override
    public void onLogoVisibilityChanged() {
        updateTopToolbarPlaceholderHeight();
    }

    @VisibleForTesting
    public boolean shouldShowFeedPlaceholder() {
        if (mFeedVisibilityInSharedPreferenceOnStartUp == null) {
            mFeedVisibilityInSharedPreferenceOnStartUp =
                    ReturnToChromeUtil.getFeedArticlesVisibility();
        }

        return mIsStartSurfaceEnabled && ChromeFeatureList.sInstantStart.isEnabled()
                && ReturnToChromeUtil.getFeedArticlesVisibility() && !mHadWarmStart
                && !mHasFeedPlaceholderShown;
    }

    void setSecondaryTasksSurfaceController(
            TabSwitcher.Controller secondaryTasksSurfaceController) {
        mSecondaryTasksSurfaceController = secondaryTasksSurfaceController;
        mSecondaryTasksSurfaceController.isDialogVisibleSupplier().addObserver(
                (v) -> notifyBackPressStateChanged());
        mSecondaryTasksSurfaceController.getHandleBackPressChangedSupplier().addObserver(
                (v) -> notifyBackPressStateChanged());
    }

    /** This interface builds the feed surface coordinator when showing if needed. */
    private void setExploreSurfaceVisibility(boolean isVisible) {
        if (isVisible == mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)) return;

        if (isVisible && mPropertyModel.get(IS_SHOWING_OVERVIEW)
                && mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR) == null
                && !mActivityStateChecker.isFinishingOrDestroyed()) {
            createAndSetExploreSurfaceCoordinator();
        }

        mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, isVisible);

        // Pull-to-refresh is not supported when explore surface is not visible, i.e. in tab
        // switcher mode.
        ExploreSurfaceCoordinator exploreSurfaceCoordinator =
                mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR);
        if (exploreSurfaceCoordinator != null) {
            exploreSurfaceCoordinator.enableSwipeRefresh(isVisible);
        }
    }

    private void updateIncognitoMode(boolean isIncognito) {
        if (isIncognito == mIsIncognito) return;
        mIsIncognito = isIncognito;

        mPropertyModel.set(IS_INCOGNITO, mIsIncognito);
        setOverviewStateInternal();

        // TODO(crbug.com/1021399): This looks not needed since there is no way to change incognito
        // mode when focusing on the omnibox and incognito mode change won't affect the visibility
        // of the tab switcher toolbar.
        if (mPropertyModel.get(IS_SHOWING_OVERVIEW)) notifyStateChange();
    }

    /**
     * Set the visibility of secondary tasks surface. Secondary tasks surface is used for showing
     * normal grid tab switcher and incognito gird tab switcher.
     * @param isVisible Whether secondary tasks surface is visible.
     * @param skipUpdateController Whether to skip mSecondaryTasksSurfaceController#showOverview and
     *         mSecondaryTasksSurfaceController#hideOverview.
     */
    private void setSecondaryTasksSurfaceVisibility(
            boolean isVisible, boolean skipUpdateController) {
        assert mIsStartSurfaceEnabled;

        if (isVisible) {
            if (mSecondaryTasksSurfacePropertyModel == null) {
                setSecondaryTasksSurfaceController(mSecondaryTasksSurfaceInitializer.initialize());
            }
            if (mSecondaryTasksSurfacePropertyModel != null) {
                mSecondaryTasksSurfacePropertyModel.set(IS_FAKE_SEARCH_BOX_VISIBLE, false);
                mSecondaryTasksSurfacePropertyModel.set(IS_INCOGNITO, mIsIncognito);
            }
            if (mSecondaryTasksSurfaceController != null && !skipUpdateController) {
                mSecondaryTasksSurfaceController.showTabSwitcherView(/* animate = */ true);
            }
        } else {
            if (mSecondaryTasksSurfaceController != null && !skipUpdateController) {
                mSecondaryTasksSurfaceController.hideTabSwitcherView(/* animate = */ false);
                if (mStartSurfaceSupplier.get() != null
                        && mStartSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE) {
                    mStartSurfaceSupplier.get().getGridTabListDelegate().postHiding();
                }
            }
        }
        mPropertyModel.set(IS_SECONDARY_SURFACE_VISIBLE, isVisible);
    }

    private void notifyStateChange() {
        notifyShowStateChange();
        if (!mIsStartSurfaceRefactorEnabled) {
            notifyStartSurfaceStateChange();
        }
    }

    private void notifyShowStateChange() {
        // StartSurface is being supplied with OneShotSupplier, notification sends after
        // StartSurface is available to avoid missing events. More detail see:
        // https://crrev.com/c/2427428.
        mController.onHomepageChanged();
        notifyBackPressStateChanged();
    }

    // TODO(1315676): Remove this when the Start surface refactoring is enabled by default.
    private void notifyStartSurfaceStateChange() {
        if (mSecondaryTasksSurfaceController != null) {
            mSecondaryTasksSurfaceController.onHomepageChanged();
        }
        mStartSurfaceSupplier.onAvailable((unused) -> {
            for (StartSurface.StateObserver observer : mStateObservers) {
                observer.onStateChanged(mStartSurfaceState, shouldShowTabSwitcherToolbar());
            }
        });
    }

    private boolean hasFakeSearchBox() {
        return isHomepageShown();
    }

    @VisibleForTesting
    public boolean shouldShowTabSwitcherToolbar() {
        // Always show in TABSWITCHER
        if (mStartSurfaceState == StartSurfaceState.SHOWN_TABSWITCHER
                || mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE)) {
            return true;
        }

        // Hide when focusing the Omnibox on the primary surface.
        return hasFakeSearchBox() && mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE);
    }

    private void setTopMargin(int topMargin) {
        mPropertyModel.set(TOP_MARGIN, topMargin);
    }

    private void setBottomMargin(int bottomMargin) {
        mPropertyModel.set(BOTTOM_BAR_HEIGHT, bottomMargin);
    }

    /**
     * This method should be called after setLogoVisibility() since we need to know whether logo is
     * shown or not to decide the height.
     * @param height The height of the top toolbar placeholder.
     */
    private void updateTopToolbarPlaceholderHeight() {
        mPropertyModel.set(TOP_TOOLBAR_PLACEHOLDER_HEIGHT,
                mStartSurfaceState == StartSurfaceState.SHOWN_TABSWITCHER
                        ? 0
                        : getTopToolbarPlaceholderHeight());
    }

    private void setTabCarouselVisibility(boolean isVisible) {
        // If the single tab switcher is shown and the current selected tab is a new tab page, we
        // shouldn't show the tab switcher layout on Start.
        boolean shouldShowTabCarousel =
                isVisible && !(isSingleTabSwitcher() && isCurrentSelectedTabNTP());

        if (shouldShowTabCarousel == mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE)) return;

        mPropertyModel.set(IS_TAB_CAROUSEL_VISIBLE, shouldShowTabCarousel);
        mPropertyModel.set(
                IS_TAB_CAROUSEL_TITLE_VISIBLE, shouldShowTabCarousel && showTabSwitcherTitle());
    }

    private void setMVTilesVisibility(boolean isVisible) {
        if (mInitializeMVTilesRunnable == null) return;
        if (isVisible && mInitializeMVTilesRunnable != null) mInitializeMVTilesRunnable.run();
        mPropertyModel.set(MV_TILES_VISIBLE, isVisible);
    }

    private void setLogoVisibility(boolean isVisible) {
        if (!mIsFeedGoneImprovementEnabled) return;

        if (isVisible && mLogoCoordinator == null) {
            mLogoCoordinator = initializeLogo();
            if (mIsNativeInitialized) mLogoCoordinator.initWithNative();
        }
        if (mLogoCoordinator != null) {
            boolean isShowingHomepage = isHomepageShown();
            mLogoCoordinator.updateVisibilityAndMaybeCleanUp(
                    isShowingHomepage && isVisible, !isShowingHomepage, false);
        }
    }

    private void setQueryTilesVisibility(boolean isVisible) {
        if (mExcludeQueryTiles || isVisible == mPropertyModel.get(QUERY_TILES_VISIBLE)) return;
        mPropertyModel.set(QUERY_TILES_VISIBLE, isVisible);
    }

    private void setFakeBoxVisibility(boolean isVisible) {
        if (mPropertyModel == null) return;
        mPropertyModel.set(IS_FAKE_SEARCH_BOX_VISIBLE, isVisible);

        // This is because VoiceRecognitionHandler monitors incognito mode and returns
        // false in incognito mode. However, when switching incognito mode, this class is notified
        // earlier than the VoiceRecognitionHandler, so isVoiceSearchEnabled returns
        // incorrect state if check synchronously.
        ThreadUtils.postOnUiThread(() -> {
            if (mOmniboxStub != null) {
                if (mOmniboxStub.getVoiceRecognitionHandler() != null) {
                    mPropertyModel.set(IS_VOICE_RECOGNITION_BUTTON_VISIBLE,
                            mOmniboxStub.getVoiceRecognitionHandler().isVoiceSearchEnabled());
                }
                mPropertyModel.set(IS_LENS_BUTTON_VISIBLE,
                        mOmniboxStub.isLensEnabled(LensEntryPoint.TASKS_SURFACE));
            }
        });
    }

    private void setIncognitoModeDescriptionVisibility(boolean isVisible) {
        if (isVisible == mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_VISIBLE)) return;

        if (!mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_INITIALIZED)) {
            mPropertyModel.set(IS_INCOGNITO_DESCRIPTION_INITIALIZED, true);
        }
        mPropertyModel.set(IS_INCOGNITO_DESCRIPTION_VISIBLE, isVisible);
        mPropertyModel.set(IS_SURFACE_BODY_VISIBLE, !isVisible);
        if (mSecondaryTasksSurfacePropertyModel != null) {
            if (!mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO_DESCRIPTION_INITIALIZED)) {
                mSecondaryTasksSurfacePropertyModel.set(IS_INCOGNITO_DESCRIPTION_INITIALIZED, true);
            }
            mSecondaryTasksSurfacePropertyModel.set(IS_INCOGNITO_DESCRIPTION_VISIBLE, isVisible);
            mSecondaryTasksSurfacePropertyModel.set(IS_SURFACE_BODY_VISIBLE, !isVisible);
        }
    }

    // TODO(crbug.com/1115757): After crrev.com/c/2315823, Overview state and Startsurface state are
    // two different things, audit the wording usage and see if we can rename this method to
    // computeStartSurfaceState.
    @StartSurfaceState
    private int computeOverviewStateShown() {
        if (mIsStartSurfaceEnabled) {
            if (mStartSurfaceState == StartSurfaceState.SHOWING_PREVIOUS) {
                assert mPreviousStartSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE
                        || mPreviousStartSurfaceState == StartSurfaceState.SHOWN_TABSWITCHER
                        || mPreviousStartSurfaceState == StartSurfaceState.NOT_SHOWN;

                // This class would be re-instantiated after changing theme, then
                // mPreviousOverviewModeState will be reset to OverviewModeState.NOT_SHOWN. We
                // default to OverviewModeState.SHOWN_HOMEPAGE in this case when SHOWING_PREVIOUS.
                return mPreviousStartSurfaceState == StartSurfaceState.NOT_SHOWN
                        ? StartSurfaceState.SHOWN_HOMEPAGE
                        : mPreviousStartSurfaceState;
            } else if (mStartSurfaceState == StartSurfaceState.SHOWING_START) {
                if (mTabModelSelector.isIncognitoSelected()) {
                    return StartSurfaceState.SHOWN_TABSWITCHER;
                }
                return StartSurfaceState.SHOWN_HOMEPAGE;
            } else if (mStartSurfaceState == StartSurfaceState.SHOWING_TABSWITCHER) {
                return StartSurfaceState.SHOWN_TABSWITCHER;
            } else if (mStartSurfaceState == StartSurfaceState.SHOWING_HOMEPAGE) {
                return StartSurfaceState.SHOWN_HOMEPAGE;
            } else {
                assert (isShownState(mStartSurfaceState)
                        || mStartSurfaceState == StartSurfaceState.NOT_SHOWN);
                return mStartSurfaceState;
            }
        }
        return StartSurfaceState.DISABLED;
    }

    private boolean isShownState(@StartSurfaceState int state) {
        return state == StartSurfaceState.SHOWN_HOMEPAGE
                || state == StartSurfaceState.SHOWN_TABSWITCHER;
    }

    private int getNormalTabCount() {
        if (!mTabModelSelector.isTabStateInitialized()) {
            return SharedPreferencesManager.getInstance().readInt(
                    ChromePreferenceKeys.REGULAR_TAB_COUNT);
        } else {
            return mTabModelSelector.getModel(false).getCount();
        }
    }

    private boolean isCurrentSelectedTabNTP() {
        Tab currentTab = mTabModelSelector.getCurrentTab();
        return mTabModelSelector.isTabStateInitialized() && currentTab != null
                        && currentTab.getUrl() != null
                ? UrlUtilities.isNTPUrl(currentTab.getUrl())
                : SharedPreferencesManager.getInstance().readInt(
                          ChromePreferenceKeys.APP_LAUNCH_LAST_KNOWN_ACTIVE_TAB_STATE)
                        == ActiveTabState.NTP;
    }

    private boolean isSingleTabSwitcher() {
        return mController.getTabSwitcherType() == TabSwitcherType.SINGLE;
    }

    private boolean showTabSwitcherTitle() {
        return !isSingleTabSwitcher();
    }

    private void notifyBackPressStateChanged() {
        mBackPressChangedSupplier.set(shouldInterceptBackPress());
    }

    @VisibleForTesting
    boolean shouldInterceptBackPress() {
        if (mSecondaryTasksSurfaceController != null
                && mSecondaryTasksSurfaceController.isDialogVisible()) {
            return true;
        } else if (mController.isDialogVisible()) {
            return true;
        }

        if (mStartSurfaceState == StartSurfaceState.SHOWN_TABSWITCHER) {
            if (mPreviousStartSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE && !mIsIncognito) {
                return true;
            } else if (mSecondaryTasksSurfaceController != null) {
                return Boolean.TRUE.equals(
                        mSecondaryTasksSurfaceController.getHandleBackPressChangedSupplier().get());
            }
        }

        if (mIsStartSurfaceRefactorEnabled && ReturnToChromeUtil.isStartSurfaceEnabled(mContext)) {
            return false;
        }

        if (Boolean.TRUE.equals(mController.getHandleBackPressChangedSupplier().get())) return true;

        return mSecondaryTasksSurfaceController != null
                && Boolean.TRUE.equals(
                        mSecondaryTasksSurfaceController.getHandleBackPressChangedSupplier().get());
    }

    boolean isLogoVisible() {
        return mLogoCoordinator != null && mLogoCoordinator.isLogoVisible();
    }

    void performSearchQuery(String queryText, List<String> searchParams) {
        if (mOmniboxStub != null) {
            mOmniboxStub.performSearchQuery(queryText, searchParams);
        }
    }

    void setOnTabSelectingListener(StartSurface.OnTabSelectingListener onTabSelectingListener) {
        mOnTabSelectingListener = onTabSelectingListener;
    }

    int getTopToolbarPlaceholderHeight() {
        // If logo is visible in Start surface instead of in the toolbar, we don't need to show the
        // top margin of the fake search box.
        return getPixelSize(R.dimen.control_container_height)
                + (isLogoVisible()
                                ? 0
                                : getPixelSize(R.dimen.start_surface_fake_search_box_top_margin));
    }

    private int getPixelSize(int id) {
        return mContext.getResources().getDimensionPixelSize(id);
    }

    private void createAndSetExploreSurfaceCoordinator() {
        ExploreSurfaceCoordinator exploreSurfaceCoordinator =
                mExploreSurfaceCoordinatorFactory.create(
                        ColorUtils.inNightMode(mContext), mHasFeedPlaceholderShown, mLaunchOrigin);
        mPropertyModel.set(EXPLORE_SURFACE_COORDINATOR, exploreSurfaceCoordinator);
        FeedReliabilityLogger feedReliabilityLogger =
                exploreSurfaceCoordinator.getFeedReliabilityLogger();
        if (feedReliabilityLogger != null) {
            mOmniboxStub.addUrlFocusChangeListener(feedReliabilityLogger);
        }
    }

    private LogoCoordinator initializeLogo() {
        Callback<LoadUrlParams> logoClickedCallback =
                mCallbackController.makeCancelable((urlParams) -> {
                    // On NTP, the logo is in the new tab page layout instead of the toolbar and the
                    // logo click events are processed in NewTabPageLayout. This callback passed
                    // into TopToolbarCoordinator will only be used for StartSurfaceToolbar, so add
                    // an assertion here.
                    assert ReturnToChromeUtil.isStartSurfaceEnabled(mContext);
                    ReturnToChromeUtil.handleLoadUrlFromStartSurface(
                            urlParams, /*incognito=*/false, mParentTabSupplier.get());
                });
        mLogoContainerView.setVisibility(View.VISIBLE);

        mLogoCoordinator = new LogoCoordinator(mContext, logoClickedCallback,
                mLogoContainerView.findViewById(R.id.search_provider_logo), true, null, null,
                isHomepageShown(), this);
        return mLogoCoordinator;
    }

    FeedReliabilityLogger getFeedReliabilityLogger() {
        if (mPropertyModel == null) return null;
        ExploreSurfaceCoordinator coordinator = mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR);
        return coordinator != null ? coordinator.getFeedReliabilityLogger() : null;
    }

    private void tweakMarginsBetweenSections() {
        Resources resources = mContext.getResources();
        mPropertyModel.set(TASKS_SURFACE_BODY_TOP_MARGIN,
                resources.getDimensionPixelSize(R.dimen.tasks_surface_body_top_margin));
        mPropertyModel.set(MV_TILES_CONTAINER_TOP_MARGIN,
                resources.getDimensionPixelSize(R.dimen.mv_tiles_container_top_margin));
        mPropertyModel.set(TAB_SWITCHER_TITLE_TOP_MARGIN,
                resources.getDimensionPixelSize(R.dimen.tab_switcher_title_top_margin));

        // If improving Start surface when Feed is disabled is needed, mvt grid layout (two row) is
        // shown.
        if (mIsFeedGoneImprovementEnabled) {
            mPropertyModel.set(MV_TILES_CONTAINER_TOP_MARGIN,
                    resources.getDimensionPixelOffset(R.dimen.tile_grid_layout_top_margin)
                            + resources.getDimensionPixelOffset(
                                    R.dimen.ntp_search_box_bottom_margin));
            mPropertyModel.set(MV_TILES_CONTAINER_LEFT_RIGHT_MARGIN,
                    resources.getDimensionPixelSize(R.dimen.ntp_header_lateral_paddings_v2));
            if (isSingleTabSwitcher()) {
                mPropertyModel.set(SINGLE_TAB_TOP_MARGIN,
                        resources.getDimensionPixelOffset(
                                R.dimen.single_tab_view_top_margin_for_feed_improvement));
            }
        }
    }

    @Override
    public void onResumeWithNative() {
        mayRecordHomepageSessionBegin();
    }

    @Override
    public void onPauseWithNative() {
        mayRecordHomepageSessionEnd();
    }

    /** Records UMA for the time spent on Start Surface. */
    private void recordTimeSpendInStart() {
        RecordHistogram.recordMediumTimesHistogram(
                "StartSurface.TimeSpent", System.currentTimeMillis() - mLastShownTimeMs);
    }

    /**
     * Records the UMA between the time that a start surface appears and the time at which a spare
     * tab creation is initiated.
     */
    private void recordTimeBetweenShowAndCreate() {
        RecordHistogram.recordMediumTimesHistogram("StartSurface.SpareTab.TimeBetweenShowAndCreate",
                System.currentTimeMillis() - mLastShownTimeMs);
    }

    /**
     * If mLastShownTimeMs is set as an actual time and hasn't been recorded by other start surface
     * hiding actions, the recordTimeSpendInStart function will be called. We then reset
     * mLastShownTimeMs as the default value in case it will be recorded again by another hiding
     * action.
     */
    void mayRecordHomepageSessionEnd() {
        if (mLastShownTimeMs != LAST_SHOW_TIME_NOT_SET) {
            recordTimeSpendInStart();
            mLastShownTimeMs = LAST_SHOW_TIME_NOT_SET;
        }
    }

    private void mayRecordHomepageSessionBegin() {
        if (isHomepageShown() && mLastShownTimeMs == LAST_SHOW_TIME_NOT_SET) {
            mLastShownTimeMs = System.currentTimeMillis();
        }
    }

    /**
     * Returns whether the Start surface homepage is showing.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    boolean isHomepageShown() {
        return mIsStartSurfaceRefactorEnabled
                ? mIsHomepageShown
                : mStartSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE;
    }

    @VisibleForTesting
    public FeedActionDelegate getFeedActionDelegateForTesting() {
        assert mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR) != null;
        return mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR)
                .getFeedActionDelegateForTesting(); // IN-TEST
    }

    TabSwitcher getTabSwitcherModuleForTesting() {
        return mTabSwitcherModule;
    }

    Runnable getInitializeMVTilesRunnableForTesting() {
        return mInitializeMVTilesRunnable;
    }
}
