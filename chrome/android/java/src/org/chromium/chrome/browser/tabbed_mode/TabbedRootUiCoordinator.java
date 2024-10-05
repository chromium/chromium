// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TraceEvent;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneShotCallback;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.SwipeRefreshHandler;
import org.chromium.chrome.browser.accessibility.PageZoomIPHController;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.crash.ChromePureJavaExceptionReporter;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.data_sharing.DataSharingTabSwitcherDelegate;
import org.chromium.chrome.browser.data_sharing.InstantMessageDelegateFactory;
import org.chromium.chrome.browser.data_sharing.InstantMessageDelegateImpl;
import org.chromium.chrome.browser.desktop_site.DesktopSiteSettingsIPHController;
import org.chromium.chrome.browser.dragdrop.ChromeTabbedOnDragListener;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.feed.webfeed.WebFeedFollowIntroController;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.gesturenav.BackActionDelegate;
import org.chromium.chrome.browser.gesturenav.HistoryNavigationCoordinator;
import org.chromium.chrome.browser.gesturenav.NavigationSheet;
import org.chromium.chrome.browser.gesturenav.RtlGestureNavIphController;
import org.chromium.chrome.browser.gesturenav.TabbedSheetDelegate;
import org.chromium.chrome.browser.history.HistoryManagerUtils;
import org.chromium.chrome.browser.hub.HubManager;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthCoordinatorFactory;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponent;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponentSupplier;
import org.chromium.chrome.browser.language.AppLanguagePromoDialog;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifetime.ApplicationLifetime;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceIphController;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.night_mode.WebContentsDarkModeMessageController;
import org.chromium.chrome.browser.notifications.permissions.NotificationPermissionController;
import org.chromium.chrome.browser.notifications.permissions.NotificationPermissionController.RationaleDelegate;
import org.chromium.chrome.browser.notifications.permissions.NotificationPermissionRationaleBottomSheet;
import org.chromium.chrome.browser.notifications.permissions.NotificationPermissionRationaleDialogController;
import org.chromium.chrome.browser.ntp.NewTabPageLaunchOrigin;
import org.chromium.chrome.browser.ntp.NewTabPageUtils;
import org.chromium.chrome.browser.offlinepages.indicator.OfflineIndicatorControllerV2;
import org.chromium.chrome.browser.offlinepages.indicator.OfflineIndicatorInProductHelpController;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.privacy_sandbox.ActivityTypeMapper;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridge;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxDialogController;
import org.chromium.chrome.browser.privacy_sandbox.SurfaceType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.read_later.ReadLaterIPHController;
import org.chromium.chrome.browser.readaloud.ReadAloudIPHController;
import org.chromium.chrome.browser.search_engines.choice_screen.ChoiceDialogCoordinator;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextIPHController;
import org.chromium.chrome.browser.share.page_info_sheet.PageInfoSharingControllerImpl;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.signin.SyncConsentActivityLauncherImpl;
import org.chromium.chrome.browser.status_indicator.StatusIndicatorCoordinator;
import org.chromium.chrome.browser.subscriptions.CommerceSubscriptionsService;
import org.chromium.chrome.browser.subscriptions.CommerceSubscriptionsServiceFactory;
import org.chromium.chrome.browser.tab.RequestDesktopUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncController;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.UndoGroupSnackbarController;
import org.chromium.chrome.browser.toolbar.ToolbarButtonInProductHelpController;
import org.chromium.chrome.browser.toolbar.ToolbarFeatures;
import org.chromium.chrome.browser.toolbar.ToolbarIntentMetadata;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuBlocker;
import org.chromium.chrome.browser.ui.appmenu.AppMenuDelegate;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderCoordinator;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.FullscreenSigninPromoLauncher;
import org.chromium.chrome.browser.ui.system.StatusBarColorController.StatusBarColorProvider;
import org.chromium.chrome.browser.webapps.PwaRestorePromoUtils;
import org.chromium.components.browser_ui.accessibility.PageZoomCoordinator;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider;
import org.chromium.components.browser_ui.widget.CoordinatorLayoutForPointer;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.TouchEventObserver;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.ServiceStatus;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.search_engines.SearchEnginesFeatures;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.webapps.bottomsheet.PwaBottomSheetController;
import org.chromium.components.webapps.bottomsheet.PwaBottomSheetControllerFactory;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.dragdrop.DragDropGlobalState;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.BooleanSupplier;
import java.util.function.Function;

/** A {@link RootUiCoordinator} variant that controls tabbed-mode specific UI. */
public class TabbedRootUiCoordinator extends RootUiCoordinator {
    private static boolean sDisableTopControlsAnimationForTesting;
    private final RootUiTabObserver mRootUiTabObserver;
    private TabbedSystemUiCoordinator mSystemUiCoordinator;
    private TabGroupSyncController mTabGroupSyncController;
    private final OneshotSupplierImpl<TabGroupUiActionHandler> mTabGroupUiActionHandlerSupplier =
            new OneshotSupplierImpl<>();
    private StatusIndicatorCoordinator mStatusIndicatorCoordinator;
    private StatusIndicatorCoordinator.StatusIndicatorObserver mStatusIndicatorObserver;
    private OfflineIndicatorControllerV2 mOfflineIndicatorController;
    private OfflineIndicatorInProductHelpController mOfflineIndicatorInProductHelpController;
    private ReadAloudIPHController mReadAloudIphController;
    private ReadLaterIPHController mReadLaterIphController;
    private DesktopSiteSettingsIPHController mDesktopSiteSettingsIphController;
    private RtlGestureNavIphController mRtlGestureNavIphController;
    private WebFeedFollowIntroController mWebFeedFollowIntroController;
    private UrlFocusChangeListener mUrlFocusChangeListener;
    private @Nullable ToolbarButtonInProductHelpController mToolbarButtonInProductHelpController;
    private PwaBottomSheetController mPwaBottomSheetController;
    private NotificationPermissionController mNotificationPermissionController;
    private HistoryNavigationCoordinator mHistoryNavigationCoordinator;
    private NavigationSheet mNavigationSheet;
    private LayoutManagerImpl mLayoutManager;
    private CommerceSubscriptionsService mCommerceSubscriptionsService;
    private UndoGroupSnackbarController mUndoGroupSnackbarController;
    private final InsetObserver mInsetObserver;
    private final Function<Tab, Boolean> mBackButtonShouldCloseTabFn;
    private final LayoutStateProvider.LayoutStateObserver mGestureNavLayoutObserver;
    private final OneshotSupplierImpl<EphemeralTabCoordinator> mEphemeralTabCoordinatorSupplier;
    private Callback<Integer> mOnTabStripHeightChangedCallback;
    private final MultiInstanceManager mMultiInstanceManager;
    private int mStatusIndicatorHeight;
    private final OneshotSupplier<HubManager> mHubManagerSupplier;
    private TouchEventObserver mDragDropTouchObserver;
    private ViewGroup mCoordinator;
    private final ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeControllerSupplier;
    private @Nullable AppHeaderCoordinator mAppHeaderCoordinator;
    private final ManualFillingComponentSupplier mManualFillingComponentSupplier;
    private final @NonNull DataSharingTabManager mDataSharingTabManager;
    protected @Nullable InstantMessageDelegateImpl mInstantMessageDelegateImpl;

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
                var swipeHandler = SwipeRefreshHandler.from(mTab);
                swipeHandler.setNavigationCoordinator(null);
                swipeHandler.setBrowserControls(null);
            }
            mTab = tab;

            if (tab != null) {
                var swipeHandler = SwipeRefreshHandler.from(mTab);
                swipeHandler.setNavigationCoordinator(mHistoryNavigationCoordinator);
                swipeHandler.setBrowserControls(mBrowserControlsManager);
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
     *
     * @param activity The activity whose UI the coordinator is responsible for.
     * @param onOmniboxFocusChangedListener callback to invoke when Omnibox focus changes.
     * @param shareDelegateSupplier Supplies the {@link ShareDelegate}.
     * @param tabProvider The {@link ActivityTabProvider} to get current tab of the activity.
     * @param profileSupplier Supplier of the currently applicable profile.
     * @param bookmarkModelSupplier Supplier of the bookmark bridge for the current profile.
     * @param tabModelSelectorSupplier Supplies the {@link TabModelSelector}.
     * @param tabSwitcherSupplier Supplier of the {@link TabSwitcher}.
     * @param incognitoTabSwitcherSupplier Supplier of the incognito {@link TabSwitcher}.
     * @param hubManagerSupplier Supplier for the {@link HubManager}.
     * @param intentMetadataOneshotSupplier Supplier with information about the launching intent.
     * @param layoutStateProviderOneshotSupplier Supplier of the {@link LayoutStateProvider}.
     * @param browserControlsManager Manages the browser controls.
     * @param windowAndroid The current {@link WindowAndroid}.
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
     * @param snackbarManagerSupplier Supplies the {@link SnackbarManager}.
     * @param activityType The {@link ActivityType} for the activity.
     * @param isInOverviewModeSupplier Supplies whether the app is in overview mode.
     * @param appMenuDelegate The app menu delegate.
     * @param statusBarColorProvider Provides the status bar color.
     * @param ephemeralTabCoordinatorSupplier Supplies the {@link EphemeralTabCoordinator}.
     * @param intentRequestTracker Tracks intent requests.
     * @param insetObserver The {@link InsetObserver}.
     * @param backButtonShouldCloseTabFn Function which supplies whether or not the back button
     *     should close the tab.
     * @param initializeUiWithIncognitoColors Whether to initialize the UI with incognito colors.
     * @param backPressManager The {@link BackPressManager} handling back press.
     * @param savedInstanceState The saved bundle for the last recorded state.
     * @param multiInstanceManager Manages multi-instance mode.
     * @param overviewColorSupplier Notifies when the overview color changes.
     * @param baseChromeLayout The base view hosting Chrome that certain views (e.g. the omnibox
     *     suggestion list) will position themselves relative to. If null, the content view will be
     *     used.
     * @param manualFillingComponentSupplier Supplies the {@link ManualFillingComponent} for
     *     interacting with non-popup filling UI.
     */
    public TabbedRootUiCoordinator(
            @NonNull AppCompatActivity activity,
            @Nullable Callback<Boolean> onOmniboxFocusChangedListener,
            @NonNull ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            @NonNull ActivityTabProvider tabProvider,
            @NonNull ObservableSupplier<Profile> profileSupplier,
            @NonNull ObservableSupplier<BookmarkModel> bookmarkModelSupplier,
            @NonNull ObservableSupplier<TabBookmarker> tabBookmarkerSupplier,
            @NonNull ObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            @NonNull OneshotSupplier<TabSwitcher> tabSwitcherSupplier,
            @NonNull OneshotSupplier<TabSwitcher> incognitoTabSwitcherSupplier,
            @NonNull OneshotSupplier<HubManager> hubManagerSupplier,
            @NonNull OneshotSupplier<ToolbarIntentMetadata> intentMetadataOneshotSupplier,
            @NonNull OneshotSupplier<LayoutStateProvider> layoutStateProviderOneshotSupplier,
            @NonNull Supplier<Long> lastUserInteractionTimeSupplier,
            @NonNull BrowserControlsManager browserControlsManager,
            @NonNull ActivityWindowAndroid windowAndroid,
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
            @NonNull Supplier<SnackbarManager> snackbarManagerSupplier,
            @NonNull ObservableSupplierImpl<EdgeToEdgeController> edgeToEdgeSupplier,
            @ActivityType int activityType,
            @NonNull Supplier<Boolean> isInOverviewModeSupplier,
            @NonNull AppMenuDelegate appMenuDelegate,
            @NonNull StatusBarColorProvider statusBarColorProvider,
            @NonNull OneshotSupplierImpl<EphemeralTabCoordinator> ephemeralTabCoordinatorSupplier,
            @NonNull IntentRequestTracker intentRequestTracker,
            @NonNull InsetObserver insetObserver,
            @NonNull Function<Tab, Boolean> backButtonShouldCloseTabFn,
            boolean initializeUiWithIncognitoColors,
            @NonNull BackPressManager backPressManager,
            @Nullable Bundle savedInstanceState,
            @Nullable MultiInstanceManager multiInstanceManager,
            @Nullable ObservableSupplier<Integer> overviewColorSupplier,
            @Nullable View baseChromeLayout,
            @NonNull ManualFillingComponentSupplier manualFillingComponentSupplier) {
        super(
                activity,
                onOmniboxFocusChangedListener,
                shareDelegateSupplier,
                tabProvider,
                profileSupplier,
                bookmarkModelSupplier,
                tabBookmarkerSupplier,
                tabModelSelectorSupplier,
                tabSwitcherSupplier,
                incognitoTabSwitcherSupplier,
                intentMetadataOneshotSupplier,
                layoutStateProviderOneshotSupplier,
                lastUserInteractionTimeSupplier,
                browserControlsManager,
                windowAndroid,
                activityLifecycleDispatcher,
                layoutManagerSupplier,
                menuOrKeyboardActionController,
                activityThemeColorSupplier,
                modalDialogManagerSupplier,
                appMenuBlocker,
                supportsAppMenuSupplier,
                supportsFindInPage,
                tabCreatorManagerSupplier,
                fullscreenManager,
                compositorViewHolderSupplier,
                tabContentManagerSupplier,
                snackbarManagerSupplier,
                edgeToEdgeSupplier,
                activityType,
                isInOverviewModeSupplier,
                appMenuDelegate,
                statusBarColorProvider,
                intentRequestTracker,
                ephemeralTabCoordinatorSupplier,
                initializeUiWithIncognitoColors,
                backPressManager,
                savedInstanceState,
                overviewColorSupplier,
                baseChromeLayout);
        mInsetObserver = insetObserver;
        mBackButtonShouldCloseTabFn = backButtonShouldCloseTabFn;
        mEphemeralTabCoordinatorSupplier = ephemeralTabCoordinatorSupplier;
        mCanAnimateBrowserControls =
                () -> {
                    // These null checks prevent any exceptions that may be caused by callbacks
                    // after destruction.
                    if (mActivity == null || mActivityTabProvider == null) return false;
                    final Tab tab = mActivityTabProvider.get();
                    return tab != null && tab.isUserInteractable() && !tab.isNativePage();
                };

        getAppBrowserControlsVisibilityDelegate()
                .addDelegate(browserControlsManager.getBrowserVisibilityDelegate());
        mRootUiTabObserver = new RootUiTabObserver(tabProvider);
        mGestureNavLayoutObserver =
                new LayoutStateProvider.LayoutStateObserver() {
                    @Override
                    public void onStartedShowing(int layoutType) {
                        if (layoutType == LayoutType.TAB_SWITCHER) {
                            mHistoryNavigationCoordinator.reset();
                        }
                    }
                };
        mMultiInstanceManager = multiInstanceManager;
        mHubManagerSupplier = hubManagerSupplier;
        mStatusBarColorController.setAllowToolbarColorOnTablets(true);
        mEdgeToEdgeControllerSupplier = edgeToEdgeSupplier;
        mManualFillingComponentSupplier = manualFillingComponentSupplier;

        DataSharingTabSwitcherDelegate dataSharingTabSwitcherDelegate =
                (int tabId) -> mTabSwitcherSupplier.get().requestOpenTabGroupDialog(tabId);

        mDataSharingTabManager =
                new DataSharingTabManager(
                        dataSharingTabSwitcherDelegate,
                        mProfileSupplier,
                        this::getBottomSheetController,
                        mShareDelegateSupplier,
                        mWindowAndroid,
                        mActivity.getResources(),
                        mTabGroupUiActionHandlerSupplier);

        initAppHeaderCoordinator(savedInstanceState);
    }

    @Override
    public void onDestroy() {
        if (mSystemUiCoordinator != null) mSystemUiCoordinator.destroy();

        if (mOfflineIndicatorController != null) {
            mOfflineIndicatorController.destroy();
        }

        if (mTabGroupSyncController != null) {
            mTabGroupSyncController.destroy();
            mTabGroupSyncController = null;
        }

        if (mToolbarManager != null) {
            mToolbarManager.getOmniboxStub().removeUrlFocusChangeListener(mUrlFocusChangeListener);
            if (mOnTabStripHeightChangedCallback != null) {
                mToolbarManager
                        .getTabStripHeightSupplier()
                        .removeObserver(mOnTabStripHeightChangedCallback);
                mOnTabStripHeightChangedCallback = null;
            }
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

        if (mReadAloudIphController != null) {
            mReadAloudIphController.destroy();
        }

        if (mWebFeedFollowIntroController != null) {
            mWebFeedFollowIntroController.destroy();
        }

        if (mRootUiTabObserver != null) mRootUiTabObserver.destroy();

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

        if (mUndoGroupSnackbarController != null) {
            mUndoGroupSnackbarController.destroy();
        }

        if (mCommerceSubscriptionsService != null) {
            mCommerceSubscriptionsService.destroy();
            mCommerceSubscriptionsService = null;
        }

        if (mNotificationPermissionController != null) {
            NotificationPermissionController.detach(mNotificationPermissionController);
            mNotificationPermissionController = null;
        }

        if (mDesktopSiteSettingsIphController != null) {
            mDesktopSiteSettingsIphController.destroy();
            mDesktopSiteSettingsIphController = null;
        }

        if (mRtlGestureNavIphController != null) {
            mRtlGestureNavIphController.destroy();
            mRtlGestureNavIphController = null;
        }

        if (mCoordinator != null && mDragDropTouchObserver != null) {
            ((CoordinatorLayoutForPointer) mCoordinator)
                    .removeTouchEventObserver(mDragDropTouchObserver);
            mDragDropTouchObserver = null;
        }

        if (mAppHeaderCoordinator != null && VERSION.SDK_INT >= VERSION_CODES.R) {
            mAppHeaderCoordinator.destroy();
            mAppHeaderCoordinator = null;
        }

        mDataSharingTabManager.destroy();

        if (mInstantMessageDelegateImpl != null) {
            mInstantMessageDelegateImpl.detachWindow(mWindowAndroid);
        }

        super.onDestroy();
    }

    @Override
    public void onPostInflationStartup() {
        super.onPostInflationStartup();

        mSystemUiCoordinator =
                new TabbedSystemUiCoordinator(
                        mActivity.getWindow(),
                        mTabModelSelectorSupplier.get(),
                        mLayoutManagerSupplier,
                        mFullscreenManager,
                        mEdgeToEdgeControllerSupplier,
                        mBottomControlsStacker,
                        mBrowserControlsManager,
                        mSnackbarManagerSupplier,
                        mContextualSearchManagerSupplier,
                        getBottomSheetController(),
                        mToolbarManager.getLocationBar().getOmniboxSuggestionsVisualState(),
                        mManualFillingComponentSupplier
                                .get()
                                .getAccessorySheetVisualStateProvider(),
                        mInsetObserver);
    }

    @Override
    protected void onFindToolbarShown() {
        super.onFindToolbarShown();
        EphemeralTabCoordinator coordinator = mEphemeralTabCoordinatorSupplier.get();
        if (coordinator != null && coordinator.isOpened()) coordinator.close();
    }

    @Override
    public int getControlContainerHeightResource() {
        return R.dimen.control_container_height;
    }

    @Override
    protected boolean canContextualSearchPromoteToNewTab() {
        return true;
    }

    @Override
    public DataSharingTabManager getDataSharingTabManager() {
        return mDataSharingTabManager;
    }

    /** Show navigation history sheet. */
    public void showFullHistorySheet() {
        if (mActivity == null) return;
        Tab tab = mActivityTabProvider.get();
        if (tab == null || tab.getWebContents() == null || !tab.isUserInteractable()) return;
        Profile profile = tab.getProfile();
        mNavigationSheet =
                NavigationSheet.create(
                        mActivity.getWindow().getDecorView().findViewById(android.R.id.content),
                        mActivity,
                        this::getBottomSheetController,
                        profile);
        mNavigationSheet.setDelegate(
                new TabbedSheetDelegate(
                        tab,
                        aTab -> {
                            HistoryManagerUtils.showHistoryManager(
                                    mActivity,
                                    aTab,
                                    mTabModelSelectorSupplier.hasValue()
                                            && mTabModelSelectorSupplier
                                                    .get()
                                                    .isIncognitoSelected());
                        },
                        mActivity.getResources().getString(R.string.show_full_history)));
        if (!mNavigationSheet.startAndExpand(/* forward= */ false, /* animate= */ true)) {
            mNavigationSheet = null;
        } else {
            getBottomSheetController()
                    .addObserver(
                            new EmptyBottomSheetObserver() {
                                @Override
                                public void onSheetClosed(int reason) {
                                    getBottomSheetController().removeObserver(this);
                                    mNavigationSheet = null;
                                }
                            });
        }
    }

    @Override
    public void onInflationComplete() {
        mCoordinator = mActivity.findViewById(R.id.coordinator);
        super.onInflationComplete();
    }

    @Override
    public void onFinishNativeInitialization() {
        super.onFinishNativeInitialization();
        assert mLayoutManager != null;

        mHistoryNavigationCoordinator =
                HistoryNavigationCoordinator.create(
                        mWindowAndroid,
                        mActivityLifecycleDispatcher,
                        mCompositorViewHolderSupplier.get(),
                        mCallbackController.makeCancelable(
                                () -> mLayoutManager.getActiveLayout().requestUpdate()),
                        mActivityTabProvider,
                        mInsetObserver,
                        new BackActionDelegate() {
                            @Override
                            public @ActionType int getBackActionType(Tab tab) {
                                if (tab.canGoBack()) {
                                    return ActionType.NAVIGATE_BACK;
                                }
                                if (TabAssociatedApp.isOpenedFromExternalApp(tab)) {
                                    return ActionType.EXIT_APP;
                                }
                                return mBackButtonShouldCloseTabFn.apply(tab)
                                        ? ActionType.CLOSE_TAB
                                        : ActionType.EXIT_APP;
                            }

                            @Override
                            public void onBackGesture() {
                                // Back navigation gesture performs what the back button would do.
                                mActivity.getOnBackPressedDispatcher().onBackPressed();
                            }

                            @Override
                            public void onGestureUnhandled() {
                                if (mRtlGestureNavIphController != null) {
                                    mRtlGestureNavIphController.onGestureUnhandled();
                                }
                            }

                            @Override
                            public void onGestureHandled() {
                                if (mRtlGestureNavIphController != null) {
                                    mRtlGestureNavIphController.onGestureHandled();
                                }
                            }
                        },
                        mCompositorViewHolderSupplier::get);
        mRootUiTabObserver.swapToTab(mActivityTabProvider.get());

        // TODO(crbug.com/40946488): Consider register this drag listener to other views besides
        // CVH.
        // Instantiating ChromeTabbedOnDragListener on tablets since tab drags is enabled only via
        // tablet tab strip.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            ChromeTabbedOnDragListener chromeTabbedOnDragListener =
                    new ChromeTabbedOnDragListener(
                            mMultiInstanceManager,
                            mTabModelSelectorSupplier.get(),
                            mWindowAndroid,
                            mLayoutStateProviderOneShotSupplier);

            mCompositorViewHolderSupplier.get().setOnDragListener(chromeTabbedOnDragListener);

            // Disable touch event while drag is in progress.
            mDragDropTouchObserver = e -> DragDropGlobalState.hasValue();
            ((CoordinatorLayoutForPointer) mCoordinator)
                    .addTouchEventObserver(mDragDropTouchObserver);
        }

        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            getToolbarManager().enableBottomControls();
        }

        SupplierUtils.waitForAll(
                mCallbackController.makeCancelable(
                        () -> {
                            initializeIph(
                                    mProfileSupplier.get().getOriginalProfile(),
                                    mIntentMetadataOneshotSupplier.get().getIsIntentWithEffect());
                        }),
                mIntentMetadataOneshotSupplier,
                mProfileSupplier);

        // TODO(crbug.com/40736706): Investigate switching to per-Activity coordinator that
        // uses signals from the current Tab to decide when to show the PWA install bottom sheet
        // rather than relying on unowned user data.
        mPwaBottomSheetController =
                PwaBottomSheetControllerFactory.createPwaBottomSheetController(mActivity);
        PwaBottomSheetControllerFactory.attach(mWindowAndroid, mPwaBottomSheetController);
        initCommerceSubscriptionsService();
        initUndoGroupSnackbarController();
        initTabStripTransitionCoordinator();
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARE_PAGE_INFO)) {
            PageInfoSharingControllerImpl.getInstance().initialize();
        }

        new OneShotCallback<>(mProfileSupplier, this::maybeInitMessageDelegateOnProfile);
    }

    @Override
    protected void initProfileDependentFeatures(Profile currentlySelectedProfile) {
        super.initProfileDependentFeatures(currentlySelectedProfile);
        Profile originalProfile = currentlySelectedProfile.getOriginalProfile();
        if (TabGroupSyncFeatures.isTabGroupSyncEnabled(originalProfile)) {
            mTabGroupSyncController =
                    new TabGroupSyncController(
                            mTabModelSelectorSupplier.get(),
                            mTabCreatorManagerSupplier.get(),
                            TabGroupSyncServiceFactory.getForProfile(originalProfile),
                            UserPrefs.get(originalProfile),
                            () -> {
                                return MultiWindowUtils.getInstanceCount() <= 1
                                        || ApplicationStatus.getLastTrackedFocusedActivity()
                                                == mActivity;
                            });
            mTabGroupUiActionHandlerSupplier.set(mTabGroupSyncController);
        }
    }

    /** Creates an instance of {@link IncognitoReauthCoordinatorFactory} for tabbed activity. */
    @Override
    protected IncognitoReauthCoordinatorFactory getIncognitoReauthCoordinatorFactory(
            Profile profile) {
        IncognitoReauthCoordinatorFactory incognitoReauthCoordinatorFactory =
                new IncognitoReauthCoordinatorFactory(
                        mActivity,
                        mTabModelSelectorSupplier.get(),
                        mModalDialogManagerSupplier.get(),
                        new IncognitoReauthManager(mActivity, profile),
                        mLayoutManager,
                        mHubManagerSupplier,
                        /* showRegularOverviewIntent= */ null,
                        /* isTabbedActivity= */ true);

        mIncognitoTabSwitcherSupplier.onAvailable(
                mCallbackController.makeCancelable(
                        (tabSwitcher) -> {
                            var tabSwitcherCustomViewManager =
                                    tabSwitcher.getTabSwitcherCustomViewManager();
                            if (tabSwitcherCustomViewManager != null) {
                                incognitoReauthCoordinatorFactory.setTabSwitcherCustomViewManager(
                                        tabSwitcherCustomViewManager);
                            }
                        }));

        return incognitoReauthCoordinatorFactory;
    }

    @Override
    protected void setLayoutStateProvider(LayoutStateProvider layoutStateProvider) {
        super.setLayoutStateProvider(layoutStateProvider);
        if (mGestureNavLayoutObserver != null) {
            layoutStateProvider.addObserver(mGestureNavLayoutObserver);
        }
    }

    // Protected class methods

    @Override
    protected void onLayoutManagerAvailable(LayoutManagerImpl layoutManager) {
        super.onLayoutManagerAvailable(layoutManager);

        initStatusIndicatorCoordinator(layoutManager);
        mLayoutManager = layoutManager;
    }

    @Override
    protected boolean canShowMenuUpdateBadge() {
        return true;
    }

    @Override
    protected boolean shouldInitializeMerchantTrustSignals() {
        return true;
    }

    @Override
    protected ScrimCoordinator buildScrimWidget() {
        ScrimCoordinator.SystemUiScrimDelegate delegate =
                new ScrimCoordinator.SystemUiScrimDelegate() {
                    @Override
                    public void setScrimColor(int scrimColor) {
                        mStatusBarColorController.setScrimColor(scrimColor);
                    }

                    @Override
                    public void setStatusBarScrimFraction(float scrimFraction) {
                        mStatusBarColorController.setStatusBarScrimFraction(scrimFraction);
                    }

                    @Override
                    public void setNavigationBarScrimFraction(float scrimFraction) {
                        // When drawing edge to edge, scrim already draws over the nav bar region.
                        // No need to change the nav bar color.
                        var edgeToEdgeController = mEdgeToEdgeControllerSupplier.get();
                        if (edgeToEdgeController != null
                                && edgeToEdgeController.isDrawingToEdge()) {
                            return;
                        }

                        TabbedNavigationBarColorController controller =
                                mSystemUiCoordinator.getNavigationBarColorController();
                        if (controller == null) {
                            return;
                        }
                        controller.setNavigationBarScrimFraction(scrimFraction);
                    }
                };
        return new ScrimCoordinator(
                mActivity,
                delegate,
                mCoordinator,
                mCoordinator
                        .getContext()
                        .getColor(R.color.omnibox_focused_fading_background_color));
    }

    // Package Private class methods
    void recordPrivacySandboxActivityType(Profile profile) {
        // Records the current ActivityType using a PrivacySandbox Bridge
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PRIVACY_SANDBOX_ACTIVITY_TYPE_STORAGE)) {
            int privacySandboxStorageActivityType =
                    ActivityTypeMapper.toPrivacySandboxStorageActivityType(ActivityType.TABBED);

            PrivacySandboxBridge privacySandboxBridge = new PrivacySandboxBridge(profile);
            privacySandboxBridge.recordActivityType(privacySandboxStorageActivityType);
        }
    }

    boolean maybeTriggerPsDialogSuppression(Profile profile) {
        // Handles whether the PS Dialog should be suppressed, logs whether it was suppressed and
        // returns whether a promo was triggered
        Tab tab = mActivityTabProvider.get();

        boolean isTabLaunchedFromExternalApp =
                tab != null && tab.getLaunchType() == TabLaunchType.FROM_EXTERNAL_APP;
        boolean shouldSuppressPsDialogForExternalAppLaunches =
                ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4,
                        "suppress-dialog-for-external-app-launches",
                        true);
        boolean shouldSuppressPsDialog =
                isTabLaunchedFromExternalApp && shouldSuppressPsDialogForExternalAppLaunches;

        String histogramName =
                "Startup.Android.PrivacySandbox.DialogNotShownDueToTabLaunchedFromExternalApp";
        RecordHistogram.recordBooleanHistogram(histogramName, shouldSuppressPsDialog);

        if (!shouldSuppressPsDialog) {
            return PrivacySandboxDialogController.maybeLaunchPrivacySandboxDialog(
                    mActivity, profile, SurfaceType.BR_APP, mWindowAndroid);
        }

        return false;
    }

    private boolean maybeTriggerPrivacySandboxPrompt(Profile profile) {
        return maybeTriggerPsDialogSuppression(profile);
    }

    // Private class methods
    private void initializeIph(Profile profile, boolean intentWithEffect) {
        if (mActivity == null) return;
        mToolbarButtonInProductHelpController =
                new ToolbarButtonInProductHelpController(
                        mActivity,
                        mWindowAndroid,
                        mAppMenuCoordinator,
                        mActivityLifecycleDispatcher,
                        profile,
                        mActivityTabProvider,
                        mIsInOverviewModeSupplier,
                        mToolbarManager.getMenuButtonView(),
                        mToolbarManager.getSecurityIconView());
        mReadAloudIphController =
                new ReadAloudIPHController(
                        mActivity,
                        profile,
                        getToolbarManager().getMenuButtonView(),
                        mAppMenuCoordinator.getAppMenuHandler(),
                        mActivityTabProvider,
                        mReadAloudControllerSupplier,
                        /* showAppMenuTextBubble= */ true);
        mReadLaterIphController =
                new ReadLaterIPHController(
                        mActivity,
                        profile,
                        getToolbarManager().getMenuButtonView(),
                        mAppMenuCoordinator.getAppMenuHandler());

        // Initializes Privacy Sandbox related logic
        recordPrivacySandboxActivityType(profile);

        boolean didTriggerPromo = maybeShowRequiredPromptsAndPromos(profile, intentWithEffect);

        if (!didTriggerPromo) {
            didTriggerPromo =
                    RequestDesktopUtils.maybeShowDefaultEnableGlobalSettingMessage(
                            profile, mMessageDispatcher, mActivity);
        }

        if (!didTriggerPromo) {
            mToolbarButtonInProductHelpController.showColdStartIPH();
            mReadLaterIphController.showColdStartIPH();
            if (MultiWindowUtils.instanceSwitcherEnabled()
                    && MultiWindowUtils.shouldShowManageWindowsMenu()) {
                MultiInstanceIphController.maybeShowInProductHelp(
                        mActivity,
                        profile,
                        getToolbarManager().getMenuButtonView(),
                        mAppMenuCoordinator.getAppMenuHandler(),
                        R.id.manage_all_windows_menu_id);
            }
            DesktopSiteSettingsIPHController.create(
                    mActivity,
                    mWindowAndroid,
                    mActivityTabProvider,
                    profile,
                    getToolbarManager().getMenuButtonView(),
                    mAppMenuCoordinator.getAppMenuHandler());
        }
        mPromoShownOneshotSupplier.set(didTriggerPromo);

        if (mOfflineIndicatorController != null) {
            // Initialize the OfflineIndicatorInProductHelpController if the
            // mOfflineIndicatorController is enabled and initialized. For example, it wouldn't be
            // initialized if the OfflineIndicatorV2 feature is disabled.
            assert mOfflineIndicatorInProductHelpController == null;
            mOfflineIndicatorInProductHelpController =
                    new OfflineIndicatorInProductHelpController(
                            mActivity,
                            profile,
                            mToolbarManager,
                            mAppMenuCoordinator.getAppMenuHandler(),
                            mStatusIndicatorCoordinator);
        }

        new LinkToTextIPHController(
                mActivityTabProvider, mTabModelSelectorSupplier.get(), mProfileSupplier);
        if (!didTriggerPromo
                && mWindowAndroid.getWindow() != null
                && LocalizationUtils.isLayoutRtl()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.BACK_FORWARD_TRANSITIONS)
                && ChromeFeatureList.isEnabled(FeatureConstants.IPH_RTL_GESTURE_NAVIGATION)
                && !UiUtils.isGestureNavigationMode(mWindowAndroid.getWindow())) {
            mRtlGestureNavIphController =
                    new RtlGestureNavIphController(mActivityTabProvider, mProfileSupplier);
        }

        Tab tab = mActivityTabProvider.get();

        if (!didTriggerPromo
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)) {
            // TODO(crbug.com/40793438): Investigate locking feature engagement system during
            // "second run promos" to avoid !didTriggerPromo check.
            WebContentsDarkModeMessageController.attemptToSendMessage(
                    mActivity,
                    tab != null ? tab.getProfile() : profile,
                    tab != null ? tab.getWebContents() : null,
                    mMessageDispatcher);
        }

        if (WebFeedBridge.isWebFeedEnabled()) {
            mWebFeedFollowIntroController =
                    new WebFeedFollowIntroController(
                            mActivity,
                            profile,
                            mAppMenuCoordinator.getAppMenuHandler(),
                            mActivityTabProvider,
                            mToolbarManager.getMenuButtonView(),
                            () -> {
                                mTabCreatorManagerSupplier
                                        .get()
                                        .getTabCreator(/* incognito= */ false)
                                        .launchUrl(
                                                NewTabPageUtils.encodeNtpUrl(
                                                        NewTabPageLaunchOrigin.WEB_FEED),
                                                TabLaunchType.FROM_CHROME_UI);
                            },
                            mModalDialogManagerSupplier.get(),
                            mSnackbarManagerSupplier.get());
        }

        if (!didTriggerPromo && PageZoomCoordinator.shouldShowMenuItem()) {
            // Page Zoom IPH should only show if the menu item is visible, and not on NTP or CCT.
            if (tab != null && tab.getWebContents() != null && !tab.isNativePage()) {
                PageZoomIPHController mPageZoomIphController =
                        new PageZoomIPHController(
                                mActivity,
                                profile,
                                mAppMenuCoordinator.getAppMenuHandler(),
                                mToolbarManager.getMenuButtonView());
                mPageZoomIphController.showColdStartIPH();
            }
        }
    }

    private void updateTopControlsHeight() {
        if (mToolbarManager == null) return;

        // TODO(crbug/331844971): Do a smooth transition head into DW mode.
        final boolean animate =
                !sDisableTopControlsAnimationForTesting
                        && !AppHeaderUtils.isAppInDesktopWindow(mAppHeaderCoordinator);
        final BrowserControlsSizer browserControlsSizer = mBrowserControlsManager;

        boolean isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity);
        int topControlsNewHeight;
        // This method can be called when the toolbar didn't go through a layout pass (e.g. when
        // theme switches in settings, activity recreates), so getToolbar().getHeight() returns
        // 0.
        // TODO(crbug.com/40943442): Remove the reference to toolbar_height_no_shadow.
        final int toolbarHeight =
                mActivity.getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow);
        final int tabStripHeight = mToolbarManager.getTabStripHeightSupplier().get();
        topControlsNewHeight = toolbarHeight + tabStripHeight + mStatusIndicatorHeight;
        if (tabStripHeight > 0 && !isTablet) {
            String msg =
                    "Non-zero tab strip height found on non-tablet form factor. tabStripHeight="
                            + " "
                            + tabStripHeight
                            + " toolbarHeight= "
                            + toolbarHeight
                            + " statusIndicatorHeight= "
                            + mStatusIndicatorHeight;
            ChromePureJavaExceptionReporter.reportJavaException(new Throwable(msg));
        }

        browserControlsSizer.setAnimateBrowserControlsHeightChanges(animate);
        browserControlsSizer.setTopControlsHeight(topControlsNewHeight, mStatusIndicatorHeight);
        if (animate) browserControlsSizer.setAnimateBrowserControlsHeightChanges(false);
    }

    private void initCommerceSubscriptionsService() {
        SupplierUtils.waitForAll(
                mCallbackController.makeCancelable(
                        () -> {
                            mCommerceSubscriptionsService =
                                    CommerceSubscriptionsServiceFactory.getInstance()
                                            .getForProfile(mProfileSupplier.get());
                            mCommerceSubscriptionsService.initDeferredStartupForActivity(
                                    mTabModelSelectorSupplier.get(), mActivityLifecycleDispatcher);
                        }),
                mTabModelSelectorSupplier,
                mProfileSupplier);
    }

    private void initUndoGroupSnackbarController() {
        mUndoGroupSnackbarController =
                new UndoGroupSnackbarController(
                        mActivity, mTabModelSelectorSupplier.get(), mSnackbarManagerSupplier.get());
    }

    private void initStatusIndicatorCoordinator(LayoutManagerImpl layoutManager) {
        // TODO(crbug.com/40112282): Disable on tablets for now as we need to do one or two extra
        // things for tablets.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            return;
        }

        mStatusIndicatorCoordinator =
                new StatusIndicatorCoordinator(
                        mActivity,
                        mCompositorViewHolderSupplier.get().getResourceManager(),
                        mBrowserControlsManager,
                        mTabObscuringHandlerSupplier.get(),
                        mStatusBarColorController::getStatusBarColorWithoutStatusIndicator,
                        mCanAnimateBrowserControls,
                        layoutManager::requestUpdate);
        layoutManager.addSceneOverlay(mStatusIndicatorCoordinator.getSceneLayer());
        mStatusIndicatorObserver =
                new StatusIndicatorCoordinator.StatusIndicatorObserver() {
                    @Override
                    public void onStatusIndicatorHeightChanged(int indicatorHeight) {
                        mStatusIndicatorHeight = indicatorHeight;
                        updateTopControlsHeight();
                        HubManager hubManager = mHubManagerSupplier.get();
                        if (hubManager != null) {
                            hubManager.setStatusIndicatorHeight(indicatorHeight);
                        }
                    }
                };
        mStatusIndicatorCoordinator.addObserver(mStatusIndicatorObserver);
        mStatusIndicatorCoordinator.addObserver(mStatusBarColorController);
        mHubManagerSupplier.onAvailable(
                hubManager -> {
                    hubManager.setStatusIndicatorHeight(mStatusIndicatorHeight);
                });

        ObservableSupplierImpl<Boolean> isUrlBarFocusedSupplier = new ObservableSupplierImpl<>();
        isUrlBarFocusedSupplier.set(mToolbarManager.isUrlBarFocused());
        mUrlFocusChangeListener =
                new UrlFocusChangeListener() {
                    @Override
                    public void onUrlFocusChange(boolean hasFocus) {
                        // Offline indicator should assume the UrlBar is focused if it's focusing.
                        if (hasFocus) {
                            isUrlBarFocusedSupplier.set(true);
                        }
                    }

                    @Override
                    public void onUrlAnimationFinished(boolean hasFocus) {
                        // Wait for the animation to finish before notifying that UrlBar is
                        // unfocused.
                        if (!hasFocus) {
                            isUrlBarFocusedSupplier.set(false);
                        }
                    }
                };
        mOfflineIndicatorController =
                new OfflineIndicatorControllerV2(
                        mActivity,
                        mStatusIndicatorCoordinator,
                        isUrlBarFocusedSupplier,
                        mCanAnimateBrowserControls);
        if (mToolbarManager.getOmniboxStub() != null) {
            mToolbarManager.getOmniboxStub().addUrlFocusChangeListener(mUrlFocusChangeListener);
        }
    }

    @Override
    protected Destroyable createEdgeToEdgeBottomChin() {
        return EdgeToEdgeControllerFactory.createBottomChin(
                mActivity.findViewById(R.id.edge_to_edge_bottom_chin),
                mWindowAndroid.getKeyboardDelegate(),
                mLayoutManager,
                mEdgeToEdgeControllerSupplier.get(),
                mSystemUiCoordinator.getNavigationBarColorController(),
                mBottomControlsStacker,
                mFullscreenManager);
    }

    private void initTabStripTransitionCoordinator() {
        // Tab strip transition is only supported for tablets.
        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) return;

        mOnTabStripHeightChangedCallback = (height) -> updateTopControlsHeight();
        mToolbarManager.getTabStripHeightSupplier().addObserver(mOnTabStripHeightChangedCallback);
    }

    @SuppressWarnings("NewApi") // OS version check is done via helper method.
    private void initAppHeaderCoordinator(Bundle savedInstanceState) {
        boolean isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity);
        if (!ToolbarFeatures.isTabStripWindowLayoutOptimizationEnabled(isTablet)) {
            return;
        }

        mAppHeaderCoordinator =
                new AppHeaderCoordinator(
                        mActivity,
                        mActivity.getWindow().getDecorView().getRootView(),
                        mBrowserControlsManager.getBrowserVisibilityDelegate(),
                        mInsetObserver,
                        mActivityLifecycleDispatcher,
                        savedInstanceState);
    }

    private void maybeInitMessageDelegateOnProfile(Profile profile) {
        DataSharingService dataSharingService = DataSharingServiceFactory.getForProfile(profile);
        @NonNull ServiceStatus serviceStatus = dataSharingService.getServiceStatus();
        if (!serviceStatus.isAllowedToJoin()) return;

        TabModelUtils.onInitializedTabModelSelector(mTabModelSelectorSupplier)
                .runSyncOrOnAvailable(
                        selector -> {
                            mInstantMessageDelegateImpl =
                                    InstantMessageDelegateFactory.getForProfile(profile);
                            TabGroupModelFilter tabGroupModelFilter =
                                    (TabGroupModelFilter)
                                            selector.getTabModelFilterProvider()
                                                    .getTabModelFilter(/* incognito= */ false);
                            mInstantMessageDelegateImpl.attachWindow(
                                    mWindowAndroid, tabGroupModelFilter);
                        });
    }

    @Override
    public DesktopWindowStateProvider getDesktopWindowStateProvider() {
        return mAppHeaderCoordinator;
    }

    /** Returns the {@link TabGroupSyncController} if it has been created yet. */
    public TabGroupSyncController getTabGroupSyncController() {
        return mTabGroupSyncController;
    }

    @Override
    protected boolean supportsEdgeToEdge() {
        return EdgeToEdgeControllerFactory.isSupportedConfiguration(mActivity);
    }

    public StatusIndicatorCoordinator getStatusIndicatorCoordinatorForTesting() {
        return mStatusIndicatorCoordinator;
    }

    public HistoryNavigationCoordinator getHistoryNavigationCoordinatorForTesting() {
        return mHistoryNavigationCoordinator;
    }

    public NavigationSheet getNavigationSheetForTesting() {
        return mNavigationSheet;
    }

    public RtlGestureNavIphController getRtlGestureNavIphControllerForTesting() {
        return mRtlGestureNavIphController;
    }

    /** Called when a link is copied through context menu. */
    public void onContextMenuCopyLink() {
        // TODO(crbug.com/40732234): Find a better way of passing event for IPH.
        mReadLaterIphController.onCopyContextMenuItemClicked();
    }

    /**
     * Triggers the display of an appropriate required prompt or promo if any.
     *
     * <p>Check and trigger here "required prompts", the ones that are very important for Chrome
     * usage, or for privacy, regulatory etc. reasons. For less critical ones, for example
     * suggestions to enable features, prefer adding them to {@link #maybeShowPromo}, which can be
     * skipped via command line, prefs or other Chrome state.
     *
     * @return whether a prompt or promo is actually displayed.
     */
    private boolean maybeShowRequiredPromptsAndPromos(Profile profile, boolean intentWithEffect) {
        if (SearchEnginesFeatures.isEnabled(SearchEnginesFeatures.CLAY_BLOCKING)) {
            if (ChoiceDialogCoordinator.maybeShow(
                    mActivity, mModalDialogManagerSupplier.get(), mActivityLifecycleDispatcher)) {
                return true;
            }
        }

        if (maybeTriggerPrivacySandboxPrompt(profile)) {
            return true;
        }

        final Supplier<RationaleDelegate> rationaleUIDelegateSupplier;
        if (NotificationPermissionController.shouldUseBottomSheetRationaleUi()) {
            rationaleUIDelegateSupplier =
                    () ->
                            new NotificationPermissionRationaleBottomSheet(
                                    mActivity, getBottomSheetController());
        } else {
            rationaleUIDelegateSupplier =
                    () ->
                            new NotificationPermissionRationaleDialogController(
                                    mActivity, mModalDialogManagerSupplier.get());
        }
        mNotificationPermissionController =
                new NotificationPermissionController(mWindowAndroid, rationaleUIDelegateSupplier);
        NotificationPermissionController.attach(mWindowAndroid, mNotificationPermissionController);
        if (mNotificationPermissionController.requestPermissionIfNeeded(/* contextual= */ false)) {
            return true;
        }

        return triggerPromo(profile, intentWithEffect);
    }

    /**
     * Triggers the display of an appropriate promo, if any, returning true if a promo is actually
     * displayed.
     */
    private boolean triggerPromo(Profile profile, boolean intentWithEffect) {
        try (TraceEvent e = TraceEvent.scoped("TabbedRootUiCoordinator.triggerPromo")) {
            if (CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_STARTUP_PROMOS)) {
                return false;
            }

            SharedPreferencesManager preferenceManager = ChromeSharedPreferences.getInstance();
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
            isShowingPromo |= maybeForceShowPromoAtStartup(profile);

            if (!isShowingPromo
                    && !intentWithEffect
                    && FirstRunStatus.getFirstRunFlowComplete()
                    && preferenceManager.readBoolean(
                            ChromePreferenceKeys.PROMOS_SKIPPED_ON_FIRST_START, false)) {
                isShowingPromo = maybeShowPromo(profile);
            } else {
                preferenceManager.writeBoolean(
                        ChromePreferenceKeys.PROMOS_SKIPPED_ON_FIRST_START, true);
            }

            if (FirstRunStatus.isFirstRunTriggered()) {
                notifyPromosOfFirstRunTriggered();
            }

            return isShowingPromo;
        }
    }

    /** Runs any promos set by feature flag to force show at every startup. */
    private boolean maybeForceShowPromoAtStartup(Profile profile) {
        // Any promo that has a force-show feature flag should be added to this list (and of course
        // any promo that you want to trigger at every startup (temporarily for debugging and/or
        // development).
        if (PwaRestorePromoUtils.maybeForceShowPromo(profile, mWindowAndroid)) return true;

        return false;
    }

    /** Notifies promos of the First Run Experience having triggered during this launch. */
    private void notifyPromosOfFirstRunTriggered() {
        PwaRestorePromoUtils.notifyFirstRunPromoTriggered();
    }

    private boolean maybeShowPromo(Profile profile) {
        // NOTE: Only one promo can be shown in one run to avoid nagging users too much.

        // The PWA Restore promotion runs when we've detected that a user has switched to a new
        // device but is leaving behind web apps on the old device. It promotes the idea that the
        // user can restore their web apps from their old device (if they have any), and as such it
        // is most effective when shown shortly after the first-run experience. It is therefore
        // at the front of the list of promotions.
        if (PwaRestorePromoUtils.launchPromoIfNeeded(profile, mWindowAndroid)) {
            return true;
        }
        if (FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                mActivity,
                profile,
                SyncConsentActivityLauncherImpl.get(),
                SigninAndHistorySyncActivityLauncherImpl.get(),
                VersionInfo.getProductMajorVersion())) {
            return true;
        }
        if (DefaultBrowserPromoUtils.getInstance()
                .prepareLaunchPromoIfNeeded(
                        mActivity,
                        mWindowAndroid,
                        TrackerFactory.getTrackerForProfile(profile),
                        /* ignoreMaxCount= */ false)) {
            return true;
        }
        return AppLanguagePromoDialog.maybeShowPrompt(
                mActivity,
                profile,
                mModalDialogManagerSupplier,
                () -> ApplicationLifetime.terminate(true));
    }

    public static void setDisableTopControlsAnimationsForTesting(boolean disable) {
        sDisableTopControlsAnimationForTesting = disable;
        ResettersForTesting.register(() -> sDisableTopControlsAnimationForTesting = false);
    }
}
