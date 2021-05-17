// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.banners.AppBannerInProductHelpController;
import org.chromium.chrome.browser.banners.AppBannerInProductHelpControllerFactory;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchContainerCoordinator;
import org.chromium.chrome.browser.datareduction.DataReductionPromoScreen;
import org.chromium.chrome.browser.feed.shared.FeedFeatures;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.feed.webfeed.WebFeedFollowIntroController;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.gesturenav.BackActionDelegate;
import org.chromium.chrome.browser.gesturenav.BackActionDelegate.ActionType;
import org.chromium.chrome.browser.gesturenav.HistoryNavigationCoordinator;
import org.chromium.chrome.browser.gesturenav.NavigationSheet;
import org.chromium.chrome.browser.gesturenav.TabbedSheetDelegate;
import org.chromium.chrome.browser.history.HistoryManagerUtils;
import org.chromium.chrome.browser.language.LanguageAskPrompt;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustMetrics;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustSignalsCoordinator;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.offlinepages.indicator.OfflineIndicatorControllerV2;
import org.chromium.chrome.browser.offlinepages.indicator.OfflineIndicatorInProductHelpController;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.read_later.ReadLaterIPHController;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.signin.SigninActivityLauncherImpl;
import org.chromium.chrome.browser.signin.ui.SigninPromoUtil;
import org.chromium.chrome.browser.status_indicator.StatusIndicatorCoordinator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.toolbar.ToolbarButtonInProductHelpController;
import org.chromium.chrome.browser.toolbar.ToolbarIntentMetadata;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.chrome.browser.ui.TabObscuringHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.chrome.browser.ui.tablet.emptybackground.EmptyBackgroundViewWrapper;
import org.chromium.chrome.browser.version.ChromeVersionInfo;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.chrome.browser.webapps.AddToHomescreenIPHController;
import org.chromium.chrome.browser.webapps.AddToHomescreenMostVisitedTileClickObserver;
import org.chromium.chrome.browser.webapps.PwaBottomSheetController;
import org.chromium.chrome.browser.webapps.PwaBottomSheetControllerFactory;
import org.chromium.chrome.features.start_surface.StartSurface;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.util.ComposedBrowserControlsVisibilityDelegate;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * A {@link RootUiCoordinator} variant that controls tabbed-mode specific UI.
 */
public class TabbedRootUiCoordinator extends RootUiCoordinator {
    private static boolean sEnableStatusIndicatorForTests;

    private final ObservableSupplierImpl<EphemeralTabCoordinator> mEphemeralTabCoordinatorSupplier;
    private TabbedSystemUiCoordinator mSystemUiCoordinator;
    private @Nullable EmptyBackgroundViewWrapper mEmptyBackgroundViewWrapper;

    private StatusIndicatorCoordinator mStatusIndicatorCoordinator;
    private StatusIndicatorCoordinator.StatusIndicatorObserver mStatusIndicatorObserver;
    private OfflineIndicatorControllerV2 mOfflineIndicatorController;
    private OfflineIndicatorInProductHelpController mOfflineIndicatorInProductHelpController;
    private ReadLaterIPHController mReadLaterIPHController;
    private WebFeedFollowIntroController mWebFeedFollowIntroController;
    private UrlFocusChangeListener mUrlFocusChangeListener;
    private @Nullable ToolbarButtonInProductHelpController mToolbarButtonInProductHelpController;
    private AddToHomescreenIPHController mAddToHomescreenIPHController;
    private AddToHomescreenMostVisitedTileClickObserver mAddToHomescreenMostVisitedTileObserver;
    private AppBannerInProductHelpController mAppBannerInProductHelpController;
    private PwaBottomSheetController mPwaBottomSheetController;
    private HistoryNavigationCoordinator mHistoryNavigationCoordinator;
    private NavigationSheet mNavigationSheet;
    private ComposedBrowserControlsVisibilityDelegate mAppBrowserControlsVisibilityDelegate;
    private LayoutManagerImpl mLayoutManager;
    private ObservableSupplierImpl<Tab> mTabSupplier;
    private ActivityTabTabObserver mTabObserver;
    private ContinuousSearchContainerCoordinator mContinuousSearchContainerCoordinator;
    private Callback<Integer> mContinuousSearchObserver;
    private TabObscuringHandler.Observer mContinuousSearchTabObscuringHandlerObserver;
    private MerchantTrustSignalsCoordinator mMerchantTrustSignalsCoordinator;

    private int mStatusIndicatorHeight;
    private int mContinuousSearchHeight;

    /**
     * Construct a new TabbedRootUiCoordinator.
     * @param activity The activity whose UI the coordinator is responsible for.
     * @param onOmniboxFocusChangedListener callback to invoke when Omnibox focus
     *         changes.
     * @param intentMetadataOneshotSupplier Supplier with information about the launching intent.
     * @param shareDelegateSupplier
     * @param tabProvider The {@link ActivityTabProvider} to get current tab of the activity.
     * @param profileSupplier Supplier of the currently applicable profile.
     * @param bookmarkBridgeSupplier Supplier of the bookmark bridge for the current profile.
     * @param overviewModeBehaviorSupplier Supplier of the overview mode manager.
     * @param contextualSearchManagerSupplier Supplier of the {@link ContextualSearchManager}.
     * @param startSurfaceSupplier Supplier of the {@link StartSurface}.
     * @param layoutStateProviderOneshotSupplier Supplier of the {@link LayoutStateProvider}.
     */
    public TabbedRootUiCoordinator(ChromeActivity activity,
            Callback<Boolean> onOmniboxFocusChangedListener,
            OneshotSupplier<ToolbarIntentMetadata> intentMetadataOneshotSupplier,
            ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            ActivityTabProvider tabProvider,
            ObservableSupplierImpl<EphemeralTabCoordinator> ephemeralTabCoordinatorSupplier,
            ObservableSupplier<Profile> profileSupplier,
            ObservableSupplier<BookmarkBridge> bookmarkBridgeSupplier,
            OneshotSupplier<OverviewModeBehavior> overviewModeBehaviorSupplier,
            Supplier<ContextualSearchManager> contextualSearchManagerSupplier,
            ObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            OneshotSupplier<StartSurface> startSurfaceSupplier,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderOneshotSupplier,
            Supplier<Tab> startSurfaceParentTabSupplier) {
        super(activity, onOmniboxFocusChangedListener, shareDelegateSupplier, tabProvider,
                profileSupplier, bookmarkBridgeSupplier, contextualSearchManagerSupplier,
                tabModelSelectorSupplier, startSurfaceSupplier, intentMetadataOneshotSupplier,
                layoutStateProviderOneshotSupplier, startSurfaceParentTabSupplier);
        mEphemeralTabCoordinatorSupplier = ephemeralTabCoordinatorSupplier;
        mCanAnimateBrowserControls = () -> {
            // These null checks prevent any exceptions that may be caused by callbacks after
            // destruction.
            if (mActivity == null || mActivity.getActivityTabProvider() == null) return false;
            final Tab tab = mActivity.getActivityTabProvider().get();
            return tab != null && tab.isUserInteractable() && !tab.isNativePage();
        };
    }

    @Override
    public void destroy() {
        if (mSystemUiCoordinator != null) mSystemUiCoordinator.destroy();
        if (mEmptyBackgroundViewWrapper != null) mEmptyBackgroundViewWrapper.destroy();

        if (mOfflineIndicatorController != null) {
            mOfflineIndicatorController.destroy();
        }

        if (mToolbarManager != null) {
            mToolbarManager.getOmniboxStub().removeUrlFocusChangeListener(mUrlFocusChangeListener);
        }

        if (mOfflineIndicatorInProductHelpController != null) {
            mOfflineIndicatorInProductHelpController.destroy();
        }
        if (mStatusIndicatorCoordinator != null) {
            mStatusIndicatorCoordinator.removeObserver(mStatusIndicatorObserver);
            mStatusIndicatorCoordinator.removeObserver(mActivity.getStatusBarColorController());
            mStatusIndicatorCoordinator.destroy();
        }

        if (mToolbarButtonInProductHelpController != null) {
            mToolbarButtonInProductHelpController.destroy();
        }

        if (mWebFeedFollowIntroController != null) {
            mWebFeedFollowIntroController.destroy();
        }

        if (mTabObserver != null) {
            mTabObserver.destroy();
        }

        if (mAppBannerInProductHelpController != null) {
            AppBannerInProductHelpControllerFactory.detach(mAppBannerInProductHelpController);
        }

        if (mPwaBottomSheetController != null) {
            PwaBottomSheetControllerFactory.detach(mPwaBottomSheetController);
        }

        if (mHistoryNavigationCoordinator != null) {
            mHistoryNavigationCoordinator.destroy();
            mHistoryNavigationCoordinator = null;
        }

        if (mContinuousSearchContainerCoordinator != null) {
            getTabObscuringHandler().removeObserver(mContinuousSearchTabObscuringHandlerObserver);
            mContinuousSearchContainerCoordinator.removeHeightObserver(mContinuousSearchObserver);
            mContinuousSearchContainerCoordinator.destroy();
            mContinuousSearchContainerCoordinator = null;
            mContinuousSearchObserver = null;
            mContinuousSearchTabObscuringHandlerObserver = null;
        }

        if (mMerchantTrustSignalsCoordinator != null) {
            mMerchantTrustSignalsCoordinator.destroy();
            mMerchantTrustSignalsCoordinator = null;
        }

        super.destroy();
    }

    @Override
    public void onPostInflationStartup() {
        super.onPostInflationStartup();

        mSystemUiCoordinator = new TabbedSystemUiCoordinator(mActivity.getWindow(),
                mActivity.getTabModelSelector(), mActivity.getOverviewModeBehaviorSupplier());
    }

    @Override
    protected void onFindToolbarShown() {
        super.onFindToolbarShown();
        EphemeralTabCoordinator coordinator = mEphemeralTabCoordinatorSupplier.get();
        if (coordinator != null && coordinator.isOpened()) coordinator.close();
    }

    /**
     * @return The toolbar button IPH controller for the tabbed UI this coordinator controls.
     * TODO(pnoland, https://crbug.com/865801): remove this in favor of wiring it directly.
     */
    public ToolbarButtonInProductHelpController getToolbarButtonInProductHelpController() {
        return mToolbarButtonInProductHelpController;
    }

    /**
     * Show navigation history sheet.
     */
    public void showFullHistorySheet() {
        Tab tab = mActivity.getActivityTabProvider().get();
        if (tab == null || tab.getWebContents() == null || !tab.isUserInteractable()) return;
        Profile profile = Profile.fromWebContents(tab.getWebContents());
        mNavigationSheet = NavigationSheet.create(
                mActivity.getWindow().getDecorView().findViewById(android.R.id.content), mActivity,
                this::getBottomSheetController, profile);
        mNavigationSheet.setDelegate(new TabbedSheetDelegate(tab, aTab -> {
            HistoryManagerUtils.showHistoryManager(
                    mActivity, aTab, mActivity.getTabModelSelector().isIncognitoSelected());
        }, mActivity.getResources().getString(R.string.show_full_history)));
        if (!mNavigationSheet.startAndExpand(/* forward=*/false, /* animate=*/true)) {
            mNavigationSheet = null;
        } else {
            getBottomSheetController().addObserver(new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(int reason) {
                    getBottomSheetController().removeObserver(this);
                    mNavigationSheet = null;
                }
            });
        }
    }

    @Override
    public void onFinishNativeInitialization() {
        super.onFinishNativeInitialization();
        assert mLayoutManager != null;
        mHistoryNavigationCoordinator = HistoryNavigationCoordinator.create(
                mActivity.getWindowAndroid(), mActivity.getLifecycleDispatcher(),
                mActivity.getCompositorViewHolder(), mActivity.getActivityTabProvider(),
                mActivity.getInsetObserverView(), new BackActionDelegate() {
                    @Override
                    public @ActionType int getBackActionType(Tab tab) {
                        if (tab.canGoBack()) return ActionType.NAVIGATE_BACK;
                        if (TabAssociatedApp.isOpenedFromExternalApp(tab)) {
                            return ActionType.EXIT_APP;
                        }
                        return mActivity.backShouldCloseTab(tab) ? ActionType.CLOSE_TAB
                                                                 : ActionType.EXIT_APP;
                    }

                    @Override
                    public void onBackGesture() {
                        // Back navigation gesture performs what the back button would do.
                        mActivity.onBackPressed();
                    }
                }, mLayoutManager);

        // TODO(twellington): Supply TabModelSelector as well and move initialization earlier.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            AppMenuHandler appMenuHandler =
                    mAppMenuCoordinator == null ? null : mAppMenuCoordinator.getAppMenuHandler();
            mEmptyBackgroundViewWrapper = new EmptyBackgroundViewWrapper(
                    mActivity.getTabModelSelector(), mActivity.getTabCreator(false), mActivity,
                    appMenuHandler, mActivity.getSnackbarManager(),
                    mActivity.getOverviewModeBehaviorSupplier());
            mEmptyBackgroundViewWrapper.initialize();
        }

        if (!mActivity.isTablet()
                && (TabUiFeatureUtilities.isTabGroupsAndroidEnabled()
                        || TabUiFeatureUtilities.isConditionalTabStripEnabled())) {
            getToolbarManager().enableBottomControls();
        }

        if (EphemeralTabCoordinator.isSupported()) {
            mEphemeralTabCoordinatorSupplier.set(new EphemeralTabCoordinator(mActivity,
                    mActivity.getWindowAndroid(), mActivity.getWindow().getDecorView(),
                    mActivity.getActivityTabProvider(), mActivity::getCurrentTabCreator,
                    getBottomSheetController(), true));
        }

        initializeTabSupplier();
        mIntentMetadataOneshotSupplier.onAvailable(mCallbackController.makeCancelable(
                (metadata) -> initializeIPH(metadata.getIsIntentWithEffect())));

        // TODO(https://crbug.com/1157955): Investigate switching to per-Activity coordinator that
        // uses signals from the current Tab to decide when to show the PWA install bottom sheet
        // rather than relying on unowned user data.
        mPwaBottomSheetController =
                PwaBottomSheetControllerFactory.createPwaBottomSheetController(mActivity);
        PwaBottomSheetControllerFactory.attach(
                mActivity.getWindowAndroid(), mPwaBottomSheetController);
        initContinuousSearchCoordinator();

        initMerchantTrustSignals();
    }

    private void initMerchantTrustSignals() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.COMMERCE_MERCHANT_VIEWER)) {
            return;
        }

        mMerchantTrustSignalsCoordinator = new MerchantTrustSignalsCoordinator(mActivity,
                mActivity.getWindowAndroid(), getBottomSheetController(),
                mActivity.getWindow().getDecorView(), mActivity.getTabModelSelector(),
                MessageDispatcherProvider.from(mActivity.getWindowAndroid()),
                mActivity.getActivityTabProvider(), mProfileSupplier, new MerchantTrustMetrics());
    }

    // Protected class methods

    @Override
    protected void onLayoutManagerAvailable(LayoutManagerImpl layoutManager) {
        super.onLayoutManagerAvailable(layoutManager);

        initStatusIndicatorCoordinator(layoutManager);
        mLayoutManager = layoutManager;
    }

    @Override
    protected boolean shouldShowMenuUpdateBadge() {
        return true;
    }

    @Override
    protected ScrimCoordinator buildScrimWidget() {
        ViewGroup coordinator = mActivity.findViewById(R.id.coordinator);
        ScrimCoordinator.SystemUiScrimDelegate delegate =
                new ScrimCoordinator.SystemUiScrimDelegate() {
                    @Override
                    public void setStatusBarScrimFraction(float scrimFraction) {
                        mActivity.getStatusBarColorController().setStatusBarScrimFraction(
                                scrimFraction);
                    }

                    @Override
                    public void setNavigationBarScrimFraction(float scrimFraction) {
                        TabbedNavigationBarColorController controller =
                                mSystemUiCoordinator.getNavigationBarColorController();
                        if (controller == null) {
                            return;
                        }
                        controller.setNavigationBarScrimFraction(scrimFraction);
                    }
                };
        return new ScrimCoordinator(mActivity, delegate, coordinator,
                ApiCompatibilityUtils.getColor(coordinator.getResources(),
                        R.color.omnibox_focused_fading_background_color));
    }

    // Private class methods

    private void initializeTabSupplier() {
        mTabSupplier = new ObservableSupplierImpl<>();
        ActivityTabProvider activityTabProvider = mActivity.getActivityTabProvider();
        mTabObserver = new ActivityTabTabObserver(activityTabProvider) {
            @Override
            public void onObservingDifferentTab(Tab tab, boolean hint) {
                mTabSupplier.set(tab);
            }
        };
        mTabSupplier.set(activityTabProvider.get());
    }

    private void initializeIPH(boolean intentWithEffect) {
        if (mActivity == null) return;
        mToolbarButtonInProductHelpController = new ToolbarButtonInProductHelpController(mActivity,
                mActivity.getWindowAndroid(), mAppMenuCoordinator,
                mActivity.getLifecycleDispatcher(), mTabSupplier, mActivity::isInOverviewMode,
                mToolbarManager.getMenuButtonView(), mToolbarManager.getSecurityIconView());
        mReadLaterIPHController = new ReadLaterIPHController(mActivity,
                getToolbarManager().getMenuButtonView(), mAppMenuCoordinator.getAppMenuHandler());

        boolean didTriggerPromo = triggerPromo(intentWithEffect);
        if (!didTriggerPromo) {
            mToolbarButtonInProductHelpController.showColdStartIPH();
            mReadLaterIPHController.showColdStartIPH();
        }
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.TOOLBAR_IPH_ANDROID)) {
            mPromoShownOneshotSupplier.set(didTriggerPromo);
        }

        if (mOfflineIndicatorController != null) {
            // Initialize the OfflineIndicatorInProductHelpController if the
            // mOfflineIndicatorController is enabled and initialized. For example, it wouldn't be
            // initialized if the OfflineIndicatorV2 feature is disabled.
            assert mOfflineIndicatorInProductHelpController == null;
            mOfflineIndicatorInProductHelpController =
                    new OfflineIndicatorInProductHelpController(mActivity, mToolbarManager,
                            mAppMenuCoordinator.getAppMenuHandler(), mStatusIndicatorCoordinator);
        }

        mAddToHomescreenIPHController = new AddToHomescreenIPHController(mActivity,
                mActivity.getWindowAndroid(), mActivity.getModalDialogManager(),
                mAppMenuCoordinator.getAppMenuHandler(), R.id.add_to_homescreen_id,
                ()
                        -> mActivity.getToolbarManager().getMenuButtonView(),
                MessageDispatcherProvider.from(mActivity.getWindowAndroid()));
        mAddToHomescreenMostVisitedTileObserver = new AddToHomescreenMostVisitedTileClickObserver(
                mTabSupplier, mAddToHomescreenIPHController);
        mAppBannerInProductHelpController =
                AppBannerInProductHelpControllerFactory.createAppBannerInProductHelpController(
                        mActivity, mAppMenuCoordinator.getAppMenuHandler(),
                        ()
                                -> mActivity.getToolbarManager().getMenuButtonView(),
                        R.id.add_to_homescreen_id);
        AppBannerInProductHelpControllerFactory.attach(
                mActivity.getWindowAndroid(), mAppBannerInProductHelpController);

        if (FeedFeatures.isWebFeedUIEnabled()) {
            mWebFeedFollowIntroController = new WebFeedFollowIntroController(mActivity,
                    mTabSupplier, mToolbarManager.getMenuButtonView(),
                    mActivity.getSnackbarManager(), new WebFeedBridge());
        }
    }

    private void updateTopControlsHeight() {
        final BrowserControlsSizer browserControlsSizer = mActivity.getBrowserControlsManager();
        final int resourceId = mActivity.getControlContainerHeightResource();
        final int topControlsNewHeight = mActivity.getResources().getDimensionPixelSize(resourceId)
                + mStatusIndicatorHeight + mContinuousSearchHeight;

        browserControlsSizer.setAnimateBrowserControlsHeightChanges(true);
        browserControlsSizer.setTopControlsHeight(topControlsNewHeight, mStatusIndicatorHeight);
        browserControlsSizer.setAnimateBrowserControlsHeightChanges(false);
    }

    private void initStatusIndicatorCoordinator(LayoutManagerImpl layoutManager) {
        // TODO(crbug.com/1035584): Disable on tablets for now as we need to do one or two extra
        // things for tablets.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)
                || (!ChromeFeatureList.isEnabled(ChromeFeatureList.OFFLINE_INDICATOR_V2)
                        && !sEnableStatusIndicatorForTests)) {
            return;
        }

        final BrowserControlsSizer browserControlsSizer = mActivity.getBrowserControlsManager();
        mStatusIndicatorCoordinator = new StatusIndicatorCoordinator(mActivity,
                mActivity.getCompositorViewHolder().getResourceManager(), browserControlsSizer,
                mActivity.getStatusBarColorController()::getStatusBarColorWithoutStatusIndicator,
                mCanAnimateBrowserControls, layoutManager::requestUpdate);
        layoutManager.addSceneOverlay(mStatusIndicatorCoordinator.getSceneLayer());
        mStatusIndicatorObserver = new StatusIndicatorCoordinator.StatusIndicatorObserver() {
            @Override
            public void onStatusIndicatorHeightChanged(int indicatorHeight) {
                mStatusIndicatorHeight = indicatorHeight;
                updateTopControlsHeight();
            }
        };
        mStatusIndicatorCoordinator.addObserver(mStatusIndicatorObserver);
        mStatusIndicatorCoordinator.addObserver(mActivity.getStatusBarColorController());

        // Don't initialize the offline indicator controller if the feature is disabled.
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.OFFLINE_INDICATOR_V2)) {
            return;
        }

        ObservableSupplierImpl<Boolean> isUrlBarFocusedSupplier = new ObservableSupplierImpl<>();
        isUrlBarFocusedSupplier.set(mToolbarManager.isUrlBarFocused());
        mUrlFocusChangeListener = new UrlFocusChangeListener() {
            @Override
            public void onUrlFocusChange(boolean hasFocus) {
                // Offline indicator should assume the UrlBar is focused if it's focusing.
                if (hasFocus) {
                    isUrlBarFocusedSupplier.set(true);
                }
            }

            @Override
            public void onUrlAnimationFinished(boolean hasFocus) {
                // Wait for the animation to finish before notifying that UrlBar is unfocused.
                if (!hasFocus) {
                    isUrlBarFocusedSupplier.set(false);
                }
            }
        };
        mOfflineIndicatorController = new OfflineIndicatorControllerV2(mActivity,
                mStatusIndicatorCoordinator, isUrlBarFocusedSupplier, mCanAnimateBrowserControls);
        if (mToolbarManager.getOmniboxStub() != null) {
            mToolbarManager.getOmniboxStub().addUrlFocusChangeListener(mUrlFocusChangeListener);
        }
    }

    private void initContinuousSearchCoordinator() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CONTINUOUS_SEARCH)) {
            return;
        }

        Supplier<Integer> defaultTopContainerHeightSupplier = ()
                -> mActivity.getResources().getDimensionPixelSize(
                        mActivity.getControlContainerHeightResource());
        final ViewStub viewStub = mActivity.findViewById(R.id.continuous_search_container_stub);
        final BrowserControlsSizer browserControlsSizer = mActivity.getBrowserControlsManager();
        mContinuousSearchContainerCoordinator = new ContinuousSearchContainerCoordinator(viewStub,
                mLayoutManager, mActivity.getCompositorViewHolder().getResourceManager(),
                mTabSupplier, browserControlsSizer, mCanAnimateBrowserControls,
                defaultTopContainerHeightSupplier, getTopUiThemeColorProvider(),
                mActivity.getResources());
        mContinuousSearchObserver = newHeight -> {
            mContinuousSearchHeight = newHeight;
            updateTopControlsHeight();
        };
        mContinuousSearchContainerCoordinator.addHeightObserver(mContinuousSearchObserver);
        mContinuousSearchTabObscuringHandlerObserver =
                isObscured -> mContinuousSearchContainerCoordinator.updateTabObscured(isObscured);
        getTabObscuringHandler().addObserver(mContinuousSearchTabObscuringHandlerObserver);
    }

    @Override
    protected BrowserControlsManager createBrowserControlsManager() {
        BrowserControlsManager manager = super.createBrowserControlsManager();
        getAppBrowserControlsVisibilityDelegate().addDelegate(
                manager.getBrowserVisibilityDelegate());
        return manager;
    }

    /**
     * @return {@link ComposedBrowserControlsVisibilityDelegate} object for tabbed activity.
     */
    public ComposedBrowserControlsVisibilityDelegate getAppBrowserControlsVisibilityDelegate() {
        if (mAppBrowserControlsVisibilityDelegate == null) {
            mAppBrowserControlsVisibilityDelegate = new ComposedBrowserControlsVisibilityDelegate();
        }
        return mAppBrowserControlsVisibilityDelegate;
    }

    @VisibleForTesting
    public StatusIndicatorCoordinator getStatusIndicatorCoordinatorForTesting() {
        return mStatusIndicatorCoordinator;
    }

    @VisibleForTesting
    public static void setEnableStatusIndicatorForTests(boolean disable) {
        sEnableStatusIndicatorForTests = disable;
    }

    @VisibleForTesting
    public EphemeralTabCoordinator getEphemeralTabCoordinatorForTesting() {
        return mEphemeralTabCoordinatorSupplier.get();
    }

    @VisibleForTesting
    public HistoryNavigationCoordinator getHistoryNavigationCoordinatorForTesting() {
        return mHistoryNavigationCoordinator;
    }

    @VisibleForTesting
    public NavigationSheet getNavigationSheetForTesting() {
        return mNavigationSheet;
    }

    /** Called when a link is copied through context menu. */
    public void onContextMenuCopyLink() {
        // TODO(crbug/1150090): Find a better way of passing event for IPH.
        mReadLaterIPHController.onCopyContextMenuItemClicked();
    }

    /**
     * Triggers the display of an appropriate promo, if any, returning true if a promo is actually
     * displayed.
     */
    private boolean triggerPromo(boolean intentWithEffect) {
        try (TraceEvent e = TraceEvent.scoped("TabbedRootUiCoordinator.triggerPromo")) {
            if (CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_STARTUP_PROMOS)) {
                return false;
            }

            SharedPreferencesManager preferenceManager = SharedPreferencesManager.getInstance();
            // Promos can only be shown when we start with ACTION_MAIN intent and
            // after FRE is complete. Native initialization can finish before the FRE flow is
            // complete, and this will only show promos on the second opportunity. This is
            // because the FRE is shown on the first opportunity, and we don't want to show such
            // content back to back.
            //
            // TODO(https://crbug.com/865801, pnoland): Unify promo dialog logic and move into a
            // single PromoDialogCoordinator.
            boolean isShowingPromo =
                    LocaleManager.getInstance().hasShownSearchEnginePromoThisSession();
            // Promo dialogs in multiwindow mode are broken on some devices:
            // http://crbug.com/354696
            boolean isLegacyMultiWindow =
                    MultiWindowUtils.getInstance().isLegacyMultiWindow(mActivity);
            if (!isShowingPromo && !intentWithEffect && FirstRunStatus.getFirstRunFlowComplete()
                    && preferenceManager.readBoolean(
                            ChromePreferenceKeys.PROMOS_SKIPPED_ON_FIRST_START, false)
                    && !VrModuleProvider.getDelegate().isInVr()
                    // VrModuleProvider.getDelegate().isInVr may not return true at this point
                    // even though Chrome is about to enter VR, so we need to also check whether
                    // we're launching into VR.
                    && !VrModuleProvider.getIntentDelegate().isLaunchingIntoVr(
                            mActivity, mActivity.getIntent())
                    && !isLegacyMultiWindow) {
                isShowingPromo = maybeShowPromo();
            } else {
                preferenceManager.writeBoolean(
                        ChromePreferenceKeys.PROMOS_SKIPPED_ON_FIRST_START, true);
            }
            return isShowingPromo;
        }
    }

    private boolean maybeShowPromo() {
        // Only one promo can be shown in one run to avoid nagging users too much.
        if (SigninPromoUtil.launchSigninPromoIfNeeded(mActivity, SigninActivityLauncherImpl.get(),
                    ChromeVersionInfo.getProductMajorVersion())) {
            return true;
        }
        if (DataReductionPromoScreen.launchDataReductionPromo(
                    mActivity, mActivity.getTabModelSelector().getCurrentModel().isIncognito())) {
            return true;
        }
        if (DefaultBrowserPromoUtils.prepareLaunchPromoIfNeeded(
                    mActivity, mActivity.getWindowAndroid())) {
            return true;
        }

        return LanguageAskPrompt.maybeShowLanguageAskPrompt(
                mActivity, mActivity.getModalDialogManagerSupplier());
    }
}
