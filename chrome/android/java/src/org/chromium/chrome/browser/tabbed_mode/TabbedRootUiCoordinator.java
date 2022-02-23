// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AppCompatActivity;

import org.chromium.base.ApiCompatibilityUtils;
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
import org.chromium.chrome.browser.ApplicationLifetime;
import org.chromium.chrome.browser.SwipeRefreshHandler;
import org.chromium.chrome.browser.app.tab_activity_glue.TabReparentingController;
import org.chromium.chrome.browser.banners.AppBannerInProductHelpController;
import org.chromium.chrome.browser.banners.AppBannerInProductHelpControllerFactory;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchContainerCoordinator;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchContainerCoordinator.HeightObserver;
import org.chromium.chrome.browser.feature_guide.notifications.FeatureNotificationUtils;
import org.chromium.chrome.browser.feature_guide.notifications.FeatureType;
import org.chromium.chrome.browser.feed.webfeed.WebFeedFollowIntroController;
import org.chromium.chrome.browser.findinpage.FindToolbarObserver;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.gesturenav.BackActionDelegate;
import org.chromium.chrome.browser.gesturenav.HistoryNavigationCoordinator;
import org.chromium.chrome.browser.gesturenav.NavigationSheet;
import org.chromium.chrome.browser.gesturenav.TabbedSheetDelegate;
import org.chromium.chrome.browser.history.HistoryManagerUtils;
import org.chromium.chrome.browser.language.AppLanguagePromoDialog;
import org.chromium.chrome.browser.language.LanguageAskPrompt;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceIphController;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.night_mode.WebContentsDarkModeMessageController;
import org.chromium.chrome.browser.ntp.NewTabPageLaunchOrigin;
import org.chromium.chrome.browser.ntp.NewTabPageUtils;
import org.chromium.chrome.browser.offlinepages.indicator.OfflineIndicatorControllerV2;
import org.chromium.chrome.browser.offlinepages.indicator.OfflineIndicatorInProductHelpController;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxPedalDelegate;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxDialogController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.read_later.ReadLaterIPHController;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextIPHController;
import org.chromium.chrome.browser.signin.SyncConsentActivityLauncherImpl;
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
import org.chromium.chrome.browser.toolbar.ToolbarButtonInProductHelpController;
import org.chromium.chrome.browser.toolbar.ToolbarIntentMetadata;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.chrome.browser.ui.TabObscuringHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuBlocker;
import org.chromium.chrome.browser.ui.appmenu.AppMenuDelegate;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.SigninPromoUtil;
import org.chromium.chrome.browser.ui.system.StatusBarColorController.StatusBarColorProvider;
import org.chromium.chrome.browser.ui.tablet.emptybackground.EmptyBackgroundViewWrapper;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.chrome.browser.webapps.AddToHomescreenIPHController;
import org.chromium.chrome.browser.webapps.AddToHomescreenMostVisitedTileClickObserver;
import org.chromium.chrome.features.start_surface.StartSurface;
import org.chromium.chrome.features.start_surface.StartSurfaceState;
import org.chromium.chrome.features.start_surface.StartSurfaceUserData;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.util.ComposedBrowserControlsVisibilityDelegate;
import org.chromium.components.browser_ui.widget.InsetObserverView;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.TouchEventObserver;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.version_info.VersionInfo;
import org.chromium.components.webapps.bottomsheet.PwaBottomSheetController;
import org.chromium.components.webapps.bottomsheet.PwaBottomSheetControllerFactory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.util.TokenHolder;

/**
 * A {@link RootUiCoordinator} variant that controls tabbed-mode specific UI.
 */
public class TabbedRootUiCoordinator extends RootUiCoordinator {
    private static boolean sEnableStatusIndicatorForTests;

    private final ObservableSupplierImpl<EphemeralTabCoordinator> mEphemeralTabCoordinatorSupplier;
    private final RootUiTabObserver mRootUiTabObserver;
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
    private LinkToTextIPHController mLinkToTextIPHController;
    private AddToHomescreenMostVisitedTileClickObserver mAddToHomescreenMostVisitedTileObserver;
    private AppBannerInProductHelpController mAppBannerInProductHelpController;
    private PwaBottomSheetController mPwaBottomSheetController;
    private HistoryNavigationCoordinator mHistoryNavigationCoordinator;
    private NavigationSheet mNavigationSheet;
    private ComposedBrowserControlsVisibilityDelegate mAppBrowserControlsVisibilityDelegate;
    private LayoutManagerImpl mLayoutManager;
    private ContinuousSearchContainerCoordinator mContinuousSearchContainerCoordinator;
    private HeightObserver mContinuousSearchObserver;
    private TabObscuringHandler.Observer mContinuousSearchTabObscuringHandlerObserver;
    private FindToolbarObserver mContinuousSearchFindToolbarObserver;
    private CommerceSubscriptionsService mCommerceSubscriptionsService;
    private UndoGroupSnackbarController mUndoGroupSnackbarController;
    private final int mControlContainerHeightResource;
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
     * @param bookmarkBridgeSupplier Supplier of the bookmark  bridge for the current profile.
     * @param contextualSearchManagerSupplier Supplier of the {@link ContextualSearchManager}.
     * @param tabModelSelectorSupplier Supplies the {@link TabModelSelector}.
     * @param startSurfaceSupplier Supplier of the {@link StartSurface}.
     * @param intentMetadataOneshotSupplier Supplier with information about the launching intent.
     * @param layoutStateProviderOneshotSupplier Supplier of the {@link LayoutStateProvider}.
     * @param startSurfaceParentTabSupplier Supplies the parent tab for the StartSurface.
     * @param browserControlsManager Manages the browser controls.
     * @param windowAndroid The current {@link WindowAndroid}.
     * @param jankTracker Tracks the jank in the app.
     * @param activityLifecycleDispatcher Allows observation of the activity lifecycle.
     * @param layoutManagerSupplier Supplies the {@link LayoutManager}.
     * @param menuOrKeyboardActionController Controls the menu or keyboard action controller.
     * @param activityThemeColorSupplier Supplies the activity color theme.
     * @param modalDialogManagerSupplier Supplies the {@link ModalDialogManager}.
     * @param appMenuBlocker Controls the app menu blocking.
     * @param supportsAppMenuSupplier Supplies the support state for the app menu.
     * @param supportsFindInPage Supplies the support state for find in page.
     * @param tabCreatorManagerSupplier Supplies the {@link TabCreatorManager}.
     * @param fullscreenManager Manages the fullscreen state.
     * @param compositorViewHolderSupplier Supplies the {@link CompositorViewHolder}.
     * @param tabContentManagerSupplier Supplies the {@link TabContentManager}.
     * @param overviewModeBehaviorSupplier Supplier of the overview mode manager.
     * @param snackbarManagerSupplier Supplies the {@link SnackbarManager}.
     * @param activityType The {@link ActivityType} for the activity.
     * @param isInOverviewModeSupplier Supplies whether the app is in overview mode.
     * @param isWarmOnResumeSupplier Supplies whether the app was warm on resume.
     * @param appMenuDelegate The app menu delegate.
     * @param statusBarColorProvider Provides the status bar color.
     * @param ephemeralTabCoordinatorSupplier Supplies the {@link EphemeralTabCoordinator}.
     * @param intentRequestTracker Tracks intent requests.
     * @param controlContainerHeightResource The resource for the control container.
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
            @NonNull ObservableSupplier<BookmarkBridge> bookmarkBridgeSupplier,
            @NonNull Supplier<ContextualSearchManager> contextualSearchManagerSupplier,
            @NonNull ObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            @NonNull OneshotSupplier<StartSurface> startSurfaceSupplier,
            @NonNull OneshotSupplier<ToolbarIntentMetadata> intentMetadataOneshotSupplier,
            @NonNull OneshotSupplier<LayoutStateProvider> layoutStateProviderOneshotSupplier,
            @NonNull Supplier<Tab> startSurfaceParentTabSupplier,
            @NonNull BrowserControlsManager browserControlsManager,
            @NonNull ActivityWindowAndroid windowAndroid, @NonNull JankTracker jankTracker,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull ObservableSupplier<LayoutManagerImpl> layoutManagerSupplier,
            @NonNull MenuOrKeyboardActionController menuOrKeyboardActionController,
            @NonNull Supplier<Integer> activityThemeColorSupplier,
            @NonNull ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            @NonNull AppMenuBlocker appMenuBlocker,
            @NonNull BooleanSupplier supportsAppMenuSupplier,
            @NonNull BooleanSupplier supportsFindInPage,
            @NonNull Supplier<TabCreatorManager> tabCreatorManagerSupplier,
            @NonNull FullscreenManager fullscreenManager,
            @NonNull Supplier<CompositorViewHolder> compositorViewHolderSupplier,
            @NonNull Supplier<TabContentManager> tabContentManagerSupplier,
            @NonNull OneshotSupplier<OverviewModeBehavior> overviewModeBehaviorSupplier,
            @NonNull Supplier<SnackbarManager> snackbarManagerSupplier,
            @ActivityType int activityType, @NonNull Supplier<Boolean> isInOverviewModeSupplier,
            @NonNull Supplier<Boolean> isWarmOnResumeSupplier,
            @NonNull AppMenuDelegate appMenuDelegate,
            @NonNull StatusBarColorProvider statusBarColorProvider,
            @NonNull ObservableSupplierImpl<EphemeralTabCoordinator>
                    ephemeralTabCoordinatorSupplier,
            @NonNull IntentRequestTracker intentRequestTracker, int controlContainerHeightResource,
            @NonNull Supplier<InsetObserverView> insetObserverViewSupplier,
            @NonNull Function<Tab, Boolean> backButtonShouldCloseTabFn,
            OneshotSupplier<TabReparentingController> tabReparentingControllerSupplier,
            @NonNull OmniboxPedalDelegate omniboxPedalDelegate,
            boolean initializeUiWithIncognitoColors) {
        super(activity, onOmniboxFocusChangedListener, shareDelegateSupplier, tabProvider,
                profileSupplier, bookmarkBridgeSupplier, contextualSearchManagerSupplier,
                tabModelSelectorSupplier, startSurfaceSupplier, intentMetadataOneshotSupplier,
                layoutStateProviderOneshotSupplier, startSurfaceParentTabSupplier,
                browserControlsManager, windowAndroid, jankTracker, activityLifecycleDispatcher,
                layoutManagerSupplier, menuOrKeyboardActionController, activityThemeColorSupplier,
                modalDialogManagerSupplier, appMenuBlocker, supportsAppMenuSupplier,
                supportsFindInPage, tabCreatorManagerSupplier, fullscreenManager,
                compositorViewHolderSupplier, tabContentManagerSupplier,
                overviewModeBehaviorSupplier, snackbarManagerSupplier, activityType,
                isInOverviewModeSupplier, isWarmOnResumeSupplier, appMenuDelegate,
                statusBarColorProvider, intentRequestTracker, tabReparentingControllerSupplier,
                omniboxPedalDelegate, initializeUiWithIncognitoColors);
        mEphemeralTabCoordinatorSupplier = ephemeralTabCoordinatorSupplier;
        mControlContainerHeightResource = controlContainerHeightResource;
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

        if (mToolbarManager != null) {
            mToolbarManager.getOmniboxStub().removeUrlFocusChangeListener(mUrlFocusChangeListener);
        }

        if (mOfflineIndicatorInProductHelpController != null) {
            mOfflineIndicatorInProductHelpController.destroy();
        }
        if (mStatusIndicatorCoordinator != null) {
            mStatusIndicatorCoordinator.removeObserver(mStatusIndicatorObserver);
            mStatusIndicatorCoordinator.removeObserver(mStatusBarColorController);
            mStatusIndicatorCoordinator.destroy();
        }

        if (mToolbarButtonInProductHelpController != null) {
            mToolbarButtonInProductHelpController.destroy();
        }

        if (mWebFeedFollowIntroController != null) {
            mWebFeedFollowIntroController.destroy();
        }

        if (mRootUiTabObserver != null) mRootUiTabObserver.destroy();

        if (mAddToHomescreenIPHController != null) mAddToHomescreenIPHController.destroy();

        if (mAppBannerInProductHelpController != null) {
            AppBannerInProductHelpControllerFactory.detach(mAppBannerInProductHelpController);
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
            if (mFindToolbarManager != null) {
                mFindToolbarManager.removeObserver(mContinuousSearchFindToolbarObserver);
            }
            mContinuousSearchContainerCoordinator.destroy();
            mContinuousSearchContainerCoordinator = null;
            mContinuousSearchObserver = null;
            mContinuousSearchTabObscuringHandlerObserver = null;
            mContinuousSearchFindToolbarObserver = null;
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
                mTabModelSelectorSupplier.get(), mOverviewModeBehaviorSupplier, mFullscreenManager);
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
        if (mActivity == null) return;
        Tab tab = mActivityTabProvider.get();
        if (tab == null || tab.getWebContents() == null || !tab.isUserInteractable()) return;
        Profile profile = Profile.fromWebContents(tab.getWebContents());
        mNavigationSheet = NavigationSheet.create(
                mActivity.getWindow().getDecorView().findViewById(android.R.id.content), mActivity,
                this::getBottomSheetController, profile);
        mNavigationSheet.setDelegate(new TabbedSheetDelegate(tab, aTab -> {
            HistoryManagerUtils.showHistoryManager(mActivity, aTab,
                    mTabModelSelectorSupplier.hasValue()
                            && mTabModelSelectorSupplier.get().isIncognitoSelected());
        }, mActivity.getResources().getString(R.string.show_full_history)));
        if (!mNavigationSheet.startAndExpand(/* forward= */ false, /* animate=*/true)) {
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
            AppMenuHandler appMenuHandler =
                    mAppMenuCoordinator == null ? null : mAppMenuCoordinator.getAppMenuHandler();
            mEmptyBackgroundViewWrapper = new EmptyBackgroundViewWrapper(
                    mTabModelSelectorSupplier.get(),
                    mTabCreatorManagerSupplier.get().getTabCreator(false), mActivity,
                    appMenuHandler, mSnackbarManagerSupplier.get(), mOverviewModeBehaviorSupplier);
            mEmptyBackgroundViewWrapper.initialize();
        }

        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)
                && (TabUiFeatureUtilities.isTabGroupsAndroidEnabled(mActivity)
                        || TabUiFeatureUtilities.isConditionalTabStripEnabled())) {
            getToolbarManager().enableBottomControls();
        }

        if (EphemeralTabCoordinator.isSupported()) {
            mEphemeralTabCoordinatorSupplier.set(
                    new EphemeralTabCoordinator(mActivity, mWindowAndroid,
                            mActivity.getWindow().getDecorView(), mActivityTabProvider, () -> {
                                return mTabCreatorManagerSupplier.get().getTabCreator(
                                        mTabModelSelectorSupplier.get().isIncognitoSelected());
                            }, getBottomSheetController(), true));
        }

        mIntentMetadataOneshotSupplier.onAvailable(mCallbackController.makeCancelable(
                (metadata) -> initializeIPH(metadata.getIsIntentWithEffect())));

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
        return mStartSurfaceSupplier.get() != null
                && mStartSurfaceSupplier.get().getController().getStartSurfaceState()
                == StartSurfaceState.SHOWN_HOMEPAGE;
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
        return new ScrimCoordinator(mActivity, delegate, coordinator,
                ApiCompatibilityUtils.getColor(coordinator.getResources(),
                        R.color.omnibox_focused_fading_background_color));
    }

    // Private class methods

    private void initializeIPH(boolean intentWithEffect) {
        if (mActivity == null) return;
        mToolbarButtonInProductHelpController = new ToolbarButtonInProductHelpController(mActivity,
                mWindowAndroid, mAppMenuCoordinator, mActivityLifecycleDispatcher,
                mActivityTabProvider, mIsInOverviewModeSupplier,
                mToolbarManager.getMenuButtonView(), mToolbarManager.getSecurityIconView());
        mReadLaterIPHController = new ReadLaterIPHController(mActivity,
                getToolbarManager().getMenuButtonView(), mAppMenuCoordinator.getAppMenuHandler());

        boolean didTriggerPromo = false;

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_3)) {
            didTriggerPromo = PrivacySandboxDialogController.maybeLaunchPrivacySandboxDialog(
                    mActivity, new SettingsLauncherImpl(),
                    mTabModelSelectorSupplier.get().isIncognitoSelected());
        }

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

        if (!didTriggerPromo) {
            mToolbarButtonInProductHelpController.showColdStartIPH();
            mReadLaterIPHController.showColdStartIPH();
            if (MultiWindowUtils.instanceSwitcherEnabled()
                    && MultiWindowUtils.shouldShowManageWindowsMenu()) {
                MultiInstanceIphController.maybeShowInProductHelp(mActivity,
                        getToolbarManager().getMenuButtonView(),
                        mAppMenuCoordinator.getAppMenuHandler(), R.id.manage_all_windows_menu_id);
            }
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

        mAddToHomescreenIPHController = new AddToHomescreenIPHController(mActivity, mWindowAndroid,
                mModalDialogManagerSupplier.get(), mAppMenuCoordinator.getAppMenuHandler(),
                R.id.add_to_homescreen_id, () -> {
                    return mToolbarManager.getMenuButtonView();
                }, MessageDispatcherProvider.from(mWindowAndroid));
        mLinkToTextIPHController =
                new LinkToTextIPHController(mActivityTabProvider, mTabModelSelectorSupplier.get());
        mAddToHomescreenMostVisitedTileObserver = new AddToHomescreenMostVisitedTileClickObserver(
                mActivityTabProvider, mAddToHomescreenIPHController);
        mAppBannerInProductHelpController =
                AppBannerInProductHelpControllerFactory.createAppBannerInProductHelpController(
                        mActivity, mAppMenuCoordinator.getAppMenuHandler(),
                        () -> mToolbarManager.getMenuButtonView(), R.id.add_to_homescreen_id);
        AppBannerInProductHelpControllerFactory.attach(
                mWindowAndroid, mAppBannerInProductHelpController);

        if (!didTriggerPromo
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)) {
            // TODO(crbug.com/1252965): Investigate locking feature engagement system during
            // "second run promos" to avoid !didTriggerPromo check.
            Tab tab;
            WebContents webContents;

            Profile profile;
            if ((tab = mActivityTabProvider.get()) != null
                    && (webContents = tab.getWebContents()) != null) {
                profile = Profile.fromWebContents(webContents);
            } else {
                profile = Profile.getLastUsedRegularProfile();
                webContents = null;
            }

            WebContentsDarkModeMessageController.attemptToSendMessage(mActivity, profile,
                    webContents, new SettingsLauncherImpl(), mMessageDispatcher);
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.WEB_FEED)) {
            mWebFeedFollowIntroController = new WebFeedFollowIntroController(mActivity,
                    mAppMenuCoordinator.getAppMenuHandler(), mActivityTabProvider,
                    mToolbarManager.getMenuButtonView(), () -> {
                        mTabCreatorManagerSupplier.get()
                                .getTabCreator(/*incognito=*/false)
                                .launchUrl(NewTabPageUtils.encodeNtpUrl(
                                                   NewTabPageLaunchOrigin.WEB_FEED),
                                        TabLaunchType.FROM_CHROME_UI);
                    }, mModalDialogManagerSupplier.get(), mSnackbarManagerSupplier.get());
        }
    }

    private void updateTopControlsHeight(boolean animate) {
        final BrowserControlsSizer browserControlsSizer = mBrowserControlsManager;
        final int resourceId = mControlContainerHeightResource;
        final int topControlsNewHeight = mActivity.getResources().getDimensionPixelSize(resourceId)
                + mStatusIndicatorHeight + mContinuousSearchHeight;

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
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)
                || (!ChromeFeatureList.isEnabled(ChromeFeatureList.OFFLINE_INDICATOR_V2)
                        && !sEnableStatusIndicatorForTests)) {
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
                -> mActivity.getResources().getDimensionPixelSize(mControlContainerHeightResource);
        final ViewStub viewStub = mActivity.findViewById(R.id.continuous_search_container_stub);
        final BrowserControlsSizer browserControlsSizer = mBrowserControlsManager;
        mContinuousSearchContainerCoordinator = new ContinuousSearchContainerCoordinator(viewStub,
                mLayoutManager, mCompositorViewHolderSupplier.get().getResourceManager(),
                mActivityTabProvider, browserControlsSizer, mCanAnimateBrowserControls,
                defaultTopContainerHeightSupplier, getTopUiThemeColorProvider(), mActivity,
                mToolbarManager::setForceHideShadow);
        mContinuousSearchObserver = (newHeight, animate) -> {
            mContinuousSearchHeight = newHeight;
            updateTopControlsHeight(animate);
        };
        mContinuousSearchContainerCoordinator.addHeightObserver(mContinuousSearchObserver);
        mContinuousSearchTabObscuringHandlerObserver =
                isObscured -> mContinuousSearchContainerCoordinator.updateTabObscured(isObscured);
        getTabObscuringHandler().addObserver(mContinuousSearchTabObscuringHandlerObserver);

        if (!mSupportsFindInPageSupplier.getAsBoolean()) return;

        assert mFindToolbarManager != null;

        mContinuousSearchFindToolbarObserver = new FindToolbarObserver() {
            private int mToken = TokenHolder.INVALID_TOKEN;

            @Override
            public void onFindToolbarShown() {
                assert mToken == TokenHolder.INVALID_TOKEN;
                mToken = mContinuousSearchContainerCoordinator.hideContainer();
            }

            @Override
            public void onFindToolbarHidden() {
                mContinuousSearchContainerCoordinator.showContainer(mToken);
                mToken = TokenHolder.INVALID_TOKEN;
            }
        };
        mFindToolbarManager.addObserver(mContinuousSearchFindToolbarObserver);
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
        if (SigninPromoUtil.launchSigninPromoIfNeeded(mActivity,
                    SyncConsentActivityLauncherImpl.get(), VersionInfo.getProductMajorVersion())) {
            return true;
        }
        if (DefaultBrowserPromoUtils.prepareLaunchPromoIfNeeded(
                    mActivity, mWindowAndroid, false /* ignoreMaxCount */)) {
            return true;
        }
        if (AppLanguagePromoDialog.maybeShowPrompt(mActivity, mModalDialogManagerSupplier,
                    () -> ApplicationLifetime.terminate(true))) {
            return true;
        }
        return LanguageAskPrompt.maybeShowLanguageAskPrompt(mActivity, mModalDialogManagerSupplier);
    }
}
