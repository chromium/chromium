// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_HEIGHT;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.EXPLORE_SURFACE_COORDINATOR;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_EXPLORE_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SHOWING_OVERVIEW;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.RESET_FEED_SURFACE_SCROLL_POSITION;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.TOP_MARGIN;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.BACKGROUND_COLOR;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.FAKE_SEARCH_BOX_CLICK_LISTENER;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.FAKE_SEARCH_BOX_TEXT_WATCHER;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_FAKE_SEARCH_BOX_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_INCOGNITO;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_INCOGNITO_DESCRIPTION_INITIALIZED;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_INCOGNITO_DESCRIPTION_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_LENS_BUTTON_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_SURFACE_BODY_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_TAB_CARD_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_VOICE_RECOGNITION_BUTTON_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.LENS_BUTTON_CLICK_LISTENER;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.MAGIC_STACK_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.MV_TILES_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.RESET_TASK_SURFACE_HEADER_SCROLL_POSITION;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.TOP_TOOLBAR_PLACEHOLDER_HEIGHT;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.VOICE_SEARCH_BUTTON_CLICK_LISTENER;

import android.content.Context;
import android.graphics.Point;
import android.text.Editable;
import android.view.View;

import androidx.annotation.ColorInt;
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
import org.chromium.chrome.browser.logo.LogoUtils;
import org.chromium.chrome.browser.logo.LogoView;
import org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager;
import org.chromium.chrome.browser.magic_stack.HomeModulesCoordinator;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegateHost;
import org.chromium.chrome.browser.ntp.NewTabPageLaunchOrigin;
import org.chromium.chrome.browser.omnibox.OmniboxFocusReason;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;
import org.chromium.chrome.browser.tab_ui.TabSwitcher.Controller;
import org.chromium.chrome.browser.tab_ui.TabSwitcher.TabSwitcherType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.ActiveTabState;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.chrome.browser.util.BrowserUiUtils.HostSurface;
import org.chromium.chrome.features.start_surface.StartSurface.TabSwitcherViewObserver;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.prefs.PrefService;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.EmptyTextWatcher;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

import java.util.List;

/** The mediator implements the logic to interact with the surfaces and caller. */
class StartSurfaceMediator
        implements TabSwitcher.TabSwitcherViewObserver,
                StartSurface.OnTabSelectingListener,
                BackPressHandler,
                LogoCoordinator.VisibilityObserver,
                PauseResumeWithNativeObserver,
                ModuleDelegateHost {
    /** Interface to check the associated activity state. */
    interface ActivityStateChecker {
        /**
         * @return Whether the associated activity is finishing or destroyed.
         */
        boolean isFinishingOrDestroyed();
    }

    /** Interface to create the magic stack. */
    interface ModuleDelegateCreator {
        /**
         * Creates the magic stack {@link ModuleDelegate} object.
         *
         * @param moduleDelegateHost The home surface which owns the magic stack.
         */
        ModuleDelegate create(ModuleDelegateHost moduleDelegateHost);
    }

    private final ObserverList<TabSwitcherViewObserver> mObservers = new ObserverList<>();
    private TabSwitcher.Controller mController;
    private final TabModelSelector mTabModelSelector;
    @Nullable private final PropertyModel mPropertyModel;
    private final boolean mIsStartSurfaceEnabled;
    private final boolean mHadWarmStart;
    private final Runnable mInitializeMVTilesRunnable;
    private final Supplier<Tab> mParentTabSupplier;
    private final ObservableSupplierImpl<Boolean> mBackPressChangedSupplier =
            new ObservableSupplierImpl<>();
    private final CallbackController mCallbackController = new CallbackController();
    private final View mLogoContainerView;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final TabCreatorManager mTabCreatorManager;
    private final boolean mUseMagicStack;
    private final boolean mIsSurfacePolishEnabled;
    @Nullable private final ModuleDelegateCreator mModuleDelegateCreator;
    private boolean mShouldIgnoreTabSelecting;

    private static final int LAST_SHOW_TIME_NOT_SET = -1;
    @Nullable private ExploreSurfaceCoordinatorFactory mExploreSurfaceCoordinatorFactory;

    // Non-null when ReturnToChromeUtils#shouldImproveStartWhenFeedIsDisabled is enabled and
    // homepage is shown.
    @Nullable private LogoCoordinator mLogoCoordinator;
    private boolean mIsIncognito;
    @Nullable private OmniboxStub mOmniboxStub;
    private Context mContext;
    @Nullable UrlFocusChangeListener mUrlFocusChangeListener;
    @NewTabPageLaunchOrigin private int mLaunchOrigin;
    @Nullable private TabModel mNormalTabModel;
    @Nullable private TabModelObserver mNormalTabModelObserver;
    @Nullable private UiConfig mUiConfig;
    private final int mStartMargin;

    @Nullable
    // Observes both regular and incognito TabModel. This observer is responsible to initiate the
    // hiding of the Start surface and layout.
    private TabModelObserver mTabModelObserver;

    @Nullable private TabModelSelectorObserver mTabModelSelectorObserver;
    private BrowserControlsStateProvider mBrowserControlsStateProvider;
    private BrowserControlsStateProvider.Observer mBrowserControlsObserver;
    private ActivityStateChecker mActivityStateChecker;

    // It indicates whether the Start surface homepage is showing when we no longer calculate
    // StartSurfaceState.
    private boolean mIsHomepageShown;

    /**
     * Whether a pending observer needed be added to the normal TabModel after the TabModel is
     * initialized.
     */
    private boolean mPendingObserver;

    private boolean mHideOverviewOnTabSelecting = true;
    private StartSurface.OnTabSelectingListener mOnTabSelectingListener;
    private TabSwitcher mTabSwitcherModule;
    private boolean mIsNativeInitialized;
    // The timestamp at which the Start Surface was last shown to the user.
    private long mLastShownTimeMs = LAST_SHOW_TIME_NOT_SET;
    private ObservableSupplier<Profile> mProfileSupplier;

    private HomeModulesCoordinator mHomeModulesCoordinator;
    private Point mContextMenuStartPosotion;

    StartSurfaceMediator(
            @Nullable TabSwitcher tabSwitcherModule,
            TabModelSelector tabModelSelector,
            @Nullable PropertyModel propertyModel,
            boolean isStartSurfaceEnabled,
            Context context,
            BrowserControlsStateProvider browserControlsStateProvider,
            ActivityStateChecker activityStateChecker,
            @Nullable TabCreatorManager tabCreatorManager,
            boolean hadWarmStart,
            Runnable initializeMVTilesRunnable,
            @Nullable ModuleDelegateCreator moduleDelegateCreator,
            Supplier<Tab> parentTabSupplier,
            View logoContainerView,
            @Nullable BackPressManager backPressManager,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            ObservableSupplier<Profile> profileSupplier) {
        mTabSwitcherModule = tabSwitcherModule;
        mController = mTabSwitcherModule != null ? mTabSwitcherModule.getController() : null;
        mIsSurfacePolishEnabled =
                isStartSurfaceEnabled && ChromeFeatureList.sSurfacePolish.isEnabled();
        mUseMagicStack = isStartSurfaceEnabled && StartSurfaceConfiguration.useMagicStack();
        // When a magic stack is enabled on Start surface, it doesn't need a controller to handle
        // its showing and hiding.
        assert mController != null || mUseMagicStack;

        mTabModelSelector = tabModelSelector;
        mPropertyModel = propertyModel;
        mIsStartSurfaceEnabled = isStartSurfaceEnabled;
        mContext = context;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mActivityStateChecker = activityStateChecker;
        mTabCreatorManager = tabCreatorManager;
        mHadWarmStart = hadWarmStart;
        mLaunchOrigin = NewTabPageLaunchOrigin.UNKNOWN;
        mInitializeMVTilesRunnable = initializeMVTilesRunnable;
        mModuleDelegateCreator = moduleDelegateCreator;
        mParentTabSupplier = parentTabSupplier;
        mLogoContainerView = logoContainerView;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);
        mProfileSupplier = profileSupplier;
        mProfileSupplier.addObserver(this::onProfileAvailable);

        mStartMargin =
                context.getResources().getDimensionPixelSize(R.dimen.mvt_container_lateral_margin);

        if (mPropertyModel != null) {
            assert mIsStartSurfaceEnabled;

            if (mTabSwitcherModule != null) {
                mPropertyModel.set(IS_TAB_CARD_VISIBLE, false);
            }

            if (mTabSwitcherModule != null || mUseMagicStack) {
                // Set the initial state.
                mPropertyModel.set(IS_SURFACE_BODY_VISIBLE, true);
                mPropertyModel.set(IS_FAKE_SEARCH_BOX_VISIBLE, true);
                mPropertyModel.set(IS_VOICE_RECOGNITION_BUTTON_VISIBLE, false);
                mPropertyModel.set(IS_LENS_BUTTON_VISIBLE, false);
            }

            mIsIncognito = mTabModelSelector.isIncognitoSelected();

            mTabModelSelectorObserver =
                    new TabModelSelectorObserver() {
                        @Override
                        public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                            // TODO(crbug.com/40635216): Optimize to not listen for selected Tab
                            // model
                            // change when overview is not shown.
                            updateIncognitoMode(newModel.isIncognito());
                        }
                    };
            mPropertyModel.set(IS_INCOGNITO, mIsIncognito);
            updateBackgroundColor(mPropertyModel);

            if (!mUseMagicStack) {
                // Hide tab carousel, which does not exist in incognito mode, when closing all
                // normal tabs.
                // This TabModelObserver observes the regular TabModel only.
                mNormalTabModelObserver =
                        new TabModelObserver() {
                            @Override
                            public void willCloseTab(Tab tab, boolean didCloseAlone) {
                                if (isHomepageShown()
                                        && mTabModelSelector.getModel(false).getCount() <= 1) {
                                    setTabCardVisibility(false);
                                }
                            }

                            @Override
                            public void tabClosureUndone(Tab tab) {
                                if (isHomepageShown()) {
                                    setTabCardVisibility(true);
                                }
                            }

                            @Override
                            public void restoreCompleted() {
                                if (!(mPropertyModel.get(IS_SHOWING_OVERVIEW)
                                        && isHomepageShown())) {
                                    return;
                                }
                                setTabCardVisibility(
                                        mTabModelSelector.getModel(false).getCount() > 0
                                                && !mIsIncognito);
                            }

                            @Override
                            public void willAddTab(Tab tab, @TabLaunchType int type) {
                                if (isHomepageShown()
                                        && type != TabLaunchType.FROM_LONGPRESS_BACKGROUND) {
                                    // Log if the creation of this tab will hide the surface and
                                    // there is an ongoing feed launch. If the tab creation is due
                                    // to a feed card tap, "card tapped" should already have been
                                    // logged marking the end of the launch.
                                    FeedReliabilityLogger logger = getFeedReliabilityLogger();
                                    if (logger != null) {
                                        logger.onPageLoadStarted();
                                    }
                                }

                                // When the tab model is empty and a new background tab is added, it
                                // is immediately selected, which normally causes the overview to
                                // hide.
                                // We don't want to hide the overview when creating a tab in the
                                // background, so when a background tab is added to an empty tab
                                // model, we should skip the next onTabSelecting().
                                mHideOverviewOnTabSelecting =
                                        mTabModelSelector.getModel(false).getCount() != 0
                                                || type != TabLaunchType.FROM_LONGPRESS_BACKGROUND;
                            }

                            @Override
                            public void didSelectTab(Tab tab, int type, int lastId) {
                                if (mUseMagicStack) return;

                                if (type == TabSelectionType.FROM_CLOSE
                                        && UrlUtilities.isNtpUrl(tab.getUrl())) {
                                    setTabCardVisibility(false);
                                }
                            }
                        };
            } else {
                // This TabModelObserver observes both the regular and incognito TabModels.
                mTabModelObserver =
                        new TabModelObserver() {
                            @Override
                            public void didSelectTab(Tab tab, int type, int lastId) {
                                assert mUseMagicStack;
                                if (type == TabSelectionType.FROM_CLOSE
                                        || type == TabSelectionType.FROM_UNDO) {
                                    return;
                                }
                                onTabSelecting(mTabModelSelector.getCurrentTabId());
                            }
                        };
                mContextMenuStartPosotion =
                        ReturnToChromeUtil.calculateContextMenuStartPosition(
                                mContext.getResources());
            }
            if (mTabModelSelector.getModels().isEmpty()) {
                TabModelSelectorObserver selectorObserver =
                        new TabModelSelectorObserver() {
                            @Override
                            public void onChange() {
                                assert !mTabModelSelector.getModels().isEmpty();
                                assert mTabModelSelector
                                                .getTabModelFilterProvider()
                                                .getTabModelFilter(false)
                                        != null;
                                assert mTabModelSelector
                                                .getTabModelFilterProvider()
                                                .getTabModelFilter(true)
                                        != null;
                                mTabModelSelector.removeObserver(this);
                                mNormalTabModel = mTabModelSelector.getModel(false);
                                if (mPendingObserver) {
                                    mPendingObserver = false;
                                    if (!mUseMagicStack) {
                                        mNormalTabModel.addObserver(mNormalTabModelObserver);
                                    } else {
                                        mTabModelSelector
                                                .getTabModelFilterProvider()
                                                .addTabModelFilterObserver(mTabModelObserver);
                                    }
                                }
                            }
                        };
                mTabModelSelector.addObserver(selectorObserver);
            } else {
                mNormalTabModel = mTabModelSelector.getModel(false);
            }

            mBrowserControlsObserver =
                    new BrowserControlsStateProvider.Observer() {
                        @Override
                        public void onControlsOffsetChanged(
                                int topOffset,
                                int topControlsMinHeightOffset,
                                int bottomOffset,
                                int bottomControlsMinHeightOffset,
                                boolean needsAnimate) {
                            if (isHomepageShown()) {
                                // Set the top margin to the top controls min height (indicator
                                // height if it's shown) since the toolbar height as extra margin
                                // is handled by top toolbar placeholder.
                                setTopMargin(
                                        mBrowserControlsStateProvider
                                                .getTopControlsMinHeightOffset());
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

            mUrlFocusChangeListener =
                    new UrlFocusChangeListener() {
                        @Override
                        public void onUrlFocusChange(boolean hasFocus) {
                            if (hasFakeSearchBox()) {
                                setFakeBoxVisibility(!hasFocus);
                                // TODO(crbug.com/40239477): We should call
                                // setLogoVisibility(!hasFocus) here.
                                // However, AppBarLayout's getTotalScrollRange() eliminates the gone
                                // child view's heights. Therefore, when focus is got, the
                                // AppBarLayout's scroll offset (based on getTotalScrollRange())
                                // doesn't count the logo's height; when focus is cleared, this
                                // wrong (smaller than actual) scroll offset is restored, causing
                                // AppBarLayout to show partially.
                                // Actually setting fake box gone also causes similar offset
                                // problem, but we decided to keep fake box as-is for now for two
                                // reasons:
                                // 1. The fake box's height is small enough. Although AppBarLayout
                                //    still shows partial blank bottom part when focus is cleared,
                                //    it's small enough and not noticeable.
                                // 2. It would be confusing if both real and fake search boxes are
                                //    visible to users.
                                //
                                // We should find a way to set both views gone when search box is
                                // focused without causing offset issues, but right now it's unclear
                                // what the plan could be regarding this stuff.
                            }
                            notifyStateChange();
                        }
                    };
        }

        if (mController != null) {
            mController.addTabSwitcherViewObserver(this);
        }

        if (backPressManager != null && BackPressManager.isEnabled()) {
            backPressManager.addHandler(this, Type.START_SURFACE);
            if (mPropertyModel != null) {
                mPropertyModel.addObserver(
                        (source, key) -> {
                            if (key == IS_INCOGNITO) notifyBackPressStateChanged();
                        });
            }
            if (mController != null) {
                mController
                        .getHandleBackPressChangedSupplier()
                        .addObserver((v) -> notifyBackPressStateChanged());
                mController
                        .isDialogVisibleSupplier()
                        .addObserver((v) -> notifyBackPressStateChanged());
            }
            notifyBackPressStateChanged();
        }
    }

    void initWithNative(
            @Nullable OmniboxStub omniboxStub,
            @Nullable ExploreSurfaceCoordinatorFactory exploreSurfaceCoordinatorFactory,
            PrefService prefService) {
        mIsNativeInitialized = true;
        mOmniboxStub = omniboxStub;
        mExploreSurfaceCoordinatorFactory = exploreSurfaceCoordinatorFactory;
        if (mPropertyModel != null) {
            assert mOmniboxStub != null;

            // Initialize.
            // Note that isVoiceSearchEnabled will return false in incognito mode.
            mPropertyModel.set(
                    IS_VOICE_RECOGNITION_BUTTON_VISIBLE,
                    mOmniboxStub.getVoiceRecognitionHandler().isVoiceSearchEnabled());
            updateLensVisibility();

            // This is for Instant Start when overview is already visible while the omnibox, Feed
            // and MV tiles haven't been set.
            if (isHomepageShown()) {
                mOmniboxStub.addUrlFocusChangeListener(mUrlFocusChangeListener);
                if (mExploreSurfaceCoordinatorFactory != null) {
                    setExploreSurfaceVisibility(!mIsIncognito);
                }
                if (mInitializeMVTilesRunnable != null) mInitializeMVTilesRunnable.run();
                if (mLogoCoordinator != null) mLogoCoordinator.initWithNative();
            }

            if (mTabSwitcherModule != null || mUseMagicStack) {
                mPropertyModel.set(
                        FAKE_SEARCH_BOX_CLICK_LISTENER,
                        v -> {
                            mOmniboxStub.setUrlBarFocus(
                                    true, null, OmniboxFocusReason.TASKS_SURFACE_FAKE_BOX_TAP);
                            RecordUserAction.record("TasksSurface.FakeBox.Tapped");
                        });
                mPropertyModel.set(
                        FAKE_SEARCH_BOX_TEXT_WATCHER,
                        new EmptyTextWatcher() {
                            @Override
                            public void afterTextChanged(Editable s) {
                                if (s.length() == 0) return;
                                mOmniboxStub.setUrlBarFocus(
                                        true,
                                        s.toString(),
                                        OmniboxFocusReason.TASKS_SURFACE_FAKE_BOX_LONG_PRESS);
                                RecordUserAction.record("TasksSurface.FakeBox.LongPressed");

                                // This won't cause infinite loop since we checked s.length() == 0
                                // above.
                                s.clear();
                            }
                        });
                mPropertyModel.set(
                        VOICE_SEARCH_BUTTON_CLICK_LISTENER,
                        v -> {
                            FeedReliabilityLogger feedReliabilityLogger =
                                    getFeedReliabilityLogger();
                            if (feedReliabilityLogger != null) {
                                feedReliabilityLogger.onVoiceSearch();
                            }
                            mOmniboxStub
                                    .getVoiceRecognitionHandler()
                                    .startVoiceRecognition(
                                            VoiceRecognitionHandler.VoiceInteractionSource
                                                    .TASKS_SURFACE);
                            RecordUserAction.record("TasksSurface.FakeBox.VoiceSearch");
                        });

                mPropertyModel.set(
                        LENS_BUTTON_CLICK_LISTENER,
                        v -> {
                            LensMetrics.recordClicked(LensEntryPoint.TASKS_SURFACE);
                            mOmniboxStub.startLens(LensEntryPoint.TASKS_SURFACE);
                        });
            }
        }
        if (mTabSwitcherModule != null) {
            mTabSwitcherModule.initWithNative();
        }

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
        if (mHomeModulesCoordinator != null) {
            mHomeModulesCoordinator.destroy();
        }
        if (mCallbackController != null) {
            mCallbackController.destroy();
        }
        if (mProfileSupplier.hasValue()) {
            TemplateUrlServiceFactory.getForProfile(mProfileSupplier.get())
                    .removeObserver(this::updateLensVisibility);
        }
        mProfileSupplier.removeObserver(this::onProfileAvailable);
        mayRecordHomepageSessionEnd();
        mActivityLifecycleDispatcher.unregister(this);
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

        // Only create a spare tab when start surface is shown.
        if (!isHomepageShown()) return;

        recordTimeBetweenShowAndCreate();

        if (!mIsIncognito) {
            Profile profile = mProfileSupplier.get();

            // We use UI_DEFAULT priority to not slow down high priority tasks such as user input.
            // As this is behavior is behind a feature flag, based on the results we will deviate to
            // lower priority if needed.
            PostTask.runOrPostTask(
                    TaskTraits.UI_DEFAULT,
                    () -> WarmupManager.getInstance().createRegularSpareTab(profile));
        }
    }

    /**
     * Show Start Surface home view. Note: this should be called only when refactor flag is enabled.
     * @param animate Whether to play an entry animation.
     */
    void show(boolean animate) {
        assert ReturnToChromeUtil.isStartSurfaceEnabled(mContext);

        // This null check is for testing.
        if (mPropertyModel == null) return;

        mIsHomepageShown = true;
        notifyShowStateChange();

        mIsIncognito = mTabModelSelector.isIncognitoSelected();
        mPropertyModel.set(IS_INCOGNITO, mIsIncognito);
        updateBackgroundColor(mPropertyModel);
        setMVTilesVisibility(!mIsIncognito);
        setMagicStackVisibility(!mIsIncognito);
        setLogoVisibility(!mIsIncognito);
        setTabCardVisibility(getNormalTabCount() > 0 && !mIsIncognito);
        setExploreSurfaceVisibility(!mIsIncognito && mExploreSurfaceCoordinatorFactory != null);
        setFakeBoxVisibility(!mIsIncognito);
        updateTopToolbarPlaceholderHeight();
        // Set the top margin to the top controls min height (indicator height if it's shown)
        // since the toolbar height as extra margin is handled by top toolbar placeholder.
        setTopMargin(mBrowserControlsStateProvider.getTopControlsMinHeight());
        // Only pad single pane home page since tabs grid has already been padding for the
        // bottom bar.
        setBottomMargin(mBrowserControlsStateProvider.getBottomControlsHeight());

        // Make sure ExploreSurfaceCoordinator is built before the explore surface is showing
        // by default.
        if (mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)
                && mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR) == null
                && !mActivityStateChecker.isFinishingOrDestroyed()
                && mExploreSurfaceCoordinatorFactory != null) {
            createAndSetExploreSurfaceCoordinator();
        }

        mPropertyModel.set(IS_SHOWING_OVERVIEW, true);

        if (mNormalTabModel != null) {
            if (!mUseMagicStack) {
                mNormalTabModel.addObserver(mNormalTabModelObserver);
            } else {
                mTabModelSelector
                        .getTabModelFilterProvider()
                        .addTabModelFilterObserver(mTabModelObserver);
            }
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
        if (mController != null) {
            mController.showTabSwitcherView(animate);
        }

        RecordUserAction.record("StartSurface.Shown");
        RecordUserAction.record("StartSurface.SinglePane.Home");
        mayRecordHomepageSessionBegin();

        maybeScheduleSpareTabCreation();
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

    void hideTabSwitcherView(boolean animate) {
        if (mController != null) {
            mController.hideTabSwitcherView(animate);
        } else {
            startedHiding();
        }
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
     * This function handles the case that Start surface is showing. The back operations of the grid
     * tab switcher is handled by TabSwitcherMediator.
     */
    private boolean onBackPressedInternal() {
        boolean isOnHomepage = isHomepageShown();

        if (mController != null && mController.isDialogVisible()) {
            boolean ret = mController.onBackPressed();
            assert !BackPressManager.isEnabled() || ret
                    : String.format(
                            "Wrong back press state: %s, is start surface shown : %s",
                            mController.getClass().getName(), mIsHomepageShown);
            return ret;
        }

        if (isOnHomepage) {
            FeedReliabilityLogger feedReliabilityLogger = getFeedReliabilityLogger();
            if (feedReliabilityLogger != null) {
                feedReliabilityLogger.onNavigateBack();
            }
        }

        if (mController != null) {
            // crbug.com/1420410: secondary task surface might be doing animations when transiting
            // to/from tab switcher and then intercept back press to wait for animation to be
            // finished.
            boolean ret = mController.onBackPressed();
            assert !BackPressManager.isEnabled() || ret
                    : String.format(
                            "Wrong back press state: %s, is start surface shown : %s",
                            mController.getClass().getName(), mIsHomepageShown);
            return ret;
        }
        return false;
    }

    void onHide() {
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
        if (mController != null) {
            mController.onOverviewShownAtLaunch(activityCreationTimeMs);
        }
        if (mPropertyModel != null) {
            ExploreSurfaceCoordinator exploreSurfaceCoordinator =
                    mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR);
            if (exploreSurfaceCoordinator != null) {
                exploreSurfaceCoordinator.onOverviewShownAtLaunch(activityCreationTimeMs);
            }
        }
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
            if (mHomeModulesCoordinator != null) {
                mHomeModulesCoordinator.hide();
            }
            if (mNormalTabModel != null) {
                if (mNormalTabModelObserver != null) {
                    mNormalTabModel.removeObserver(mNormalTabModelObserver);
                } else if (mTabModelObserver != null) {
                    mTabModelSelector
                            .getTabModelFilterProvider()
                            .removeTabModelFilterObserver(mTabModelObserver);
                }
            } else if (mPendingObserver) {
                mPendingObserver = false;
            }
            if (mTabModelSelectorObserver != null) {
                mTabModelSelector.removeObserver(mTabModelSelectorObserver);
            }
            if (mBrowserControlsObserver != null) {
                mBrowserControlsStateProvider.removeObserver(mBrowserControlsObserver);
            }
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

    // StartSurface.OnTabSelectingListener
    @Override
    public void onTabSelecting(int tabId) {
        if (!mHideOverviewOnTabSelecting) {
            mHideOverviewOnTabSelecting = true;
            return;
        }
        // Because there are multiple upstream tab selection listeners that attempt to re-trigger
        // the onTabSelecting in response to TabModelObserver#didSelectTab it is necessary to treat
        // this as a non-rentrant method until the original operation finishes.
        // TODO(crbug.com/40286397): This can be removed once StartSurfaceRefactor is cleaned up.
        if (mShouldIgnoreTabSelecting) return;
        mShouldIgnoreTabSelecting = true;

        assert mOnTabSelectingListener != null;
        mOnTabSelectingListener.onTabSelecting(tabId);
        mShouldIgnoreTabSelecting = false;
    }

    // LogoCoordinator.VisibilityObserver
    @Override
    public void onLogoVisibilityChanged() {
        updateTopToolbarPlaceholderHeight();
    }

    /** This interface builds the feed surface coordinator when showing if needed. */
    private void setExploreSurfaceVisibility(boolean isVisible) {
        if (isVisible == mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)) return;

        if (isVisible
                && mPropertyModel.get(IS_SHOWING_OVERVIEW)
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
            mUiConfig = exploreSurfaceCoordinator.getUiConfig();
        }
    }

    private void updateIncognitoMode(boolean isIncognito) {
        if (isIncognito == mIsIncognito) return;
        mIsIncognito = isIncognito;

        mPropertyModel.set(IS_INCOGNITO, mIsIncognito);
        updateBackgroundColor(mPropertyModel);

        // TODO(crbug.com/40657059): This looks not needed since there is no way to change incognito
        // mode when focusing on the omnibox and incognito mode change won't affect the visibility
        // of the tab switcher toolbar.
        if (mPropertyModel.get(IS_SHOWING_OVERVIEW)) notifyStateChange();
    }

    private void notifyStateChange() {
        notifyShowStateChange();
    }

    private void notifyShowStateChange() {
        // StartSurface is being supplied with OneShotSupplier, notification sends after
        // StartSurface is available to avoid missing events. More detail see:
        // https://crrev.com/c/2427428.
        if (mController != null) {
            mController.onHomepageChanged();
        }
        notifyBackPressStateChanged();
    }

    private boolean hasFakeSearchBox() {
        return isHomepageShown();
    }

    @VisibleForTesting
    public boolean shouldShowTabSwitcherToolbar() {
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
        mPropertyModel.set(TOP_TOOLBAR_PLACEHOLDER_HEIGHT, getTopToolbarPlaceholderHeight());
    }

    private void setTabCardVisibility(boolean isVisible) {
        if (mUseMagicStack) return;

        // If the single tab switcher is shown and the current selected tab is a new tab page, we
        // shouldn't show the tab switcher layout on Start.
        boolean shouldShowTabCard =
                isVisible && !(isSingleTabSwitcher() && isCurrentSelectedTabNtp());

        if (shouldShowTabCard == mPropertyModel.get(IS_TAB_CARD_VISIBLE)) return;

        mPropertyModel.set(IS_TAB_CARD_VISIBLE, shouldShowTabCard);
    }

    private void setMVTilesVisibility(boolean isVisible) {
        if (mInitializeMVTilesRunnable == null) return;
        mPropertyModel.set(MV_TILES_VISIBLE, isVisible);
        if (isVisible && mInitializeMVTilesRunnable != null) mInitializeMVTilesRunnable.run();
    }

    @VisibleForTesting
    void setMagicStackVisibility(boolean isVisible) {
        if (!StartSurfaceConfiguration.useMagicStack()) return;

        assert mModuleDelegateCreator != null;
        if (isVisible) {
            if (mHomeModulesCoordinator == null) {
                mHomeModulesCoordinator =
                        (HomeModulesCoordinator) mModuleDelegateCreator.create(this);
            }
            mHomeModulesCoordinator.show(this::onMagicStackShown);
        }
    }

    private void onMagicStackShown(boolean isVisible) {
        mPropertyModel.set(MAGIC_STACK_VISIBLE, isVisible);
    }

    private void setLogoVisibility(boolean isVisible) {
        if (!mIsSurfacePolishEnabled) return;

        if (isVisible && mLogoCoordinator == null) {
            mLogoCoordinator = initializeLogo();
            if (mIsNativeInitialized) mLogoCoordinator.initWithNative();
        }
        if (mLogoCoordinator != null) {
            mLogoCoordinator.updateVisibility(false);
        }
    }

    private void setFakeBoxVisibility(boolean isVisible) {
        if (mPropertyModel == null) return;
        mPropertyModel.set(IS_FAKE_SEARCH_BOX_VISIBLE, isVisible);

        // This is because VoiceRecognitionHandler monitors incognito mode and returns
        // false in incognito mode. However, when switching incognito mode, this class is notified
        // earlier than the VoiceRecognitionHandler, so isVoiceSearchEnabled returns
        // incorrect state if check synchronously.
        ThreadUtils.postOnUiThread(
                () -> {
                    if (mOmniboxStub != null) {
                        if (mOmniboxStub.getVoiceRecognitionHandler() != null) {
                            mPropertyModel.set(
                                    IS_VOICE_RECOGNITION_BUTTON_VISIBLE,
                                    mOmniboxStub
                                            .getVoiceRecognitionHandler()
                                            .isVoiceSearchEnabled());
                        }
                        mPropertyModel.set(
                                IS_LENS_BUTTON_VISIBLE,
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
    }

    private int getNormalTabCount() {
        if (!mTabModelSelector.isTabStateInitialized()) {
            return ChromeSharedPreferences.getInstance()
                    .readInt(ChromePreferenceKeys.REGULAR_TAB_COUNT);
        } else {
            return mTabModelSelector.getModel(false).getCount();
        }
    }

    private boolean isCurrentSelectedTabNtp() {
        Tab currentTab = mTabModelSelector.getCurrentTab();
        return mTabModelSelector.isTabStateInitialized()
                        && currentTab != null
                        && currentTab.getUrl() != null
                ? UrlUtilities.isNtpUrl(currentTab.getUrl())
                : ChromeSharedPreferences.getInstance()
                                .readInt(
                                        ChromePreferenceKeys.APP_LAUNCH_LAST_KNOWN_ACTIVE_TAB_STATE)
                        == ActiveTabState.NTP;
    }

    private boolean isSingleTabSwitcher() {
        return mController != null && mController.getTabSwitcherType() == TabSwitcherType.SINGLE;
    }

    private void notifyBackPressStateChanged() {
        mBackPressChangedSupplier.set(shouldInterceptBackPress());
    }

    @VisibleForTesting
    boolean shouldInterceptBackPress() {
        if (mController != null && mController.isDialogVisible()) {
            return true;
        }

        if (ReturnToChromeUtil.isStartSurfaceEnabled(mContext)) {
            return false;
        }

        if (mController != null
                && Boolean.TRUE.equals(mController.getHandleBackPressChangedSupplier().get())) {
            return true;
        }

        return false;
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
                        ColorUtils.inNightMode(mContext), mLaunchOrigin);
        mPropertyModel.set(EXPLORE_SURFACE_COORDINATOR, exploreSurfaceCoordinator);
        FeedReliabilityLogger feedReliabilityLogger =
                exploreSurfaceCoordinator.getFeedReliabilityLogger();
        if (feedReliabilityLogger != null) {
            mOmniboxStub.addUrlFocusChangeListener(feedReliabilityLogger);
        }
    }

    private LogoCoordinator initializeLogo() {
        Callback<LoadUrlParams> logoClickedCallback =
                mCallbackController.makeCancelable(
                        (urlParams) -> {
                            // On NTP, the logo is in the new tab page layout instead of the toolbar
                            // and the logo click events are processed in NewTabPageLayout. This
                            // callback passed into TopToolbarCoordinator will only be used for
                            // StartSurfaceToolbar, so add an assertion here.
                            assert ReturnToChromeUtil.isStartSurfaceEnabled(mContext);
                            ReturnToChromeUtil.handleLoadUrlFromStartSurface(
                                    urlParams, /* incognito= */ false, mParentTabSupplier.get());
                        });
        mLogoContainerView.setVisibility(View.VISIBLE);
        LogoView logoView = mLogoContainerView.findViewById(R.id.search_provider_logo);
        if (mIsSurfacePolishEnabled) {
            LogoUtils.setLogoViewLayoutParams(
                    logoView,
                    mContext.getResources(),
                    /* isTablet= */ false,
                    StartSurfaceConfiguration.isLogoPolishEnabled(),
                    StartSurfaceConfiguration.getLogoSizeForLogoPolish());
        }

        mLogoCoordinator =
                new LogoCoordinator(mContext, logoClickedCallback, logoView, true, null, this);
        return mLogoCoordinator;
    }

    FeedReliabilityLogger getFeedReliabilityLogger() {
        if (mPropertyModel == null) return null;
        ExploreSurfaceCoordinator coordinator = mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR);
        return coordinator != null ? coordinator.getFeedReliabilityLogger() : null;
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
        RecordHistogram.recordMediumTimesHistogram(
                "StartSurface.SpareTab.TimeBetweenShowAndCreate",
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

    /** Returns whether the Start surface homepage is showing. */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    boolean isHomepageShown() {
        return mIsHomepageShown;
    }

    @Override
    public int getHostSurfaceType() {
        return HostSurface.START_SURFACE;
    }

    @Override
    public Point getContextMenuStartPoint() {
        return mContextMenuStartPosotion;
    }

    @Nullable
    @Override
    public UiConfig getUiConfig() {
        return mUiConfig;
    }

    @Override
    public void onUrlClicked(GURL gurl) {
        ReturnToChromeUtil.handleLoadUrlFromStartSurface(new LoadUrlParams(gurl), false, null);
    }

    @Override
    public void onTabSelected(int tabId) {
        mOnTabSelectingListener.onTabSelecting(tabId);
    }

    @Override
    public void customizeSettings() {
        HomeModulesConfigManager.getInstance().onMenuClick(mContext, new SettingsLauncherImpl());
    }

    @Override
    public int getStartMargin() {
        return mStartMargin;
    }

    @Override
    public boolean isHomeSurface() {
        return true;
    }

    public FeedActionDelegate getFeedActionDelegateForTesting() {
        assert mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR) != null;
        return mPropertyModel
                .get(EXPLORE_SURFACE_COORDINATOR)
                .getFeedActionDelegateForTesting(); // IN-TEST
    }

    TabSwitcher getTabSwitcherModuleForTesting() {
        return mTabSwitcherModule;
    }

    Controller getControllerForTesting() {
        return mController;
    }

    Runnable getInitializeMVTilesRunnableForTesting() {
        return mInitializeMVTilesRunnable;
    }

    HomeModulesCoordinator getHomeModulesCoordinatorForTesting() {
        return mHomeModulesCoordinator;
    }

    /**
     * Update the background color of the start surface based on whether it is in the incognito mode
     * or non-incognito mode.
     */
    @VisibleForTesting
    void updateBackgroundColor(PropertyModel propertyModel) {
        @ColorInt int surfaceBackgroundColor;
        if (!mIsIncognito) {
            surfaceBackgroundColor =
                    ChromeColors.getSurfaceColor(
                            mContext, R.dimen.home_surface_background_color_elevation);
        } else {
            surfaceBackgroundColor = ChromeColors.getPrimaryBackgroundColor(mContext, mIsIncognito);
        }
        propertyModel.set(BACKGROUND_COLOR, surfaceBackgroundColor);
    }

    void setIsIncognitoForTesting(boolean isIncognito) {
        mIsIncognito = isIncognito;
    }
}
