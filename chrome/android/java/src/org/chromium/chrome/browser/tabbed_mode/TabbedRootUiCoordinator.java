// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import android.graphics.Color;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AppCompatActivity;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.Function;
import org.chromium.base.TraceEvent;
import org.chromium.base.jank_tracker.JankTracker;
import org.chromium.base.supplier.BooleanSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.SwipeRefreshHandler;
import org.chromium.chrome.browser.app.tab_activity_glue.TabReparentingController;
import org.chromium.chrome.browser.banners.AppBannerInProductHelpController;
import org.chromium.chrome.browser.banners.AppBannerInProductHelpControllerFactory;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchContainerCoordinator;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchContainerCoordinator.HeightObserver;
import org.chromium.chrome.browser.feature_guide.notifications.FeatureNotificationUtils;
import org.chromium.chrome.browser.feature_guide.notifications.FeatureType;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.gesturenav.BackActionDelegate;
import org.chromium.chrome.browser.gesturenav.HistoryNavigationCoordinator;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.offlinepages.indicator.OfflineIndicatorControllerV2;
import org.chromium.chrome.browser.offlinepages.indicator.OfflineIndicatorInProductHelpController;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.crow.CrowButtonDelegateImpl;
import org.chromium.chrome.browser.share.crow.CrowIphController;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextIPHController;
import org.chromium.chrome.browser.status_indicator.StatusIndicatorCoordinator;
import org.chromium.chrome.browser.subscriptions.CommerceSubscriptionsService;
import org.chromium.chrome.browser.subscriptions.CommerceSubscriptionsServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.PriceTrackingUtilities;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.tasks.tab_management.UndoGroupSnackbarController;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.chrome.browser.ui.TabObscuringHandler;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.system.StatusBarColorController.StatusBarColorProvider;
import org.chromium.chrome.browser.ui.tablet.emptybackground.EmptyBackgroundViewWrapper;
import org.chromium.chrome.browser.webapps.AddToHomescreenIPHController;
import org.chromium.chrome.features.start_surface.StartSurface;
import org.chromium.chrome.features.start_surface.StartSurfaceUserData;
import org.chromium.components.browser_ui.util.ComposedBrowserControlsVisibilityDelegate;
import org.chromium.components.browser_ui.widget.InsetObserverView;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.TouchEventObserver;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.webapps.bottomsheet.PwaBottomSheetController;
import org.chromium.components.webapps.bottomsheet.PwaBottomSheetControllerFactory;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * A {@link RootUiCoordinator} variant that controls tabbed-mode specific UI.
 */
public class TabbedRootUiCoordinator extends RootUiCoordinator {
    private final ObservableSupplierImpl<EphemeralTabCoordinator> mEphemeralTabCoordinatorSupplier;
    private final RootUiTabObserver mRootUiTabObserver;
    private TabbedSystemUiCoordinator mSystemUiCoordinator;
    private @Nullable EmptyBackgroundViewWrapper mEmptyBackgroundViewWrapper;

    private StatusIndicatorCoordinator mStatusIndicatorCoordinator;
    private StatusIndicatorCoordinator.StatusIndicatorObserver mStatusIndicatorObserver;
    private OfflineIndicatorControllerV2 mOfflineIndicatorController;
    private OfflineIndicatorInProductHelpController mOfflineIndicatorInProductHelpController;
    private AddToHomescreenIPHController mAddToHomescreenIPHController;
    private LinkToTextIPHController mLinkToTextIPHController;
    private CrowIphController mCrowIphController;
    private AppBannerInProductHelpController mAppBannerInProductHelpController;
    private PwaBottomSheetController mPwaBottomSheetController;
    private HistoryNavigationCoordinator mHistoryNavigationCoordinator;
    private ComposedBrowserControlsVisibilityDelegate mAppBrowserControlsVisibilityDelegate;
    private LayoutManagerImpl mLayoutManager;
    private ContinuousSearchContainerCoordinator mContinuousSearchContainerCoordinator;
    private HeightObserver mContinuousSearchObserver;
    private TabObscuringHandler.Observer mContinuousSearchTabObscuringHandlerObserver;
    private CommerceSubscriptionsService mCommerceSubscriptionsService;
    private UndoGroupSnackbarController mUndoGroupSnackbarController;
    private final Supplier<InsetObserverView> mInsetObserverViewSupplier;
    private final Function<Tab, Boolean> mBackButtonShouldCloseTabFn;
    private LayoutStateProvider.LayoutStateObserver mGestureNavLayoutObserver;

    private int mStatusIndicatorHeight;
    private int mContinuousSearchHeight;

    // Activity tab observer that updates the current tab used by various UI components.
    private class RootUiTabObserver extends ActivityTabTabObserver {
        private Tab mTab;

        private RootUiTabObserver(ActivityTabProvider activityTabProvider) {
            super(activityTabProvider);
        }

        @Override
        public void onObservingDifferentTab(Tab tab, boolean hint) {
            swapToTab(tab);
        }

        private void swapToTab(Tab tab) {
            if (mTab != null && !mTab.isDestroyed()) {
                SwipeRefreshHandler.from(mTab).setNavigationCoordinator(null);
            }
            mTab = tab;

            if (tab != null) {
                SwipeRefreshHandler.from(tab).setNavigationCoordinator(
                        mHistoryNavigationCoordinator);
            }
        }

        @Override
        public void destroy() {
            if (mLayoutStateProvider != null && mGestureNavLayoutObserver != null) {
                mLayoutStateProvider.removeObserver(mGestureNavLayoutObserver);
            }
            super.destroy();
            swapToTab(null);
        }
    }

    /**
     * Construct a new TabbedRootUiCoordinator.
     * @param activity The activity whose UI the coordinator is responsible for.
     * @param onOmniboxFocusChangedListener callback to invoke when Omnibox focus
     *         changes.
     * @param shareDelegateSupplier Supplies the {@link ShareDelegate}.
     * @param tabProvider The {@link ActivityTabProvider} to get current tab of the activity.
     * @param profileSupplier Supplier of the currently applicable profile.
     * @param contextualSearchManagerSupplier Supplier of the {@link ContextualSearchManager}.
     * @param tabModelSelectorSupplier Supplies the {@link TabModelSelector}.
     * @param startSurfaceSupplier Supplier of the {@link StartSurface}.
     * @param startSurfaceParentTabSupplier Supplies the parent tab for the StartSurface.
     * @param browserControlsManager Manages the browser controls.
     * @param windowAndroid The current {@link WindowAndroid}.
     * @param jankTracker Tracks the jank in the app.
     * @param activityLifecycleDispatcher Allows observation of the activity lifecycle.
     * @param layoutManagerSupplier Supplies the {@link LayoutManager}.
     * @param menuOrKeyboardActionController Controls the menu or keyboard action controller.
     * @param activityThemeColorSupplier Supplies the activity color theme.
     * @param modalDialogManagerSupplier Supplies the {@link ModalDialogManager}.
     * @param supportsAppMenuSupplier Supplies the support state for the app menu.
     * @param supportsFindInPage Supplies the support state for find in page.
     * @param tabCreatorManagerSupplier Supplies the {@link TabCreatorManager}.
     * @param fullscreenManager Manages the fullscreen state.
     * @param compositorViewHolderSupplier Supplies the {@link CompositorViewHolder}.
     * @param tabContentManagerSupplier Supplies the {@link TabContentManager}.
     * @param snackbarManagerSupplier Supplies the {@link SnackbarManager}.
     * @param activityType The {@link ActivityType} for the activity.
     * @param isInOverviewModeSupplier Supplies whether the app is in overview mode.
     * @param isWarmOnResumeSupplier Supplies whether the app was warm on resume.
     * @param statusBarColorProvider Provides the status bar color.
     * @param ephemeralTabCoordinatorSupplier Supplies the {@link EphemeralTabCoordinator}.
     * @param intentRequestTracker Tracks intent requests.
     * @param insetObserverViewSupplier Supplier for the {@link InsetObserverView}.
     * @param backButtonShouldCloseTabFn Function which supplies whether or not the back button
     *         should close the tab.
     * @param tabReparentingControllerSupplier Supplier for the {@link TabReparentingController}.
     * @param initializeUiWithIncognitoColors Whether to initialize the UI with incognito colors.
     */
    public TabbedRootUiCoordinator(@NonNull AppCompatActivity activity,
            @Nullable Callback<Boolean> onOmniboxFocusChangedListener,
            @NonNull ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            @NonNull ActivityTabProvider tabProvider,
            @NonNull ObservableSupplier<Profile> profileSupplier,
            @NonNull Supplier<ContextualSearchManager> contextualSearchManagerSupplier,
            @NonNull ObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            @NonNull OneshotSupplier<StartSurface> startSurfaceSupplier,
            @NonNull Supplier<Tab> startSurfaceParentTabSupplier,
            @NonNull BrowserControlsManager browserControlsManager,
            @NonNull ActivityWindowAndroid windowAndroid, @NonNull JankTracker jankTracker,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull ObservableSupplier<LayoutManagerImpl> layoutManagerSupplier,
            @NonNull MenuOrKeyboardActionController menuOrKeyboardActionController,
            @NonNull Supplier<Integer> activityThemeColorSupplier,
            @NonNull ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            @NonNull BooleanSupplier supportsAppMenuSupplier,
            @NonNull BooleanSupplier supportsFindInPage,
            @NonNull Supplier<TabCreatorManager> tabCreatorManagerSupplier,
            @NonNull FullscreenManager fullscreenManager,
            @NonNull Supplier<CompositorViewHolder> compositorViewHolderSupplier,
            @NonNull Supplier<TabContentManager> tabContentManagerSupplier,
            @NonNull Supplier<SnackbarManager> snackbarManagerSupplier,
            @ActivityType int activityType, @NonNull Supplier<Boolean> isInOverviewModeSupplier,
            @NonNull Supplier<Boolean> isWarmOnResumeSupplier,
            @NonNull StatusBarColorProvider statusBarColorProvider,
            @NonNull ObservableSupplierImpl<EphemeralTabCoordinator>
                    ephemeralTabCoordinatorSupplier,
            @NonNull IntentRequestTracker intentRequestTracker,
            @NonNull Supplier<InsetObserverView> insetObserverViewSupplier,
            @NonNull Function<Tab, Boolean> backButtonShouldCloseTabFn,
            OneshotSupplier<TabReparentingController> tabReparentingControllerSupplier,
            boolean initializeUiWithIncognitoColors) {
        super(activity, onOmniboxFocusChangedListener, shareDelegateSupplier, tabProvider,
                profileSupplier, contextualSearchManagerSupplier,
                tabModelSelectorSupplier, startSurfaceParentTabSupplier,
                browserControlsManager, windowAndroid, jankTracker, activityLifecycleDispatcher,
                layoutManagerSupplier, menuOrKeyboardActionController, activityThemeColorSupplier,
                modalDialogManagerSupplier, supportsAppMenuSupplier,
                supportsFindInPage, tabCreatorManagerSupplier, fullscreenManager,
                compositorViewHolderSupplier, tabContentManagerSupplier, snackbarManagerSupplier,
                activityType, isInOverviewModeSupplier, isWarmOnResumeSupplier,
                statusBarColorProvider, intentRequestTracker, tabReparentingControllerSupplier,
                initializeUiWithIncognitoColors);
        mEphemeralTabCoordinatorSupplier = ephemeralTabCoordinatorSupplier;
        mInsetObserverViewSupplier = insetObserverViewSupplier;
        mBackButtonShouldCloseTabFn = backButtonShouldCloseTabFn;
        mCanAnimateBrowserControls = () -> {
            // These null checks prevent any exceptions that may be caused by callbacks after
            // destruction.
            if (mActivity == null || mActivityTabProvider == null) return false;
            final Tab tab = mActivityTabProvider.get();
            return tab != null && tab.isUserInteractable() && !tab.isNativePage();
        };

        getAppBrowserControlsVisibilityDelegate().addDelegate(
                browserControlsManager.getBrowserVisibilityDelegate());
        mRootUiTabObserver = new RootUiTabObserver(tabProvider);
    }

    @Override
    public void onDestroy() {
        FeatureNotificationUtils.unregisterIPHCallback(FeatureType.DEFAULT_BROWSER);

        if (mSystemUiCoordinator != null) mSystemUiCoordinator.destroy();
        if (mEmptyBackgroundViewWrapper != null) mEmptyBackgroundViewWrapper.destroy();

        if (mOfflineIndicatorController != null) {
            mOfflineIndicatorController.destroy();
        }

        if (mOfflineIndicatorInProductHelpController != null) {
            mOfflineIndicatorInProductHelpController.destroy();
        }
        if (mStatusIndicatorCoordinator != null) {
            mStatusIndicatorCoordinator.removeObserver(mStatusIndicatorObserver);
            mStatusIndicatorCoordinator.removeObserver(mStatusBarColorController);
            mStatusIndicatorCoordinator.destroy();
        }

        if (mRootUiTabObserver != null) mRootUiTabObserver.destroy();

        if (mAddToHomescreenIPHController != null) mAddToHomescreenIPHController.destroy();

        if (mAppBannerInProductHelpController != null) {
            AppBannerInProductHelpControllerFactory.detach(mAppBannerInProductHelpController);
        }

        if (mCrowIphController != null) {
            mCrowIphController.destroy();
        }

        if (mPwaBottomSheetController != null) {
            PwaBottomSheetControllerFactory.detach(mPwaBottomSheetController);
        }

        if (mHistoryNavigationCoordinator != null) {
            TouchEventObserver obs = mHistoryNavigationCoordinator.getTouchEventObserver();
            if (mCompositorViewHolderSupplier.hasValue() && obs != null) {
                mCompositorViewHolderSupplier.get().removeTouchEventObserver(obs);
            }
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

        if (mUndoGroupSnackbarController != null) {
            mUndoGroupSnackbarController.destroy();
        }

        if (mCommerceSubscriptionsService != null) {
            mCommerceSubscriptionsService.destroy();
            mCommerceSubscriptionsService = null;
        }

        super.onDestroy();
    }

    @Override
    public void onPostInflationStartup() {
        super.onPostInflationStartup();

        mSystemUiCoordinator = new TabbedSystemUiCoordinator(mActivity.getWindow(),
                mTabModelSelectorSupplier.get(), mLayoutManagerSupplier, mFullscreenManager);
    }

    @Override
    protected void onFindToolbarShown() {
        super.onFindToolbarShown();
        EphemeralTabCoordinator coordinator = mEphemeralTabCoordinatorSupplier.get();
        if (coordinator != null && coordinator.isOpened()) coordinator.close();
    }

    @Override
    public void onFinishNativeInitialization() {
        super.onFinishNativeInitialization();
        assert mLayoutManager != null;

        // final Function<Tab, Boolean> backButtonShouldCloseTabFn = mBackButtonShouldCloseTabFn;
        mHistoryNavigationCoordinator = HistoryNavigationCoordinator.create(mWindowAndroid,
                mActivityLifecycleDispatcher, mCompositorViewHolderSupplier.get(),
                mCallbackController.makeCancelable(
                        () -> mLayoutManager.getActiveLayout().requestUpdate()),
                mActivityTabProvider, mInsetObserverViewSupplier.get(),
                new BackActionDelegate() {
                    @Override
                    public @ActionType int getBackActionType(Tab tab) {
                        if (isShowingStartSurfaceHomepage()) return ActionType.EXIT_APP;
                        if (tab.canGoBack() || StartSurfaceUserData.isOpenedFromStart(tab)
                                || tab.getLaunchType() == TabLaunchType.FROM_START_SURFACE) {
                            return ActionType.NAVIGATE_BACK;
                        }
                        if (TabAssociatedApp.isOpenedFromExternalApp(tab)) {
                            return ActionType.EXIT_APP;
                        }
                        return mBackButtonShouldCloseTabFn.apply(tab) ? ActionType.CLOSE_TAB
                                                                      : ActionType.EXIT_APP;
                    }

                    @Override
                    public void onBackGesture() {
                        // Back navigation gesture performs what the back button would do.
                        mActivity.onBackPressed();
                    }

                    @Override
                    public boolean isNavigable() {
                        return isShowingStartSurfaceHomepage();
                    }
                },
                mCompositorViewHolderSupplier.get()::addTouchEventObserver,
                mCompositorViewHolderSupplier.get()::removeTouchEventObserver, mLayoutManager);
        mGestureNavLayoutObserver = new LayoutStateProvider.LayoutStateObserver() {
            @Override
            public void onStartedShowing(int layoutType, boolean showToolbar) {
                if (layoutType == LayoutType.TAB_SWITCHER) mHistoryNavigationCoordinator.reset();
            }
        };
        mRootUiTabObserver.swapToTab(mActivityTabProvider.get());

        // TODO(twellington): Supply TabModelSelector as well and move initialization earlier.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            mEmptyBackgroundViewWrapper =
                    new EmptyBackgroundViewWrapper(mTabModelSelectorSupplier.get(),
                            mTabCreatorManagerSupplier.get().getTabCreator(false), mActivity,
                            mSnackbarManagerSupplier.get(), mLayoutManagerSupplier);
            mEmptyBackgroundViewWrapper.initialize();
        }

        if (EphemeralTabCoordinator.isSupported()) {
            mEphemeralTabCoordinatorSupplier.set(
                    new EphemeralTabCoordinator(mActivity, mWindowAndroid,
                            mActivity.getWindow().getDecorView(), mActivityTabProvider, () -> {
                                return mTabCreatorManagerSupplier.get().getTabCreator(
                                        mTabModelSelectorSupplier.get().isIncognitoSelected());
                            }, getBottomSheetController(), true));
        }

        // TODO(https://crbug.com/1157955): Investigate switching to per-Activity coordinator that
        // uses signals from the current Tab to decide when to show the PWA install bottom sheet
        // rather than relying on unowned user data.
        mPwaBottomSheetController =
                PwaBottomSheetControllerFactory.createPwaBottomSheetController(mActivity);
        PwaBottomSheetControllerFactory.attach(mWindowAndroid, mPwaBottomSheetController);
        initContinuousSearchCoordinator();
        initCommerceSubscriptionsService();
        initUndoGroupSnackbarController();
    }

    @Override
    protected void setLayoutStateProvider(LayoutStateProvider layoutStateProvider) {
        super.setLayoutStateProvider(layoutStateProvider);
        if (mGestureNavLayoutObserver != null) {
            layoutStateProvider.addObserver(mGestureNavLayoutObserver);
        }
    }

    private boolean isShowingStartSurfaceHomepage() {
        return false;
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
    protected boolean shouldInitializeMerchantTrustSignals() {
        return true;
    }

    @Override
    protected ScrimCoordinator buildScrimWidget() {
        ViewGroup coordinator = mActivity.findViewById(R.id.coordinator);
        ScrimCoordinator.SystemUiScrimDelegate delegate =
                new ScrimCoordinator.SystemUiScrimDelegate() {
                    @Override
                    public void setStatusBarScrimFraction(float scrimFraction) {
                        mStatusBarColorController.setStatusBarScrimFraction(scrimFraction);
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
        return new ScrimCoordinator(mActivity, delegate, coordinator, Color.RED);
    }

    // Private class methods

    private void initializeIPH(boolean intentWithEffect) {
        if (mActivity == null) return;
        boolean didTriggerPromo = false;

        if (!didTriggerPromo) {
            didTriggerPromo = FeatureNotificationUtils.willShowIPH(FeatureType.DEFAULT_BROWSER);
            FeatureNotificationUtils.registerIPHCallback(FeatureType.DEFAULT_BROWSER, () -> {
                DefaultBrowserPromoUtils.prepareLaunchPromoIfNeeded(
                        mActivity, mWindowAndroid, true /* ignoreMaxCount */);
            });
        }

        if (!didTriggerPromo) {
            didTriggerPromo = triggerPromo(intentWithEffect);
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
                    new OfflineIndicatorInProductHelpController(mActivity,
                            mStatusIndicatorCoordinator);
        }

        mAddToHomescreenIPHController = new AddToHomescreenIPHController(mActivity, mWindowAndroid,
                mModalDialogManagerSupplier.get(),
                MessageDispatcherProvider.from(mWindowAndroid));
        mLinkToTextIPHController =
                new LinkToTextIPHController(mActivityTabProvider, mTabModelSelectorSupplier.get());
        mAppBannerInProductHelpController =
                AppBannerInProductHelpControllerFactory.createAppBannerInProductHelpController(
                        mActivity);
        AppBannerInProductHelpControllerFactory.attach(
                mWindowAndroid, mAppBannerInProductHelpController);
        mCrowIphController = new CrowIphController(mActivity,
                new CrowButtonDelegateImpl(), mActivityTabProvider);
    }

    private void updateTopControlsHeight(boolean animate) {
        final BrowserControlsSizer browserControlsSizer = mBrowserControlsManager;
        final int topControlsNewHeight = mStatusIndicatorHeight + mContinuousSearchHeight;

        browserControlsSizer.setAnimateBrowserControlsHeightChanges(animate);
        browserControlsSizer.setTopControlsHeight(topControlsNewHeight, mStatusIndicatorHeight);
        if (animate) browserControlsSizer.setAnimateBrowserControlsHeightChanges(false);
    }

    private void initCommerceSubscriptionsService() {
        if (!PriceTrackingUtilities.getPriceTrackingNotificationsEnabled()) {
            return;
        }

        CommerceSubscriptionsServiceFactory factory = new CommerceSubscriptionsServiceFactory();
        mCommerceSubscriptionsService = factory.getForLastUsedProfile();
        mCommerceSubscriptionsService.getSubscriptionsManager().queryAndUpdateWaaEnabled();
        mCommerceSubscriptionsService.initDeferredStartupForActivity(
                mTabModelSelectorSupplier.get(), mActivityLifecycleDispatcher);
    }

    private void initUndoGroupSnackbarController() {
        if (TabUiFeatureUtilities.isTabGroupsAndroidEnabled(mActivity)) {
            mUndoGroupSnackbarController = new UndoGroupSnackbarController(
                    mActivity, mTabModelSelectorSupplier.get(), mSnackbarManagerSupplier.get());
        } else {
            mUndoGroupSnackbarController = null;
        }
    }

    private void initStatusIndicatorCoordinator(LayoutManagerImpl layoutManager) {
        // TODO(crbug.com/1035584): Disable on tablets for now as we need to do one or two extra
        // things for tablets.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            return;
        }

        final BrowserControlsSizer browserControlsSizer = mBrowserControlsManager;
        mStatusIndicatorCoordinator = new StatusIndicatorCoordinator(mActivity,
                mCompositorViewHolderSupplier.get().getResourceManager(), browserControlsSizer,
                mStatusBarColorController::getStatusBarColorWithoutStatusIndicator,
                mCanAnimateBrowserControls, layoutManager::requestUpdate);
        layoutManager.addSceneOverlay(mStatusIndicatorCoordinator.getSceneLayer());
        mStatusIndicatorObserver = new StatusIndicatorCoordinator.StatusIndicatorObserver() {
            @Override
            public void onStatusIndicatorHeightChanged(int indicatorHeight) {
                mStatusIndicatorHeight = indicatorHeight;
                updateTopControlsHeight(/*animate=*/true);
            }
        };
        mStatusIndicatorCoordinator.addObserver(mStatusIndicatorObserver);
        mStatusIndicatorCoordinator.addObserver(mStatusBarColorController);

        ObservableSupplierImpl<Boolean> isUrlBarFocusedSupplier = new ObservableSupplierImpl<>();
        mOfflineIndicatorController = new OfflineIndicatorControllerV2(mActivity,
                mStatusIndicatorCoordinator, isUrlBarFocusedSupplier, mCanAnimateBrowserControls);
    }

    private void initContinuousSearchCoordinator() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CONTINUOUS_SEARCH)) {
            return;
        }

        Supplier<Integer> defaultTopContainerHeightSupplier = ()
                -> 0;
        final ViewStub viewStub = mActivity.findViewById(R.id.continuous_search_container_stub);
        final BrowserControlsSizer browserControlsSizer = mBrowserControlsManager;
        mContinuousSearchContainerCoordinator = new ContinuousSearchContainerCoordinator(viewStub,
                mLayoutManager, mCompositorViewHolderSupplier.get().getResourceManager(),
                mActivityTabProvider, browserControlsSizer, mCanAnimateBrowserControls,
                defaultTopContainerHeightSupplier, getTopUiThemeColorProvider(), mActivity,
                result -> {});
        mContinuousSearchObserver = (newHeight, animate) -> {
            mContinuousSearchHeight = newHeight;
            updateTopControlsHeight(animate);
        };
        mContinuousSearchContainerCoordinator.addHeightObserver(mContinuousSearchObserver);
        mContinuousSearchTabObscuringHandlerObserver =
                isObscured -> mContinuousSearchContainerCoordinator.updateTabObscured(isObscured);
        getTabObscuringHandler().addObserver(mContinuousSearchTabObscuringHandlerObserver);

        if (!mSupportsFindInPageSupplier.getAsBoolean()) return;
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
    public EphemeralTabCoordinator getEphemeralTabCoordinatorForTesting() {
        return mEphemeralTabCoordinatorSupplier.get();
    }

    @VisibleForTesting
    public HistoryNavigationCoordinator getHistoryNavigationCoordinatorForTesting() {
        return mHistoryNavigationCoordinator;
    }

    /** Called when a link is copied through context menu. */
    public void onContextMenuCopyLink() {
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
            if (!isShowingPromo && !intentWithEffect
                    && preferenceManager.readBoolean(
                            ChromePreferenceKeys.PROMOS_SKIPPED_ON_FIRST_START, false)
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
        if (DefaultBrowserPromoUtils.prepareLaunchPromoIfNeeded(
                    mActivity, mWindowAndroid, false /* ignoreMaxCount */)) {
            return true;
        }
        return false;
    }
}
