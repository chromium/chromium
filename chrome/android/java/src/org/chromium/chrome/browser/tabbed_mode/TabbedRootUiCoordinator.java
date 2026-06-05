// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.os.PersistableBundle;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AppCompatActivity;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.CommandLine;
import org.chromium.base.DeviceInfo;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.Token;
import org.chromium.base.TraceEvent;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneShotCallback;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.ChromeInactivityTracker;
import org.chromium.chrome.browser.ChromeInactivityTracker.InactivityObserver;
import org.chromium.chrome.browser.SwipeRefreshHandler;
import org.chromium.chrome.browser.accessibility.PageZoomIphController;
import org.chromium.chrome.browser.actor.ui.ActorOverlayCoordinator;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerOpener;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkOpener;
import org.chromium.chrome.browser.bookmarks.BookmarkOpenerImpl;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarCoordinator;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarUtils;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarVisibilityProvider;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarVisibilityProvider.BookmarkBarVisibilityObserver;
import org.chromium.chrome.browser.browser_controls.BottomOverscrollHandler;
import org.chromium.chrome.browser.collaboration.CollaborationControllerDelegateFactory;
import org.chromium.chrome.browser.collaboration.CollaborationControllerDelegateImpl;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.collaboration.messaging.MessagingBackendServiceFactory;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulatorFactory;
import org.chromium.chrome.browser.contextual_tasks.ContextualTasksBridge;
import org.chromium.chrome.browser.contextual_tasks.ContextualTasksFuseboxManagerImpl;
import org.chromium.chrome.browser.contextual_tasks.fusebox.ContextualTasksFusebox.ContextualTasksFuseboxConfig;
import org.chromium.chrome.browser.contextual_tasks.fusebox.ContextualTasksFuseboxManager;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.data_sharing.DataSharingNotificationManager;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabGroupUtils;
import org.chromium.chrome.browser.data_sharing.DataSharingTabGroupsDelegate;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.data_sharing.InstantMessageDelegateFactory;
import org.chromium.chrome.browser.data_sharing.InstantMessageDelegateImpl;
import org.chromium.chrome.browser.desktop_site.DesktopSiteSettingsIphController;
import org.chromium.chrome.browser.dom_distiller.ReaderModeIphController;
import org.chromium.chrome.browser.dragdrop.ChromeTabbedOnDragListener;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.extensions.ExtensionsUrlOverrideRegistryManagerFactory;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.gesturenav.BackActionDelegate;
import org.chromium.chrome.browser.gesturenav.GestureUserEducationIphController;
import org.chromium.chrome.browser.gesturenav.HistoryNavigationCoordinator;
import org.chromium.chrome.browser.gesturenav.NavigationSheet;
import org.chromium.chrome.browser.gesturenav.TabbedSheetDelegate;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicKeyedService.GlicInvocationSource;
import org.chromium.chrome.browser.glic.GlicKeyedServiceHandler;
import org.chromium.chrome.browser.glic.GlicMetrics;
import org.chromium.chrome.browser.glic.GlicNavigationUtils;
import org.chromium.chrome.browser.glic.GlicPromoCoordinator;
import org.chromium.chrome.browser.glic.GlicUiCoordinator;
import org.chromium.chrome.browser.history.HistoryManagerUtils;
import org.chromium.chrome.browser.hub.HubManager;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthCoordinatorFactory;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponent;
import org.chromium.chrome.browser.language.AppLanguagePromoDialog;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifetime.ApplicationLifetime;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.multiwindow.MultiInstanceIphController;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.night_mode.WebContentsDarkModeMessageController;
import org.chromium.chrome.browser.notifications.permissions.NotificationPermissionController;
import org.chromium.chrome.browser.notifications.permissions.NotificationPermissionController.RationaleDelegate;
import org.chromium.chrome.browser.notifications.permissions.NotificationPermissionRationaleDialogController;
import org.chromium.chrome.browser.notifications.tips.TipsOptInCoordinator;
import org.chromium.chrome.browser.notifications.tips.TipsUtils;
import org.chromium.chrome.browser.ntp.NewTabPageLocationPolicyManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.policy.NtpCustomizationPolicyManager;
import org.chromium.chrome.browser.ntp_customization.theme.NtpSyncedThemeManager;
import org.chromium.chrome.browser.offlinepages.indicator.OfflineIndicatorControllerV2;
import org.chromium.chrome.browser.offlinepages.indicator.OfflineIndicatorInProductHelpController;
import org.chromium.chrome.browser.omnibox.LocationBarEmbedder;
import org.chromium.chrome.browser.omnibox.OmniboxChipManager;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxActionDelegateImpl;
import org.chromium.chrome.browser.open_in_app.OpenInAppUtils;
import org.chromium.chrome.browser.open_in_app.TabbedOpenInAppEntryPoint;
import org.chromium.chrome.browser.pdf.PdfPageIphController;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.privacy.settings.PrivacySettings;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandbox3pcdRollbackMessageController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.read_later.ReadLaterIphController;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.readaloud.ReadAloudIphController;
import org.chromium.chrome.browser.readaloud.ReadAloudMetrics.ReasonForStoppingPlayback;
import org.chromium.chrome.browser.safe_browsing.AdvancedProtectionCoordinator;
import org.chromium.chrome.browser.search_engines.choice_screen.ChoiceDialogCoordinator;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextIphController;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.status_indicator.StatusIndicatorCoordinator;
import org.chromium.chrome.browser.status_indicator.StatusIndicatorCoordinator.StatusIndicatorObserver;
import org.chromium.chrome.browser.subscriptions.CommerceSubscriptionsService;
import org.chromium.chrome.browser.subscriptions.CommerceSubscriptionsServiceFactory;
import org.chromium.chrome.browser.sync.synced_set_up.CrossDeviceSettingImporter;
import org.chromium.chrome.browser.tab.RequestDesktopUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;
import org.chromium.chrome.browser.tab.TabFavicon;
import org.chromium.chrome.browser.tab_bottom_sheet.CoBrowseViewFactory;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetManager;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetManagerImpl;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetUtils;
import org.chromium.chrome.browser.tab_group_suggestion.toolbar.GroupSuggestionsButtonController;
import org.chromium.chrome.browser.tab_group_suggestion.toolbar.GroupSuggestionsButtonControllerFactory;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncControllerImpl;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;
import org.chromium.chrome.browser.tab_ui.TabSwitcherUtils;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.tabwindow.WindowId;
import org.chromium.chrome.browser.tasks.tab_management.FaviconResolver;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupFaviconCluster;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListFaviconResolverFactory;
import org.chromium.chrome.browser.tasks.tab_management.TabUiUtils;
import org.chromium.chrome.browser.tasks.tab_management.UndoGroupSnackbarController;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabsSideUiCoordinator;
import org.chromium.chrome.browser.toolbar.ToolbarButtonInProductHelpController;
import org.chromium.chrome.browser.toolbar.ToolbarFeatures;
import org.chromium.chrome.browser.toolbar.ToolbarIntentMetadata;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarBehavior;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarPrefs;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.chrome.browser.ui.actions.ActionUtils;
import org.chromium.chrome.browser.ui.app_rating.AppRatingPromoController;
import org.chromium.chrome.browser.ui.appmenu.AppMenuBlocker;
import org.chromium.chrome.browser.ui.appmenu.AppMenuDelegate;
import org.chromium.chrome.browser.ui.bottombar.BottomBarConfigUtils;
import org.chromium.chrome.browser.ui.bottombar.BottomBarHostManager;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeatureKey;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderCoordinator;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerFactory;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.chrome.browser.ui.edge_to_edge.TopInsetProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.side_panel.AndroidSidePanelEnabledFn;
import org.chromium.chrome.browser.ui.side_panel.SidePanelCoordinatorAndroid;
import org.chromium.chrome.browser.ui.side_panel.SidePanelCoordinatorAndroidFactory;
import org.chromium.chrome.browser.ui.side_panel.SidePanelRegistryBridgeFactory;
import org.chromium.chrome.browser.ui.side_panel.WindowScopedSidePanelRegistryBridge;
import org.chromium.chrome.browser.ui.side_panel_container.SidePanelContainerCoordinator;
import org.chromium.chrome.browser.ui.side_panel_container.SidePanelContainerCoordinatorFactory;
import org.chromium.chrome.browser.ui.side_panel_container.dev.SidePanelDevFeature;
import org.chromium.chrome.browser.ui.side_panel_container.dev.SidePanelDevFeatureFactory;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinatorFactory;
import org.chromium.chrome.browser.ui.side_ui.SideUiStateProvider;
import org.chromium.chrome.browser.ui.side_ui.ViewMarginAdjusterForSideUi;
import org.chromium.chrome.browser.ui.signin.ForcedSigninController;
import org.chromium.chrome.browser.ui.signin.FullscreenSigninPromoLauncher;
import org.chromium.chrome.browser.ui.system.StatusBarColorController.StatusBarColorProvider;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.browser.user_education.UserEducationUtils;
import org.chromium.chrome.browser.user_education.UserEducationUtils.OptionalPromoType;
import org.chromium.chrome.browser.webapps.PwaRestorePromoUtils;
import org.chromium.components.browser_ui.accessibility.PageZoomUtils;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.widget.CoordinatorLayoutForPointer;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.TouchEventObserver;
import org.chromium.components.browser_ui.widget.loading.LoadingFullscreenCoordinator;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager.ScrimClient;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncController;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.webapps.bottomsheet.PwaBottomSheetController;
import org.chromium.components.webapps.bottomsheet.PwaBottomSheetControllerFactory;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.UiAndroidFeatureList;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.dragdrop.DragDropGlobalState;
import org.chromium.ui.edge_to_edge.EdgeToEdgeManager;
import org.chromium.ui.edge_to_edge.EdgeToEdgeStateProvider;
import org.chromium.ui.edge_to_edge.SystemBarColorHelper;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

import java.util.Collections;
import java.util.function.BooleanSupplier;
import java.util.function.Function;
import java.util.function.Supplier;

/** A {@link RootUiCoordinator} variant that controls tabbed-mode specific UI. */
@NullMarked
public class TabbedRootUiCoordinator extends RootUiCoordinator {
    // The tag length is restricted to be at most 20 characters.
    private static final String TAG = "TabbedRootUiCoord";
    private static boolean sDisableTopControlsAnimationForTesting;
    private final RootUiTabObserver mRootUiTabObserver;
    private @Nullable TabbedSystemUiCoordinator mSystemUiCoordinator;
    private @Nullable TabGroupSyncController mTabGroupSyncController;
    private final OneshotSupplierImpl<TabGroupUiActionHandler> mTabGroupUiActionHandlerSupplier =
            new OneshotSupplierImpl<>();
    private @Nullable StatusIndicatorCoordinator mStatusIndicatorCoordinator;
    private @Nullable StatusIndicatorObserver mStatusIndicatorObserver;
    private @Nullable OfflineIndicatorControllerV2 mOfflineIndicatorController;
    private @Nullable OfflineIndicatorInProductHelpController
            mOfflineIndicatorInProductHelpController;
    private @Nullable ReadAloudIphController mReadAloudIphController;
    private @Nullable ReadLaterIphController mReadLaterIphController;
    private @Nullable DesktopSiteSettingsIphController mDesktopSiteSettingsIphController;
    private @Nullable PdfPageIphController mPdfPageIphController;
    private @Nullable UrlFocusChangeListener mUrlFocusChangeListener;
    private @Nullable ToolbarButtonInProductHelpController mToolbarButtonInProductHelpController;
    private @Nullable PwaBottomSheetController mPwaBottomSheetController;
    private @Nullable NotificationPermissionController mNotificationPermissionController;
    private @Nullable HistoryNavigationCoordinator mHistoryNavigationCoordinator;
    private @Nullable NavigationSheet mNavigationSheet;
    private @Nullable LayoutManagerImpl mLayoutManager;
    private @Nullable CommerceSubscriptionsService mCommerceSubscriptionsService;
    private @Nullable UndoGroupSnackbarController mUndoGroupSnackbarController;
    private @Nullable PrivacySandbox3pcdRollbackMessageController
            mPrivacySandbox3pcdRollbackMessageController;
    private @Nullable GestureUserEducationIphController mGestureUserEducationIphController;
    private final InsetObserver mInsetObserver;
    private final Function<Tab, Boolean> mBackButtonShouldCloseTabFn;
    private final Callback<@Nullable Tab> mSendToBackground;
    private final LayoutStateProvider.LayoutStateObserver mGestureNavLayoutObserver;

    private final @Nullable MultiInstanceManager mMultiInstanceManager;
    private int mStatusIndicatorHeight;
    private final OneshotSupplier<HubManager> mHubManagerSupplier;
    private @Nullable TouchEventObserver mDragDropTouchObserver;
    private @Nullable ViewGroup mCoordinator;
    private final OneshotSupplierImpl<SystemBarColorHelper> mSystemBarColorHelperSupplier;
    private final MonotonicObservableSupplier<ManualFillingComponent>
            mManualFillingComponentSupplier;
    private final DataSharingTabManager mDataSharingTabManager;
    private final Supplier<Boolean> mCanAnimateBrowserControls;
    protected @Nullable InstantMessageDelegateImpl mInstantMessageDelegateImpl;
    private @Nullable BookmarkBarCoordinator mBookmarkBarCoordinator;
    private @Nullable BookmarkBarVisibilityProvider mBookmarkBarVisibilityProvider;
    private @Nullable BookmarkBarVisibilityObserver mBookmarkBarVisibilityObserver;
    private @Nullable Supplier<Integer> mBookmarkBarHeightSupplier;
    private @Nullable LoadingFullscreenCoordinator mLoadingFullscreenCoordinator;
    private @Nullable BookmarkOpener mBookmarkOpener;
    private @Nullable TabBottomSheetManager mTabBottomSheetManager;
    private @Nullable Callback<ReadAloudController> mTabBottomSheetReadAloudControllerCallback;
    private @Nullable ContextualTasksFuseboxManager mContextualTasksFuseboxManager;
    private @Nullable CoBrowseViewFactory mCoBrowseViewFactory;
    private final MonotonicObservableSupplier<BookmarkManagerOpener> mBookmarkManagerOpenerSupplier;
    private @Nullable AdvancedProtectionCoordinator mAdvancedProtectionCoordinator;
    private final KeyboardFocusRowManager mKeyboardFocusRowManager;
    private CharSequence mApplicationLabel;
    private @Nullable TipsOptInCoordinator mTipsOptInCoordinator;
    private @Nullable GlicPromoCoordinator mGlicPromoCoordinator;
    private boolean mPromosEvaluatedForCurrentForeground;
    private final OneshotSupplier<ChromeInactivityTracker> mInactivityTrackerSupplier;
    private final InactivityObserver mInactivityObserver;
    private @Nullable NtpSyncedThemeManager mNtpSyncedThemeManager;
    private final @Nullable CrossDeviceSettingImporter mCrossDeviceSettingImporter;
    private @Nullable SideUiCoordinator mSideUiCoordinator;
    private @Nullable SidePanelContainerCoordinator mSidePanelContainerCoordinator;
    private @Nullable SidePanelDevFeature mSidePanelDevFeature;
    private final OneshotSupplierImpl<Boolean> mTrackerInitializedOneshotSupplier =
            new OneshotSupplierImpl<>();
    private final OneshotSupplierImpl<SideUiStateProvider> mSideUiStateProviderSupplier =
            new OneshotSupplierImpl<>();
    private @Nullable ViewMarginAdjusterForSideUi mSecondaryUiContainerMarginAdjuster;
    private @Nullable ContextualTasksBridge mContextualTasksBridge;
    private @Nullable GlicUiCoordinator mGlicUiCoordinator;
    private @Nullable ForcedSigninController mForcedSigninController;
    private @Nullable VerticalTabsSideUiCoordinator mVerticalTabsSideUiCoordinator;

    // Activity tab observer that updates the current tab used by various UI components.
    private class RootUiTabObserver extends ActivityTabTabObserver {
        private @Nullable Tab mTab;

        private RootUiTabObserver(ActivityTabProvider activityTabProvider) {
            super(activityTabProvider);
        }

        @Override
        public void onObservingDifferentTab(@Nullable Tab tab) {
            swapToTab(tab);
        }

        @Override
        public void onTitleUpdated(Tab tab) {
            setActivityTitle(tab, /* isTabSwitcher= */ false);
        }

        private void swapToTab(@Nullable Tab tab) {
            if (mTab != null && !mTab.isDestroyed()) {
                var swipeHandler = SwipeRefreshHandler.from(mTab);
                swipeHandler.setNavigationCoordinator(null);
                swipeHandler.setBottomOverscrollHandler(null);
            }
            mTab = tab;

            if (tab != null) {
                var swipeHandler = SwipeRefreshHandler.from(tab);
                swipeHandler.setNavigationCoordinator(mHistoryNavigationCoordinator);
                if (UiAndroidFeatureList.sReportBottomOverscrolls.isEnabled()) {
                    swipeHandler.setBottomOverscrollHandler(
                            new BottomOverscrollHandler(mBrowserControlsManager));
                }
            }
            setActivityTitle(tab, /* isTabSwitcher= */ false);
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
     * @param tabCreatorManagerSupplier Supplies the {@link TabCreatorManager}.
     * @param fullscreenManager Manages the fullscreen state.
     * @param compositorViewHolderSupplier Supplies the {@link CompositorViewHolder}.
     * @param tabContentManagerSupplier Supplies the {@link TabContentManager}.
     * @param snackbarManagerSupplier Supplies the {@link SnackbarManager}.
     * @param edgeToEdgeSupplier Supplies the {@link EdgeToEdgeController}.
     * @param topInsetProvider The {@link TopInsetProvider} instance.
     * @param systemBarColorHelperSupplier Supplies the {@link SystemBarColorHelper} for the
     *     edge-to-edge bottom chin.
     * @param activityType The {@link ActivityType} for the activity.
     * @param isInOverviewModeSupplier Supplies whether the app is in overview mode.
     * @param appMenuDelegate The app menu delegate.
     * @param statusBarColorProvider Provides the status bar color.
     * @param ephemeralTabCoordinatorSupplier Supplies the {@link EphemeralTabCoordinator}.
     * @param intentRequestTracker Tracks intent requests.
     * @param insetObserver The {@link InsetObserver}.
     * @param backButtonShouldCloseTabFn Function which supplies whether or not the back button
     *     should close the tab.
     * @param sendToBackground Callback exiting the app and closing the tab.
     * @param initializeUiWithIncognitoColors Whether to initialize the UI with incognito colors.
     * @param backPressManager The {@link BackPressManager} handling back press.
     * @param savedInstanceState The saved bundle for the last recorded state.
     * @param persistentState The persistent bundle for the last recorded state.
     * @param multiInstanceManager Manages multi-instance mode.
     * @param overviewColorSupplier Notifies when the overview color changes.
     * @param manualFillingComponentSupplier Supplies the {@link ManualFillingComponent} for
     *     interacting with non-popup filling UI.
     * @param edgeToEdgeManager Manages core edge-to-edge state and logic.
     * @param bookmarkManagerOpenerSupplier Supplies {@link BookmarkManagerOpener}.
     * @param xrSpaceModeObservableSupplier Supplies current XR space mode status. True for XR full
     *     space mode, false otherwise.
     */
    public TabbedRootUiCoordinator(
            AppCompatActivity activity,
            @Nullable Callback<Boolean> onOmniboxFocusChangedListener,
            MonotonicObservableSupplier<ShareDelegate> shareDelegateSupplier,
            ActivityTabProvider tabProvider,
            MonotonicObservableSupplier<Profile> profileSupplier,
            NullableObservableSupplier<BookmarkModel> bookmarkModelSupplier,
            MonotonicObservableSupplier<TabBookmarker> tabBookmarkerSupplier,
            MonotonicObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            OneshotSupplier<TabSwitcher> tabSwitcherSupplier,
            OneshotSupplier<TabSwitcher> incognitoTabSwitcherSupplier,
            OneshotSupplier<HubManager> hubManagerSupplier,
            OneshotSupplier<ToolbarIntentMetadata> intentMetadataOneshotSupplier,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderOneshotSupplier,
            BrowserControlsManager browserControlsManager,
            ActivityWindowAndroid windowAndroid,
            ActivityResultTracker activityResultTracker,
            OneshotSupplier<ChromeAndroidTask> chromeAndroidTaskSupplier,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            MonotonicObservableSupplier<LayoutManagerImpl> layoutManagerSupplier,
            MenuOrKeyboardActionController menuOrKeyboardActionController,
            Supplier<Integer> activityThemeColorSupplier,
            MonotonicObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            AppMenuBlocker appMenuBlocker,
            BooleanSupplier supportsAppMenuSupplier,
            MonotonicObservableSupplier<TabCreatorManager> tabCreatorManagerSupplier,
            FullscreenManager fullscreenManager,
            MonotonicObservableSupplier<CompositorViewHolder> compositorViewHolderSupplier,
            Supplier<TabContentManager> tabContentManagerSupplier,
            MonotonicObservableSupplier<SnackbarManager> snackbarManagerSupplier,
            SettableMonotonicObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier,
            TopInsetProvider topInsetProvider,
            OneshotSupplierImpl<SystemBarColorHelper> systemBarColorHelperSupplier,
            @ActivityType int activityType,
            Supplier<Boolean> isInOverviewModeSupplier,
            AppMenuDelegate appMenuDelegate,
            StatusBarColorProvider statusBarColorProvider,
            SettableMonotonicObservableSupplier<EphemeralTabCoordinator>
                    ephemeralTabCoordinatorSupplier,
            IntentRequestTracker intentRequestTracker,
            InsetObserver insetObserver,
            Function<Tab, Boolean> backButtonShouldCloseTabFn,
            Callback<@Nullable Tab> sendToBackground,
            boolean initializeUiWithIncognitoColors,
            BackPressManager backPressManager,
            @Nullable Bundle savedInstanceState,
            @Nullable PersistableBundle persistentState,
            @Nullable MultiInstanceManager multiInstanceManager,
            NonNullObservableSupplier<Integer> overviewColorSupplier,
            MonotonicObservableSupplier<ManualFillingComponent> manualFillingComponentSupplier,
            EdgeToEdgeManager edgeToEdgeManager,
            MonotonicObservableSupplier<BookmarkManagerOpener> bookmarkManagerOpenerSupplier,
            NonNullObservableSupplier<Boolean> xrSpaceModeObservableSupplier,
            OneshotSupplier<ChromeInactivityTracker> inactivityTrackerSupplier,
            @Nullable BottomBarHostManager bottomBarHostManager) {
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
                browserControlsManager,
                windowAndroid,
                activityResultTracker,
                chromeAndroidTaskSupplier,
                activityLifecycleDispatcher,
                layoutManagerSupplier,
                menuOrKeyboardActionController,
                activityThemeColorSupplier,
                modalDialogManagerSupplier.asNonNull(),
                appMenuBlocker,
                supportsAppMenuSupplier,
                tabCreatorManagerSupplier,
                fullscreenManager,
                compositorViewHolderSupplier,
                tabContentManagerSupplier,
                snackbarManagerSupplier,
                edgeToEdgeSupplier,
                topInsetProvider,
                activityType,
                isInOverviewModeSupplier,
                appMenuDelegate,
                statusBarColorProvider,
                intentRequestTracker,
                ephemeralTabCoordinatorSupplier,
                initializeUiWithIncognitoColors,
                backPressManager,
                savedInstanceState,
                persistentState,
                overviewColorSupplier,
                edgeToEdgeManager,
                xrSpaceModeObservableSupplier,
                initAppHeaderCoordinator(
                        activity,
                        savedInstanceState,
                        persistentState,
                        edgeToEdgeManager.getEdgeToEdgeStateProvider(),
                        browserControlsManager,
                        insetObserver,
                        activityLifecycleDispatcher,
                        multiInstanceManager),
                bottomBarHostManager);

        if (BottomBarConfigUtils.isBottomBarEnabled(activity)) {
            mActionRegistry = new ActionRegistry();
            ActionUtils.registerBottomBarActions(mActionRegistry);
        }

        mInsetObserver = insetObserver;
        mBackButtonShouldCloseTabFn = backButtonShouldCloseTabFn;
        mSendToBackground = sendToBackground;

        mStatusBarColorController.maybeInitializeForCustomizedNtp(
                mActivity,
                NtpCustomizationUtils.supportsEnableEdgeToEdgeOnTop(
                        windowAndroid,
                        DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity)));
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
                            assert mHistoryNavigationCoordinator != null;
                            mHistoryNavigationCoordinator.reset();
                            setActivityTitle(/* tab= */ null, /* isTabSwitcher= */ true);
                        }
                    }
                };
        mMultiInstanceManager = multiInstanceManager;
        mHubManagerSupplier = hubManagerSupplier;
        mStatusBarColorController.setAllowToolbarColorOnTablets(true);
        mSystemBarColorHelperSupplier = systemBarColorHelperSupplier;
        mManualFillingComponentSupplier = manualFillingComponentSupplier;
        mInactivityTrackerSupplier = inactivityTrackerSupplier;

        DataSharingTabGroupsDelegate dataSharingTabGroupsDelegate =
                createDataSharingTabGroupsDelegate();

        Callback<Callback<Boolean>> startAccountRefreshCallback =
                (Callback<Boolean> successCallback) -> {
                    assert getDataSharingTabManager() != null;
                    var profile = mProfileSupplier.get();
                    assert profile != null;
                    IdentityManager identityManager =
                            IdentityServicesProvider.get().getIdentityManager(profile);
                    assumeNonNull(identityManager);
                    @Nullable AccountInfo primaryAccountInfo =
                            identityManager.getPrimaryAccountInfo();
                    assert primaryAccountInfo != null;
                    AccountManagerFacadeProvider.getInstance()
                            .updateCredentials(
                                    primaryAccountInfo.getId(), mActivity, successCallback);
                };

        CollaborationControllerDelegateFactory collaborationControllerDelegateFactory =
                (flowType, switchToTabSwitcherCallback) -> {
                    assert getDataSharingTabManager() != null;
                    return new CollaborationControllerDelegateImpl(
                            mActivity,
                            flowType,
                            getDataSharingTabManager(),
                            SigninAndHistorySyncActivityLauncherImpl.get(),
                            getLoadingFullscreenCoordinator(),
                            switchToTabSwitcherCallback,
                            startAccountRefreshCallback);
                };

        mDataSharingTabManager =
                new DataSharingTabManager(
                        mTabModelSelectorSupplier,
                        dataSharingTabGroupsDelegate,
                        getBottomSheetControllerSupplier(),
                        mShareDelegateSupplier,
                        mWindowAndroid,
                        mActivity.getResources(),
                        mTabGroupUiActionHandlerSupplier,
                        collaborationControllerDelegateFactory);

        mBookmarkManagerOpenerSupplier = bookmarkManagerOpenerSupplier;

        mKeyboardFocusRowManager =
                new KeyboardFocusRowManager(
                        () -> mBookmarkBarCoordinator, // Gets current mBookmarkBarCoordinator
                        compositorViewHolderSupplier,
                        modalDialogManagerSupplier,
                        () -> mSidePanelContainerCoordinator,
                        mSideUiStateProviderSupplier,
                        () -> assumeNonNull(mLayoutManager).getStripLayoutHelperManager(),
                        mTabObscuringHandlerSupplier.get(),
                        () -> mToolbarManager // Gets current value of mToolbarManager
                        );

        mInactivityObserver =
                new InactivityObserver() {
                    @Override
                    public void onForegrounded(long timeSinceLastBackgroundedMs) {
                        // Reset the evaluation flag when the app is foregrounded, indicating a
                        // new foreground session has started and promos should be re-evaluated.
                        mPromosEvaluatedForCurrentForeground = false;
                        maybeShowPromosOnForeground();
                    }
                };

        mInactivityTrackerSupplier.onAvailable(
                (inactivityTracker) -> {
                    inactivityTracker.addObserver(mInactivityObserver);
                });

        maybeShowPromosOnForeground();

        mCrossDeviceSettingImporter =
                DeviceInfo.isDesktop()
                        ? null
                        : new CrossDeviceSettingImporter(
                                activityLifecycleDispatcher,
                                mActivityTabProvider.asObservable(),
                                mActivity,
                                modalDialogManagerSupplier,
                                snackbarManagerSupplier);

        new OneShotCallback<>(mProfileSupplier, this::waitForTrackerInit);

        try {
            PackageManager packageManager = mActivity.getPackageManager();
            ApplicationInfo applicationInfo =
                    packageManager.getApplicationInfo(mActivity.getPackageName(), 0);
            mApplicationLabel = packageManager.getApplicationLabel(applicationInfo);
        } catch (PackageManager.NameNotFoundException e) {
            Log.w(TAG, "Error getting application info", e);
            mApplicationLabel = mActivity.getResources().getString(R.string.app_name);
        }
    }

    @Override
    @SuppressWarnings("NullAway")
    public void onDestroy() {
        if (mOpenInAppEntryPoint != null) {
            mOpenInAppEntryPoint.destroy();
            mOpenInAppEntryPoint = null;
        }

        if (mOmniboxChipManager != null) {
            mOmniboxChipManager = null;
        }

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

        if (mRootUiTabObserver != null) mRootUiTabObserver.destroy();

        if (mPwaBottomSheetController != null) {
            PwaBottomSheetControllerFactory.detach(mPwaBottomSheetController);
        }

        if (mHistoryNavigationCoordinator != null) {
            TouchEventObserver obs = mHistoryNavigationCoordinator.getTouchEventObserver();
            var compositorViewHolder = mCompositorViewHolderSupplier.get();
            if (compositorViewHolder != null && obs != null) {
                compositorViewHolder.removeTouchEventObserver(obs);
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

        if (mPdfPageIphController != null) {
            mPdfPageIphController.destroy();
            mPdfPageIphController = null;
        }

        if (mCoordinator != null && mDragDropTouchObserver != null) {
            ((CoordinatorLayoutForPointer) mCoordinator)
                    .removeTouchEventObserver(mDragDropTouchObserver);
            mDragDropTouchObserver = null;
        }

        mDataSharingTabManager.destroy();

        if (mInstantMessageDelegateImpl != null) {
            mInstantMessageDelegateImpl.detachWindow(mWindowAndroid);
        }

        if (mBookmarkBarVisibilityProvider != null) {
            if (mBookmarkBarCoordinator != null) {
                mBookmarkBarVisibilityProvider.removeObserver(mBookmarkBarCoordinator);
            }
            mBookmarkBarVisibilityProvider.removeObserver(mBookmarkBarVisibilityObserver);
            mBookmarkBarVisibilityProvider.destroy();
            mBookmarkBarVisibilityProvider = null;
        }

        if (mBookmarkBarCoordinator != null) {
            mBookmarkBarCoordinator.destroy();
            mBookmarkBarCoordinator = null;
            mBookmarkOpener = null;
            if (mToolbarManager != null) {
                mToolbarManager.setBookmarkBarHeightSupplier(null);
            }
        }

        if (mLoadingFullscreenCoordinator != null) {
            mLoadingFullscreenCoordinator.destroy();
            mLoadingFullscreenCoordinator = null;
        }

        if (mAdvancedProtectionCoordinator != null) {
            mAdvancedProtectionCoordinator.destroy();
            mAdvancedProtectionCoordinator = null;
        }

        if (mPrivacySandbox3pcdRollbackMessageController != null) {
            mPrivacySandbox3pcdRollbackMessageController.destroy();
            mPrivacySandbox3pcdRollbackMessageController = null;
        }

        if (mTipsOptInCoordinator != null) {
            mTipsOptInCoordinator.destroy();
        }
        if (mGlicPromoCoordinator != null) {
            mGlicPromoCoordinator.destroy();
        }

        if (mInactivityTrackerSupplier.get() != null) {
            mInactivityTrackerSupplier.get().removeObserver(mInactivityObserver);
        }

        if (mNtpSyncedThemeManager != null) {
            mNtpSyncedThemeManager.destroy();
        }

        if (mTabBottomSheetReadAloudControllerCallback != null) {
            mReadAloudControllerSupplier.removeObserver(mTabBottomSheetReadAloudControllerCallback);
            mTabBottomSheetReadAloudControllerCallback = null;
        }

        if (mTabBottomSheetManager != null) {
            mTabBottomSheetManager.destroy();
            mTabBottomSheetManager = null;
        }

        if (mContextualTasksFuseboxManager != null) {
            mContextualTasksFuseboxManager.destroy();
            mContextualTasksFuseboxManager = null;
        }

        if (mContextualTasksBridge != null) {
            mContextualTasksBridge = null;
        }

        if (mCoBrowseViewFactory != null) {
            mCoBrowseViewFactory.destroy();
            mCoBrowseViewFactory = null;
        }

        if (mCrossDeviceSettingImporter != null) {
            mCrossDeviceSettingImporter.destroy();
        }

        if (mGlicUiCoordinator != null) {
            mGlicUiCoordinator.destroy();
            mGlicUiCoordinator = null;
        }

        if (mGestureUserEducationIphController != null) {
            mGestureUserEducationIphController.destroy();
            mGestureUserEducationIphController = null;
        }

        if (mForcedSigninController != null) {
            mForcedSigninController.destroy();
            mForcedSigninController = null;
        }

        destroySideUi();

        super.onDestroy();
    }

    @Override
    public void onPostInflationStartup() {
        super.onPostInflationStartup();

        ToolbarManager toolbarManager = mToolbarManager;
        assert toolbarManager != null;

        var bottomSheetController = getBottomSheetController();
        assert bottomSheetController != null;
        mSystemUiCoordinator =
                new TabbedSystemUiCoordinator(
                        mActivity.getWindow(),
                        mTabModelSelectorSupplier.asNonNull().get(),
                        mLayoutManagerSupplier,
                        mFullscreenManager,
                        mEdgeToEdgeControllerSupplier,
                        mBottomControlsStacker,
                        mBrowserControlsManager,
                        mContextualSearchManagerSupplier,
                        bottomSheetController,
                        toolbarManager.getLocationBar().getOmniboxSuggestionsVisualState(),
                        mManualFillingComponentSupplier.get(),
                        mOverviewColorSupplier,
                        mInsetObserver,
                        mEdgeToEdgeManager.getEdgeToEdgeSystemBarColorHelper());
    }

    @Override
    @EnsuresNonNull("mToolbarManager")
    protected void initializeToolbar() {
        if (OpenInAppUtils.isOpenInAppAvailable()) {
            View controlContainer = mActivity.findViewById(R.id.control_container);
            assert controlContainer != null;

            ViewGroup omniboxChipContainer =
                    controlContainer.findViewById(R.id.omnibox_chip_container);
            LocationBarEmbedder locationBarEmbedder = mActivity.findViewById(R.id.toolbar);
            mOmniboxChipManager = new OmniboxChipManager(omniboxChipContainer, locationBarEmbedder);
        }

        assert mFindToolbarManager != null;
        super.initializeToolbar();

        if (AndroidSidePanelEnabledFn.isEnabled()) {
            mToolbarManager.setSideUiStateProviderSupplier(mSideUiStateProviderSupplier);
        }
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

    /** Returns the {@link LoadingFullscreenCoordinator} to control loading over the activity. */
    public @Nullable LoadingFullscreenCoordinator getLoadingFullscreenCoordinator() {
        return mLoadingFullscreenCoordinator;
    }

    /** Show navigation history sheet. */
    public void showFullHistorySheet() {
        if (mActivity == null) return;
        Tab tab = mActivityTabProvider.get();
        if (tab == null
                || tab.getWebContents() == null
                || !tab.isUserInteractable()
                || tab.getContentView() == null) return;
        Profile profile = tab.getProfile();
        mNavigationSheet =
                NavigationSheet.create(
                        tab.getContentView(),
                        mActivity,
                        getBottomSheetControllerSupplier(),
                        profile);
        mNavigationSheet.setDelegate(
                new TabbedSheetDelegate(
                        tab,
                        aTab -> {
                            HistoryManagerUtils.showHistoryManager(
                                    mActivity, aTab, aTab.getProfile());
                        },
                        mActivity.getResources().getString(R.string.show_full_history)));
        if (!mNavigationSheet.startAndExpand(/* forward= */ false, /* animate= */ true)) {
            mNavigationSheet = null;
        } else {
            var controller = getBottomSheetController();
            assert controller != null;
            controller.addObserver(
                    new EmptyBottomSheetObserver() {
                        @Override
                        public void onSheetClosed(int reason) {
                            var bottomSheetController = getBottomSheetController();
                            assumeNonNull(bottomSheetController);
                            bottomSheetController.removeObserver(this);
                            mNavigationSheet = null;
                        }
                    });
        }
    }

    @Override
    public void onInflationComplete() {
        mCoordinator = mActivity.findViewById(R.id.coordinator);

        super.onInflationComplete();

        ViewStub loadingStub = mActivity.findViewById(R.id.loading_stub);
        assert loadingStub != null;

        loadingStub.setLayoutResource(R.layout.loading_fullscreen);
        loadingStub.inflate();

        mLoadingFullscreenCoordinator =
                new LoadingFullscreenCoordinator(
                        mActivity,
                        getScrimManager(),
                        mActivity.findViewById(R.id.loading_fullscreen_container));

        if (OpenInAppUtils.isOpenInAppAvailable()) {
            var omniboxChipManager = mOmniboxChipManager;
            assert omniboxChipManager != null;
            mOpenInAppEntryPoint =
                    new TabbedOpenInAppEntryPoint(
                            mActivityTabProvider.asObservable(), omniboxChipManager, mActivity);
        }

        if (AndroidSidePanelEnabledFn.isEnabled()) {
            mCompositorViewHolderSupplier
                    .asNonNull()
                    .get()
                    .setSideUiStateProviderSupplier(mSideUiStateProviderSupplier);
        }
    }

    @Override
    public void onFinishNativeInitialization() {
        super.onFinishNativeInitialization();
        assert mLayoutManager != null;

        mAdvancedProtectionCoordinator =
                new AdvancedProtectionCoordinator(mWindowAndroid, PrivacySettings.class);

        UmaSessionStats.registerSyntheticFieldTrial(
                "AndroidNavigationMode",
                UiUtils.isGestureNavigationMode(mActivity.getWindow())
                        ? "GestureNav"
                        : "ThreeButton");

        CompositorViewHolder compositorViewHolder = mCompositorViewHolderSupplier.asNonNull().get();
        mHistoryNavigationCoordinator =
                HistoryNavigationCoordinator.create(
                        mWindowAndroid,
                        mActivityLifecycleDispatcher,
                        compositorViewHolder,
                        mCallbackController.makeCancelable(
                                () -> {
                                    assumeNonNull(mLayoutManager);
                                    var activeLayout = mLayoutManager.getActiveLayout();
                                    if (activeLayout != null) activeLayout.requestUpdate();
                                }),
                        mActivityTabProvider.asObservable(),
                        mInsetObserver,
                        new BackActionDelegate() {
                            @Override
                            public @ActionType int getBackActionType(Tab tab) {
                                if (tab.canGoBack()) {
                                    return ActionType.NAVIGATE_BACK;
                                }
                                if (TabAssociatedApp.isOpenedFromExternalApp(tab)) {
                                    return ActionType.EXIT_APP_AND_CLOSE_TAB;
                                }
                                return mBackButtonShouldCloseTabFn.apply(tab)
                                        ? ActionType.CLOSE_TAB
                                        : ActionType.EXIT_APP_ONLY;
                            }

                            @Override
                            public void onBackGesture(Tab tab) {
                                // Back navigation gesture performs only history navigation or
                                // closing the current tab/chrome. Other actions back button can do
                                // is not considered.
                                switch (getBackActionType(tab)) {
                                    case ActionType.NAVIGATE_BACK:
                                        tab.goBack();
                                        break;
                                    case ActionType.CLOSE_TAB:
                                        mTabModelSelectorSupplier
                                                .asNonNull()
                                                .get()
                                                .getCurrentModel()
                                                .getTabRemover()
                                                .closeTabs(
                                                        TabClosureParams.closeTab(tab).build(),
                                                        /* allowDialog= */ false);
                                        break;
                                    case ActionType.EXIT_APP_AND_CLOSE_TAB:
                                        mSendToBackground.onResult(tab);
                                        break;
                                    case ActionType.EXIT_APP_ONLY:
                                        mSendToBackground.onResult(null);
                                        break;
                                }
                            }
                        },
                        compositorViewHolder,
                        mFullscreenManager);
        mRootUiTabObserver.swapToTab(mActivityTabProvider.get());

        // TODO(crbug.com/40946488): Consider register this drag listener to other views besides
        // CVH.
        // Instantiating ChromeTabbedOnDragListener on tablets since tab drags is enabled only via
        // tablet tab strip.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            assert mMultiInstanceManager != null;
            ChromeTabbedOnDragListener chromeTabbedOnDragListener =
                    new ChromeTabbedOnDragListener(
                            mMultiInstanceManager,
                            mTabModelSelectorSupplier.asNonNull().get(),
                            mActivity,
                            mLayoutStateProviderOneShotSupplier);

            compositorViewHolder.setOnDragListener(chromeTabbedOnDragListener);

            // Disable touch event while drag is in progress.
            mDragDropTouchObserver = e -> DragDropGlobalState.hasValue();
            assumeNonNull(mCoordinator);
            ((CoordinatorLayoutForPointer) mCoordinator)
                    .addTouchEventObserver(mDragDropTouchObserver);
        }

        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            // mToolbarManager is initialized in initializeToolbar() which happens during
            // inflation.
            assumeNonNull(mToolbarManager).enableBottomControls();
        }

        SupplierUtils.waitForAll(
                mCallbackController.makeCancelable(
                        () -> {
                            var profile = mProfileSupplier.asNonNull().get();
                            var intentMetadata = mIntentMetadataOneshotSupplier.get();
                            assert intentMetadata != null;
                            initializeIph(
                                    profile.getOriginalProfile(),
                                    intentMetadata.getIsIntentWithEffect());
                        }),
                mIntentMetadataOneshotSupplier,
                mProfileSupplier,
                mTrackerInitializedOneshotSupplier);

        // TODO(crbug.com/40736706): Investigate switching to per-Activity coordinator that
        // uses signals from the current Tab to decide when to show the PWA install bottom sheet
        // rather than relying on unowned user data.
        mPwaBottomSheetController =
                PwaBottomSheetControllerFactory.createPwaBottomSheetController(mActivity);
        PwaBottomSheetControllerFactory.attach(mWindowAndroid, mPwaBottomSheetController);
        initCommerceSubscriptionsService();
        initUndoGroupSnackbarController();

        new OneShotCallback<>(mProfileSupplier, this::initCollaborationDelegatesOnProfile);

        if (BookmarkBarUtils.isDeviceBookmarkBarCompatible(mActivity)) {
            BookmarkBarUtils.recordStartUpMetrics(
                    mActivity, mProfileSupplier.get(), mXrSpaceModeObservableSupplier.get());
            mBookmarkBarVisibilityProvider =
                    new BookmarkBarVisibilityProvider(
                            mActivity,
                            mActivityLifecycleDispatcher,
                            mProfileSupplier,
                            mXrSpaceModeObservableSupplier);
            mBookmarkBarVisibilityObserver =
                    new BookmarkBarVisibilityObserver() {
                        @Override
                        public void onVisibilityChanged(boolean visibility) {
                            updateBookmarkBarIfNecessary(visibility);
                        }
                    };
            mBookmarkBarVisibilityProvider.addObserver(mBookmarkBarVisibilityObserver);
        }

        initiateTabBottomSheetManagers();

        if (GlicEnabling.isEnabledByFlags() && mTabBottomSheetManager != null) {
            GlicNavigationUtils.setLauncher(SigninAndHistorySyncActivityLauncherImpl::get);
            ViewStub actorOverlayStub = mActivity.findViewById(R.id.actor_overlay_stub);
            mGlicUiCoordinator =
                    new GlicUiCoordinator(
                            mActivity,
                            mTabBottomSheetManager,
                            mProfileSupplier,
                            mActivityTabProvider.asObservable(),
                            mTabModelSelectorSupplier,
                            mBrowserControlsManager,
                            mTabObscuringHandlerSupplier.get(),
                            assumeNonNull(mSnackbarManagerSupplier.get()),
                            mBackPressManager,
                            mLayoutManagerSupplier,
                            actorOverlayStub,
                            assertNonNull(getBottomSheetController()),
                            mActivityLifecycleDispatcher,
                            mSideUiStateProviderSupplier.get());
        }

        mForcedSigninController =
                new ForcedSigninController(
                        mActivity,
                        mProfileSupplier.asNonNull().get().getOriginalProfile(),
                        SigninAndHistorySyncActivityLauncherImpl.get());
    }

    @Override
    protected AdaptiveToolbarBehavior createAdaptiveToolbarBehavior(
            Supplier<@Nullable Tracker> trackerSupplier) {

        Supplier<GroupSuggestionsButtonController> groupSuggestionsButtonControllerSupplier =
                () -> {
                    Profile profile = mProfileSupplier.asNonNull().get();
                    var controller = GroupSuggestionsButtonControllerFactory.getForProfile(profile);
                    assert controller != null;
                    return controller;
                };

        return new TabbedAdaptiveToolbarBehavior(
                mActivity,
                mActivityLifecycleDispatcher,
                mTabCreatorManagerSupplier,
                mTabBookmarkerSupplier,
                mBookmarkModelSupplier,
                mActivityTabProvider,
                () -> addVoiceSearchAdaptiveButton(trackerSupplier),
                groupSuggestionsButtonControllerSupplier,
                mTabModelSelectorSupplier,
                // TODO(agrieve): See if this can be changed to a NonNullObservableSupplier.
                (MonotonicObservableSupplier<Integer>) mTabStripVisibilitySupplier,
                (preventClose, invocationSource) -> toggleGlic(preventClose, invocationSource),
                mChromeAndroidTaskSupplier,
                mBrowserControlsManager);
    }

    @Override
    protected void initProfileDependentFeatures(Profile currentlySelectedProfile) {
        super.initProfileDependentFeatures(currentlySelectedProfile);
        Profile originalProfile = currentlySelectedProfile.getOriginalProfile();

        if (ChromeFeatureList.sChromeNativeUrlOverriding.isEnabled()) {
            ExtensionsUrlOverrideRegistryManagerFactory.getForProfile(originalProfile);
        }

        if (TabGroupSyncFeatures.isTabGroupSyncEnabled(originalProfile)) {
            var tabGroupSyncService = TabGroupSyncServiceFactory.getForProfile(originalProfile);
            assert tabGroupSyncService != null;
            mTabGroupSyncController =
                    new TabGroupSyncControllerImpl(
                            mTabModelSelectorSupplier.asNonNull().get(),
                            tabGroupSyncService,
                            UserPrefs.get(originalProfile),
                            () -> {
                                return MultiWindowUtils.getInstanceCount(PersistedInstanceType.ANY)
                                                <= 1
                                        || ApplicationStatus.getLastTrackedFocusedActivity()
                                                == mActivity;
                            });
            mTabGroupUiActionHandlerSupplier.set(mTabGroupSyncController);
        }

        boolean isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity);
        boolean isNewTabPageCustomizationV2Enabled =
                isTablet
                        || NtpCustomizationUtils.supportsEnableEdgeToEdgeOnTop(
                                mWindowAndroid, isTablet);
        if (isNewTabPageCustomizationV2Enabled) {
            mNtpSyncedThemeManager = new NtpSyncedThemeManager(mActivity, originalProfile);
            NtpCustomizationPolicyManager.getInstance()
                    .onFinishNativeInitialization(currentlySelectedProfile);
        }
        NewTabPageLocationPolicyManager.getInstance().onFinishNativeInitialization(originalProfile);

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_TASKS)) {
            if (mChromeAndroidTaskSupplier.get() != null) {
                mContextualTasksBridge = new ContextualTasksBridge(originalProfile, mWindowAndroid);
                mChromeAndroidTaskSupplier
                        .get()
                        .addFeature(
                                new ChromeAndroidTaskFeatureKey(
                                        ContextualTasksBridge.class,
                                        originalProfile,
                                        mWindowAndroid),
                                () -> mContextualTasksBridge);
            }

            if (ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_TASKS_JAVA_FUSEBOX)) {
                mContextualTasksFuseboxManager =
                        new ContextualTasksFuseboxManagerImpl(
                                mActivity,
                                () -> {
                                    ViewStub stub =
                                            mActivity.findViewById(
                                                    R.id.contextual_tasks_fusebox_stub);
                                    return createContextualTasksFuseboxConfig(stub.inflate());
                                },
                                mActivityTabProvider.asObservable(),
                                mWindowAndroid,
                                mActivityLifecycleDispatcher,
                                mProfileSupplier,
                                mSnackbarManagerSupplier);
            }
        }
        initializeSideUi(currentlySelectedProfile);
    }

    /** Creates an instance of {@link IncognitoReauthCoordinatorFactory} for tabbed activity. */
    @Override
    protected IncognitoReauthCoordinatorFactory getIncognitoReauthCoordinatorFactory(
            Profile profile) {
        IncognitoReauthCoordinatorFactory incognitoReauthCoordinatorFactory =
                new IncognitoReauthCoordinatorFactory(
                        mActivity,
                        mTabModelSelectorSupplier.asNonNull().get(),
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
        layoutStateProvider.addObserver(mGestureNavLayoutObserver);
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
    protected ScrimManager buildScrimWidget() {
        assert mCoordinator != null;
        ScrimManager scrimManager =
                new ScrimManager(mActivity, mCoordinator, ScrimClient.TABBED_ROOT_UI_COORDINATOR);
        scrimManager
                .getStatusBarColorSupplier()
                .addSyncObserverAndPostIfNonNull(mStatusBarColorController::onScrimColorChanged);
        scrimManager
                .getNavigationBarColorSupplier()
                .addSyncObserverAndPostIfNonNull(this::onNavBarScrimColorChanged);
        return scrimManager;
    }

    @SuppressLint("NewApi")
    private void onNavBarScrimColorChanged(@ColorInt int color) {
        // When drawing edge to edge, scrim already draws over the nav bar region.
        // No need to change the nav bar color.
        final var edgeToEdgeController = mEdgeToEdgeControllerSupplier.get();
        if (edgeToEdgeController != null && edgeToEdgeController.isDrawingToEdge()) {
            return;
        }

        assumeNonNull(mSystemUiCoordinator);
        TabbedNavigationBarColorController controller =
                mSystemUiCoordinator.getNavigationBarColorController();
        if (controller == null) {
            return;
        }
        controller.setNavigationBarScrimColor(color);
    }

    // Private class methods
    private ContextualTasksFuseboxConfig createContextualTasksFuseboxConfig(View contentView) {
        var omniboxActionDelegate =
                new OmniboxActionDelegateImpl(
                        mActivity,
                        /* tabSupplier= */ () -> null,
                        /* openUrlInExistingTabElseNewTabCb= */ (url) -> {},
                        /* openIncognitoTabCb= */ CallbackUtils.emptyRunnable(),
                        /* openPasswordSettingsCb= */ CallbackUtils.emptyRunnable(),
                        /* openQuickDeleteCb= */ CallbackUtils.emptyRunnable(),
                        /* tabWindowManagerSupplier= */ TabWindowManagerSingleton::getInstance,
                        /* bringTabToFrontCallback= */ (tabInfo, url) -> {});
        return new ContextualTasksFuseboxConfig(
                contentView,
                contentView.findViewById(R.id.search_location_bar),
                contentView.findViewById(R.id.toolbar),
                contentView.findViewById(R.id.control_container),
                contentView.findViewById(R.id.bottom_container),
                omniboxActionDelegate);
    }

    private void initializeIph(Profile profile, boolean intentWithEffect) {
        if (mActivity == null) return;
        ToolbarManager toolbarManager = mToolbarManager;
        if (toolbarManager == null) return;

        var menuButtonView = toolbarManager.getMenuButtonView();
        assert menuButtonView != null;
        assert mAppMenuCoordinator != null;
        assert mMessageDispatcher != null;

        mToolbarButtonInProductHelpController =
                new ToolbarButtonInProductHelpController(
                        mActivity,
                        mWindowAndroid,
                        mAppMenuCoordinator,
                        profile,
                        mActivityTabProvider.asObservable(),
                        mIsInOverviewModeSupplier,
                        menuButtonView);
        mReadAloudIphController =
                new ReadAloudIphController(
                        mActivity,
                        profile,
                        menuButtonView,
                        mAppMenuCoordinator.getAppMenuHandler(),
                        mActivityTabProvider.asObservable(),
                        mReadAloudControllerSupplier,
                        /* showAppMenuTextBubble= */ true);
        mReadLaterIphController =
                new ReadLaterIphController(
                        mActivity,
                        profile,
                        menuButtonView,
                        mAppMenuCoordinator.getAppMenuHandler());
        mReaderModeIphControllerSupplier.set(
                new ReaderModeIphController(
                        mActivity,
                        profile,
                        menuButtonView,
                        mAppMenuCoordinator.getAppMenuHandler()));

        boolean didTriggerPromo =
                maybeShowRequiredPromptsAndPromos(profile, intentWithEffect)
                        || RequestDesktopUtils.maybeShowDefaultEnableGlobalSettingMessage(
                                profile, mMessageDispatcher, mActivity);

        if (didTriggerPromo) {
            TrackerFactory.getTrackerForProfile(profile)
                    .notifyEvent(EventConstants.ANDROID_STARTUP_PROMO_SHOWN);
        } else {
            mToolbarButtonInProductHelpController.showColdStartIph();
            mReadLaterIphController.showColdStartIph();
            if (MultiWindowUtils.shouldShowInstanceSwitcherIph()) {
                MultiInstanceIphController.maybeShowInProductHelp(
                        mActivity,
                        profile,
                        menuButtonView,
                        mAppMenuCoordinator.getAppMenuHandler(),
                        R.id.manage_all_windows_menu_id);
            }
            mDesktopSiteSettingsIphController =
                    DesktopSiteSettingsIphController.create(
                            mActivity,
                            mWindowAndroid,
                            mActivityTabProvider,
                            profile,
                            menuButtonView,
                            mAppMenuCoordinator.getAppMenuHandler());
            mPdfPageIphController =
                    PdfPageIphController.create(
                            mActivity,
                            mWindowAndroid,
                            mActivityTabProvider,
                            profile,
                            menuButtonView,
                            mAppMenuCoordinator.getAppMenuHandler(),
                            /* isBrowserApp= */ true);
            if (ChromeFeatureList.sGestureUserEducationBackSwipe.isEnabled()
                    && !DeviceInfo.isAutomotive()
                    && !DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)
                    && UiUtils.isGestureNavigationMode(mActivity.getWindow())
                    && TrackerFactory.getTrackerForProfile(profile)
                            .wouldTriggerHelpUi(FeatureConstants.GESTURE_USER_EDUCATION)) {
                mGestureUserEducationIphController =
                        new GestureUserEducationIphController(
                                mActivity.findViewById(R.id.compositor_view_holder),
                                mActivityTabProvider,
                                mBackPressManager,
                                getScrimManager());
            }
        }
        mPromoShownOneshotSupplier.set(didTriggerPromo);

        if (mOfflineIndicatorController != null) {
            // Initialize the OfflineIndicatorInProductHelpController if the
            // mOfflineIndicatorController is enabled and initialized. For example, it wouldn't be
            // initialized if the OfflineIndicatorV2 feature is disabled.
            assert mOfflineIndicatorInProductHelpController == null;
            assert mStatusIndicatorCoordinator != null;

            mOfflineIndicatorInProductHelpController =
                    new OfflineIndicatorInProductHelpController(
                            mActivity,
                            profile,
                            toolbarManager,
                            mAppMenuCoordinator.getAppMenuHandler(),
                            mStatusIndicatorCoordinator);
        }

        new LinkToTextIphController(
                mActivityTabProvider.asObservable(),
                mTabModelSelectorSupplier.asNonNull().get(),
                mProfileSupplier);

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

        if (!didTriggerPromo && PageZoomUtils.shouldShowZoomMenuItem()) {
            // Page Zoom IPH should only show if the menu item is visible, and not on NTP or CCT.
            if (!BuildConfig.IS_FOR_TEST
                    && tab != null
                    && tab.getWebContents() != null
                    && !tab.isNativePage()) {
                PageZoomIphController pageZoomIphController =
                        new PageZoomIphController(
                                mActivity,
                                profile,
                                mAppMenuCoordinator.getAppMenuHandler(),
                                menuButtonView);
                pageZoomIphController.showColdStartIph();
            }
        }
    }

    // TODO(crbug.com/515566274): Move foreground promos coordination logic to its own class to
    // facilitate testing and reduce TabbedRootUiCoordinator's complexity.
    private void maybeShowPromosOnForeground() {
        mTrackerInitializedOneshotSupplier.onAvailable(
                mCallbackController.makeCancelable(
                        (initialized) -> {
                            if (!initialized) {
                                return;
                            }
                            if (mInactivityTrackerSupplier.get() == null) {
                                return;
                            }
                            if (mPromosEvaluatedForCurrentForeground) {
                                return;
                            }
                            mPromosEvaluatedForCurrentForeground = true;

                            if (maybeShowGlicPromo()) {
                                return;
                            }

                            long timeSinceLastBackgroundedMs =
                                    mInactivityTrackerSupplier
                                            .get()
                                            .getTimeSinceLastBackgroundedMs();
                            maybeShowTipsOptInPromo(timeSinceLastBackgroundedMs);
                        }));
    }

    private void maybeShowTipsOptInPromo(long timeSinceLastBackgroundedMs) {
        var bottomSheetController = getBottomSheetController();
        assert bottomSheetController != null;
        TipsUtils.maybeShowTipsOptInPromo(
                mActivity,
                bottomSheetController,
                mSnackbarManagerSupplier.asNonNull().get(),
                ChromeSharedPreferences.getInstance(),
                timeSinceLastBackgroundedMs,
                (coordinator) -> mTipsOptInCoordinator = coordinator);
    }

    @VisibleForTesting
    boolean maybeShowGlicPromo() {
        Profile profile = mProfileSupplier.get();
        if (profile == null
                || mActivity == null
                || mActivity.isFinishing()
                || mActivity.isDestroyed()) {
            return false;
        }

        if (!ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.GLIC, "adaptive-toolbar-auto-pin", true)) {
            return false;
        }

        // When the Android Bottom Bar is enabled the promo is not required as the button is
        // available by default.
        boolean glicEnabled = GlicEnabling.isEnabledForProfile(profile);
        boolean bottomBarEnabled = BottomBarConfigUtils.isBottomBarEnabled(mActivity);
        if (!glicEnabled || bottomBarEnabled) {
            return false;
        }

        boolean hasEvaluatedGlicPromo =
                ChromeSharedPreferences.getInstance()
                        .contains(ChromePreferenceKeys.GLIC_PROMO_ACCEPTED);
        if (hasEvaluatedGlicPromo) {
            return false;
        }
        boolean isGlicPinned =
                AdaptiveToolbarPrefs.getCustomizationSetting() == AdaptiveToolbarButtonVariant.GLIC;
        boolean isToolbarPinned =
                AdaptiveToolbarPrefs.getCustomizationSetting() != AdaptiveToolbarButtonVariant.AUTO;
        // We use wouldTriggerHelpUi and notifyEvent manually instead of shouldTriggerHelpUi
        // to avoid locking the IPH session and blocking other IPHs from showing.
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        boolean shouldPinGlic =
                tracker.wouldTriggerHelpUi(
                        FeatureConstants.ADAPTIVE_BUTTON_PIN_GLIC_TOOLBAR_BUTTON_FEATURE);
        tracker.notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_GLIC_IPH_TRIGGER);

        // Auto-enable the Glic button and bypass the promo if:
        // 1. Glic is already pinned to the toolbar.
        // 2. The feature engagement tracker recommends pinning Glic AND the user has not
        //    manually customized the toolbar with a different button (to avoid overriding
        //    the user's explicit preference).
        if (isGlicPinned || (shouldPinGlic && !isToolbarPinned)) {
            enableGlicButton();
            return false;
        }

        showGlicPromo();
        return true;
    }

    private void showGlicPromo() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.GLIC_PROMO_ACCEPTED, false);

        Runnable onAccepted = this::enableGlicButton;
        Runnable onDismissed = () -> {};

        var bottomSheetController = getBottomSheetController();
        assert bottomSheetController != null;
        mGlicPromoCoordinator =
                new GlicPromoCoordinator(mActivity, bottomSheetController, onAccepted, onDismissed);
        mGlicPromoCoordinator.showBottomSheet();
    }

    private void enableGlicButton() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.GLIC_PROMO_ACCEPTED, true);
        AdaptiveToolbarPrefs.saveToolbarButtonManualOverride(AdaptiveToolbarButtonVariant.GLIC);
    }

    private void updateTopControlsHeight() {
        updateTopControlsHeight(/* allowAnimations= */ true);
    }

    private void updateTopControlsHeight(boolean allowAnimations) {
        if (mToolbarManager == null) return;

        // TODO(crbug/331844971): Do a smooth transition head into DW mode.
        final boolean animate =
                allowAnimations
                        && !sDisableTopControlsAnimationForTesting
                        && !AppHeaderUtils.isAppInDesktopWindow(getDesktopWindowStateManager());
        mTopControlsStacker.requestLayerUpdateSync(animate);
    }

    private void initCommerceSubscriptionsService() {
        SupplierUtils.waitForAll(
                mCallbackController.makeCancelable(
                        () -> {
                            mCommerceSubscriptionsService =
                                    CommerceSubscriptionsServiceFactory.getInstance()
                                            .getForProfile(mProfileSupplier.asNonNull().get());
                            mCommerceSubscriptionsService.initDeferredStartupForActivity(
                                    mTabModelSelectorSupplier.asNonNull().get(),
                                    mActivityLifecycleDispatcher);
                        }),
                mTabModelSelectorSupplier,
                mProfileSupplier);
    }

    private void initUndoGroupSnackbarController() {
        mUndoGroupSnackbarController =
                new UndoGroupSnackbarController(
                        mActivity,
                        mTabModelSelectorSupplier.asNonNull().get(),
                        mSnackbarManagerSupplier.asNonNull().get());
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
                        mCompositorViewHolderSupplier.asNonNull().get().getResourceManager(),
                        mBrowserControlsManager,
                        mTabObscuringHandlerSupplier.get(),
                        mStatusBarColorController::getStatusBarColorWithoutStatusIndicator,
                        mCanAnimateBrowserControls,
                        layoutManager::requestUpdate,
                        mTopControlsStacker);
        layoutManager.addSceneOverlay(mStatusIndicatorCoordinator.getSceneLayer());
        mStatusIndicatorObserver =
                new StatusIndicatorObserver() {
                    @Override
                    public void onStatusIndicatorHeightChanged(int indicatorHeight) {
                        mStatusIndicatorHeight = indicatorHeight;
                        updateTopControlsHeight();
                        HubManager hubManager = mHubManagerSupplier.get();
                        if (hubManager != null) {
                            hubManager.setStatusIndicatorHeight(indicatorHeight);
                        }
                        // Disable edge-to-edge on top when the status indicator is visible
                        // to avoid the indicator being obscured by the status bar in e2e
                        // mode.
                        if (mTopInsetCoordinator != null) {
                            mTopInsetCoordinator.setStatusIndicatorVisible(indicatorHeight > 0);
                        }
                    }
                };
        mStatusIndicatorCoordinator.addObserver(mStatusIndicatorObserver);
        mStatusIndicatorCoordinator.addObserver(mStatusBarColorController);
        mHubManagerSupplier.onAvailable(
                hubManager -> {
                    hubManager.setStatusIndicatorHeight(mStatusIndicatorHeight);
                });

        // mToolbarManager is initialized in initializeToolbar() which happens during
        // inflation.
        SettableNonNullObservableSupplier<Boolean> isUrlBarFocusedSupplier =
                ObservableSuppliers.createNonNull(assumeNonNull(mToolbarManager).isUrlBarFocused());
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

        if (!CommandLine.getInstance()
                .hasSwitch(ContentSwitches.FORCE_ONLINE_CONNECTION_STATE_FOR_INDICATOR)) {
            mOfflineIndicatorController =
                    new OfflineIndicatorControllerV2(
                            mActivity,
                            mStatusIndicatorCoordinator,
                            isUrlBarFocusedSupplier,
                            mCanAnimateBrowserControls);
        }

        if (mToolbarManager.getOmniboxStub() != null) {
            mToolbarManager.getOmniboxStub().addUrlFocusChangeListener(mUrlFocusChangeListener);
        }
    }

    @Override
    protected Destroyable createEdgeToEdgeBottomChin() {
        boolean defaultVisibility =
                !DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)
                        || EdgeToEdgeUtils.defaultVisibilityOfBottomChinOnTablet(mActivity);
        assert mWindowAndroid != null;
        assert mLayoutManager != null;
        SystemBarColorHelper bottomChinColorHelper =
                EdgeToEdgeControllerFactory.createBottomChin(
                        mActivity.findViewById(R.id.edge_to_edge_bottom_chin),
                        mWindowAndroid.getKeyboardDelegate(),
                        mInsetObserver,
                        mLayoutManager,
                        mLayoutManager::requestUpdate,
                        mEdgeToEdgeControllerSupplier.get(),
                        mBottomControlsStacker,
                        mFullscreenManager,
                        defaultVisibility);
        mSystemBarColorHelperSupplier.set(bottomChinColorHelper);
        return bottomChinColorHelper;
    }

    private void initiateTabBottomSheetManagers() {
        if (TabBottomSheetUtils.isTabBottomSheetEnabled()
                || AndroidSidePanelEnabledFn.isEnabled()) {
            View contentView =
                    mActivity.getLayoutInflater().inflate(R.layout.search_activity, null);
            ContextualTasksFuseboxConfig fuseboxConfig =
                    createContextualTasksFuseboxConfig(contentView);
            ContextMenuPopulatorFactory contextMenuPopulatorFactory =
                    new ChromeContextMenuPopulatorFactory(
                            /* itemDelegate= */ null,
                            mShareDelegateSupplier,
                            ChromeContextMenuPopulator.ContextMenuMode.THIN_WEB_VIEW,
                            /* customContentActions= */ Collections.emptyList());
            mCoBrowseViewFactory =
                    new CoBrowseViewFactory(
                            mActivity,
                            fuseboxConfig,
                            mProfileSupplier.asNonNull(),
                            mWindowAndroid,
                            mActivityLifecycleDispatcher,
                            mSnackbarManagerSupplier.asNonNull().get(),
                            contextMenuPopulatorFactory);
        }
        if (TabBottomSheetUtils.isTabBottomSheetEnabled()) {
            mTabBottomSheetManager =
                    new TabBottomSheetManagerImpl(
                            mActivity,
                            mWindowAndroid,
                            assertNonNull(getBottomSheetController()),
                            mLayoutStateProviderOneShotSupplier,
                            SupplierUtils.asNonNull(mCompositorViewHolderSupplier).get());
            Callback<ReadAloudController> callback =
                    mCallbackController.makeCancelable(
                            (ReadAloudController readAloudController) -> {
                                if (mTabBottomSheetManager != null) {
                                    mTabBottomSheetManager.initReadAloudIntegration(
                                            readAloudController.getActivePlaybackTabSupplier(),
                                            () -> {
                                                readAloudController.maybeStopPlayback(
                                                        null,
                                                        ReasonForStoppingPlayback.MANUAL_CLOSE);
                                            });
                                }
                                assert mTabBottomSheetReadAloudControllerCallback != null;
                                mReadAloudControllerSupplier.removeObserver(
                                        mTabBottomSheetReadAloudControllerCallback);
                                mTabBottomSheetReadAloudControllerCallback = null;
                            });
            mTabBottomSheetReadAloudControllerCallback = callback;
            mReadAloudControllerSupplier.addSyncObserverAndCallIfNonNull(callback);
        }
    }

    @SuppressWarnings("NewApi") // OS version check is done via helper method.
    private static @Nullable AppHeaderCoordinator initAppHeaderCoordinator(
            AppCompatActivity activity,
            @Nullable Bundle savedInstanceState,
            @Nullable PersistableBundle persistentState,
            EdgeToEdgeStateProvider edgeToEdgeStateProvider,
            BrowserControlsManager browserControlsManager,
            InsetObserver insetObserver,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @Nullable MultiInstanceManager multiInstanceManager) {
        boolean isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity);
        if (!ToolbarFeatures.isAppHeaderCustomizationSupported(
                isTablet, DisplayUtil.isContextInDefaultDisplay(activity))) {
            return null;
        }

        return new AppHeaderCoordinator(
                activity,
                activity.getWindow().getDecorView().getRootView(),
                browserControlsManager.getBrowserVisibilityDelegate(),
                insetObserver,
                activityLifecycleDispatcher,
                savedInstanceState,
                persistentState,
                edgeToEdgeStateProvider,
                () ->
                        multiInstanceManager == null
                                ? TabWindowManager.INVALID_WINDOW_ID
                                : multiInstanceManager.getCurrentInstanceId());
    }

    private void initCollaborationDelegatesOnProfile(Profile profile) {
        if (!TabUiUtils.isDataSharingFunctionalityEnabled()) return;

        // We must use the original non-OTR profile here.
        Profile originalProfile = profile.getOriginalProfile();

        CollaborationService collaborationService =
                CollaborationServiceFactory.getForProfile(originalProfile);

        mDataSharingTabManager.initWithProfile(
                originalProfile,
                DataSharingServiceFactory.getForProfile(originalProfile),
                MessagingBackendServiceFactory.getForProfile(originalProfile),
                collaborationService);

        TabModelUtils.onInitializedTabModelSelector(mTabModelSelectorSupplier)
                .runSyncOrOnAvailable(
                        selector -> {
                            mInstantMessageDelegateImpl =
                                    InstantMessageDelegateFactory.getForProfile(originalProfile);
                            TabModel tabModel = selector.getModel(/* incognito= */ false);
                            DataSharingNotificationManager dataSharingNotificationManager =
                                    new DataSharingNotificationManager(mActivity);
                            mInstantMessageDelegateImpl.attachWindow(
                                    mWindowAndroid,
                                    tabModel,
                                    dataSharingNotificationManager,
                                    mDataSharingTabManager,
                                    () -> {
                                        return MultiWindowUtils.getInstanceCount(
                                                                PersistedInstanceType.ANY)
                                                        <= 1
                                                || ApplicationStatus.getLastTrackedFocusedActivity()
                                                        == mActivity;
                                    });
                        });
    }

    private DataSharingTabGroupsDelegate createDataSharingTabGroupsDelegate() {
        return new DataSharingTabGroupsDelegate() {
            @Override
            public void openTabGroup(@Nullable Token tabGroupId) {
                TabModel tabModel = mTabModelSelectorSupplier.asNonNull().get().getModel(false);
                if (!tabModel.tabGroupExists(tabGroupId)) {
                    // This method is only supposed to be called when the tab group is in the local
                    // model. However it's possible that something has recently changed. In which
                    // case just be defensive and give up.
                    return;
                }

                // Due to crbug.com/396159718 (see also crbug.com/395847973) it was possible to
                // reach this method without the tab switcher being open. For now this will
                // remain as a safegaurd against bugs as internally it will noop if the tab switcher
                // is already opened. It could be removed in the future with some care taken to add
                // an assert and verify no callers are using it in an unexpected flow.
                int tabId = tabModel.getGroupLastShownTabId(tabGroupId);
                assert mLayoutManager != null;
                TabSwitcherUtils.navigateToTabSwitcher(
                        mLayoutManager,
                        /* animate= */ false,
                        () -> {
                            var tabSwitcher = mTabSwitcherSupplier.get();
                            assert tabSwitcher != null;
                            tabSwitcher.requestOpenTabGroupDialog(tabId);
                        });
            }

            @Override
            public void openUrlInChromeCustomTab(Context context, GURL gurl) {
                CustomTabActivity.showInfoPage(context, gurl.getSpec());
            }

            @Override
            public void hideTabSwitcherAndShowTab(int tabId) {
                TabSwitcherUtils.hideTabSwitcherAndShowTab(
                        tabId, mTabModelSelectorSupplier.get(), mLayoutManager);
            }

            @Override
            public void getPreviewBitmap(
                    String collaborationId, int size, Callback<@Nullable Bitmap> callback) {
                @Nullable TabGroupSyncService tabGroupSyncService =
                        TabGroupSyncServiceFactory.getForProfile(
                                mProfileSupplier.asNonNull().get());
                if (tabGroupSyncService == null) {
                    callback.onResult(null);
                    return;
                }

                @Nullable SavedTabGroup savedTabGroup =
                        DataSharingTabGroupUtils.getTabGroupForCollabIdFromSync(
                                collaborationId, tabGroupSyncService);
                if (savedTabGroup == null) {
                    callback.onResult(null);
                    return;
                }

                TabListFaviconProvider tabListFaviconProvider =
                        new TabListFaviconProvider(
                                mActivity,
                                false,
                                R.dimen.default_favicon_corner_radius,
                                TabFavicon::getBitmap);
                FaviconResolver faviconResolver =
                        TabGroupListFaviconResolverFactory.build(
                                mActivity,
                                mProfileSupplier.asNonNull().get(),
                                tabListFaviconProvider);

                Callback<@Nullable Bitmap> cleanUpAndContinue =
                        (@Nullable Bitmap bitmap) -> {
                            tabListFaviconProvider.destroy();
                            callback.onResult(bitmap);
                        };
                TabGroupFaviconCluster.createBitmapFrom(
                        savedTabGroup, mActivity, faviconResolver, cleanUpAndContinue);
            }

            @Override
            public @WindowId int findWindowIdForTabGroup(@Nullable Token tabGroupId) {
                assert tabGroupId != null;
                return TabWindowManagerSingleton.getInstance().findWindowIdForTabGroup(tabGroupId);
            }

            @Override
            public void launchIntentInMaybeClosedWindow(Intent intent, @WindowId int windowId) {
                MultiWindowUtils.launchIntentInMaybeClosedWindow(mActivity, intent, windowId);
            }
        };
    }

    @Override
    public void initContextualSearchManager() {
        super.initContextualSearchManager();
        if (AndroidSidePanelEnabledFn.isEnabled()) {
            mContextualSearchManagerSupplier
                    .asNonNull()
                    .get()
                    .setSideUiStateProviderSupplier(mSideUiStateProviderSupplier);
        }
    }

    private void initializeSideUi(Profile currentlySelectedProfile) {
        ViewGroup anchorContainerParent = mActivity.findViewById(R.id.constrained_views_container);
        ViewStub sideUiStartAnchorContainerStub =
                mActivity.findViewById(R.id.side_ui_left_anchor_container_stub);
        ViewStub sideUiEndAnchorContainerStub =
                mActivity.findViewById(R.id.side_ui_right_anchor_container_stub);

        NonNullObservableSupplier<Integer> stripBottomPxSupplier = null;
        assumeNonNull(mLayoutManager);
        StripLayoutHelperManager stripLayoutHelperManager =
                mLayoutManager.getStripLayoutHelperManager();
        if (stripLayoutHelperManager != null) {
            stripBottomPxSupplier = stripLayoutHelperManager.getStripBottomPxSupplier();
        }

        mSideUiCoordinator =
                SideUiCoordinatorFactory.create(
                        mActivity,
                        mActivityLifecycleDispatcher,
                        anchorContainerParent,
                        sideUiStartAnchorContainerStub,
                        sideUiEndAnchorContainerStub,
                        stripBottomPxSupplier);
        if (mSideUiCoordinator == null) {
            return;
        }

        mSidePanelContainerCoordinator =
                SidePanelContainerCoordinatorFactory.create(mActivity, mSideUiCoordinator);
        if (mSidePanelContainerCoordinator != null) {
            // Initialize SidePanelCoordinatorAndroid and a window-scoped SidePanelRegistry, and
            // associate them with a ChromeAndroidTask.
            // This will allow SidePanelCoordinatorAndroid and SidePanelRegistry to access the
            // native BrowserWindowInterface and ensure the lifecycle and destruction order for both
            // are correct.
            //
            // Note:
            //
            // (1) ChromeAndroidTask should be non-null here as ChromeAndroidTask is initialized
            // immediately after native initialization, along with TabModel;
            //
            // (2) The lifecycles of SidePanelCoordinatorAndroid and the window-scoped
            // SidePanelRegistry are in sync with a native BrowserWindowInterface, but
            // SidePanelCoordinatorAndroid doesn't own the SidePanelRegistry, or vice versa. This
            // matches the WML implementation.
            var chromeAndroidTask = mChromeAndroidTaskSupplier.get();
            assert chromeAndroidTask != null
                    : "ChromeAndroidTask shouldn't be null when side panel is enabled";

            var sidePanelCoordinatorAndroid =
                    (SidePanelCoordinatorAndroid)
                            chromeAndroidTask.addFeature(
                                    new ChromeAndroidTaskFeatureKey(
                                            SidePanelCoordinatorAndroid.class,
                                            currentlySelectedProfile,
                                            mWindowAndroid),
                                    () ->
                                            SidePanelCoordinatorAndroidFactory.create(
                                                    mSidePanelContainerCoordinator));
            assert sidePanelCoordinatorAndroid != null
                    : "SidePanelCoordinatorAndroid shouldn't be null when side panel is enabled";

            chromeAndroidTask.addFeature(
                    new ChromeAndroidTaskFeatureKey(
                            WindowScopedSidePanelRegistryBridge.class,
                            currentlySelectedProfile,
                            mWindowAndroid),
                    SidePanelRegistryBridgeFactory::createWindowScopedBridge);

            mSidePanelContainerCoordinator.init(sidePanelCoordinatorAndroid);

            // TODO(crbug.com/489548570): Remove SidePanelDevFeature when it's not needed.
            mSidePanelDevFeature =
                    SidePanelDevFeatureFactory.create(
                            mProfileSupplier,
                            mSidePanelContainerCoordinator,
                            mWindowAndroid,
                            mActivityTabProvider);
        }
        if (VerticalTabUtils.isVerticalTabsEligible(mActivity)) {
            mVerticalTabsSideUiCoordinator =
                    new VerticalTabsSideUiCoordinator(
                            mActivity,
                            mSideUiCoordinator,
                            new VerticalTabListCoordinator(
                                    mActivity,
                                    assumeNonNull(mTabModelSelectorSupplier.get()),
                                    assumeNonNull(mProfileSupplier.get())));
            mSideUiCoordinator.registerSideUiContainer(mVerticalTabsSideUiCoordinator);
        }
        mSideUiStateProviderSupplier.set(mSideUiCoordinator);

        // TODO(crbug.com/510890983): Add render tests for the secondary container adjustment.
        View secondaryUiContainer = mActivity.findViewById(R.id.secondary_ui_container);
        mSecondaryUiContainerMarginAdjuster = new ViewMarginAdjusterForSideUi(secondaryUiContainer);
        mSideUiCoordinator.addObserver(mSecondaryUiContainerMarginAdjuster);

        // Restore the user's saved tab layout preference upon browser cold launch.
        if (VerticalTabUtils.isVerticalTabsEligible(mActivity)) {
            boolean useVerticalLayoutOnLaunch =
                    ChromeSharedPreferences.getInstance()
                            .readBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, false);

            if (useVerticalLayoutOnLaunch) {
                var transitionCoordinator =
                        assumeNonNull(mToolbarManager).getTabStripTransitionCoordinator();
                assumeNonNull(transitionCoordinator).suppressTabStrip(true);
                assumeNonNull(mVerticalTabsSideUiCoordinator).setVisible(true);
            }
        }
    }

    /** Toggle the visibility between horizontal tab strip and vertical tab list. */
    public void toggleTabStrip() {
        var transitionCoordinator =
                assumeNonNull(mToolbarManager).getTabStripTransitionCoordinator();
        // TODO(crbug.com/509226293):
        //    - Coordinate horizontal/vertical tab animation.
        boolean shouldShowVerticalTabs =
                !ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, false);

        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, shouldShowVerticalTabs);

        assumeNonNull(transitionCoordinator).suppressTabStrip(shouldShowVerticalTabs);
        assumeNonNull(mVerticalTabsSideUiCoordinator).setVisible(shouldShowVerticalTabs);
    }

    private void destroySideUi() {
        // The destruction order matters:
        //
        // Each SideUiContainer implementation that's registered with SideUiCoordinator should be
        // destroyed before SideUiCoordinator.

        // The CompositorViewHolder itself is a SideUiObserver and queries its reference of the
        // SideUiCoordinator. It's expected to be null by this point.
        assert mCompositorViewHolderSupplier.get() == null;

        // TODO(crbug.com/489548570): Remove SidePanelDevFeature when it's not needed.
        if (mSidePanelDevFeature != null) {
            mSidePanelDevFeature.destroy();
            mSidePanelDevFeature = null;
        }

        if (mSidePanelContainerCoordinator != null) {
            mSidePanelContainerCoordinator.destroy();
            mSidePanelContainerCoordinator = null;
        }

        if (mSideUiCoordinator != null) {
            // Remove observers.
            if (mSecondaryUiContainerMarginAdjuster != null) {
                mSideUiCoordinator.removeObserver(mSecondaryUiContainerMarginAdjuster);
            }

            mSideUiCoordinator.destroy();
            mSideUiCoordinator = null;
        }
    }

    public @Nullable SidePanelContainerCoordinator getSidePanelContainerCoordinatorForTesting() {
        return mSidePanelContainerCoordinator;
    }

    public @Nullable SidePanelDevFeature getSidePanelDevFeatureForTesting() {
        return mSidePanelDevFeature;
    }

    /** Returns the {@link TabGroupSyncControllerImpl} if it has been created yet. */
    public TabGroupSyncController getTabGroupSyncController() {
        assert mTabGroupSyncController != null;
        return mTabGroupSyncController;
    }

    @Override
    protected boolean supportsEdgeToEdge() {
        return EdgeToEdgeUtils.isEdgeToEdgeBottomChinEnabled(mActivity);
    }

    public @Nullable StatusIndicatorCoordinator getStatusIndicatorCoordinatorForTesting() {
        return mStatusIndicatorCoordinator;
    }

    public @Nullable HistoryNavigationCoordinator getHistoryNavigationCoordinatorForTesting() {
        return mHistoryNavigationCoordinator;
    }

    public @Nullable NavigationSheet getNavigationSheetForTesting() {
        return mNavigationSheet;
    }

    public @Nullable TabbedSystemUiCoordinator getTabbedSystemUiCoordinatorForTesting() {
        return mSystemUiCoordinator;
    }

    public @Nullable ActorOverlayCoordinator getActorOverlayCoordinatorForTesting() {
        return mGlicUiCoordinator != null
                ? mGlicUiCoordinator.getActorOverlayCoordinatorForTesting() // IN-TEST
                : null;
    }

    public @Nullable TabBottomSheetManager getTabBottomSheetManagerForTesting() {
        return mTabBottomSheetManager;
    }

    /** Called when a link is copied through context menu. */
    public void onContextMenuCopyLink() {
        // The iph controller will be null before tracker fully initialized.
        if (mReadLaterIphController == null) return;
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
        if (ChoiceDialogCoordinator.maybeShow(
                mActivity, mModalDialogManagerSupplier.get(), mActivityLifecycleDispatcher)) {
            return true;
        }

        assert mMessageDispatcher != null;
        mPrivacySandbox3pcdRollbackMessageController =
                new PrivacySandbox3pcdRollbackMessageController(
                        mActivity, profile, mActivityTabProvider, mMessageDispatcher);
        if (mPrivacySandbox3pcdRollbackMessageController.maybeShow()) {
            return true;
        }

        final Supplier<RationaleDelegate> rationaleUIDelegateSupplier =
                () ->
                        new NotificationPermissionRationaleDialogController(
                                mActivity, mModalDialogManagerSupplier.get());
        mNotificationPermissionController =
                new NotificationPermissionController(mWindowAndroid, rationaleUIDelegateSupplier);
        NotificationPermissionController.attach(mWindowAndroid, mNotificationPermissionController);
        if (mNotificationPermissionController.requestPermissionIfNeeded(/* contextual= */ false)) {
            return true;
        }

        assert mAdvancedProtectionCoordinator != null;
        if (mAdvancedProtectionCoordinator.showMessageOnStartupIfNeeded()) {
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
            // TODO(https://crbug.com/40585866, pnoland): Unify promo dialog logic and move into a
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
        if (FullscreenSigninPromoLauncher.launchPromoIfForced(
                mActivity, profile, SigninAndHistorySyncActivityLauncherImpl.get())) {
            return true;
        }
        if (PwaRestorePromoUtils.maybeForceShowPromo(profile, mWindowAndroid)) {
            return true;
        }

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
            UserEducationUtils.recordOptionalPromoType(OptionalPromoType.PWA_RESTORE_PROMO);
            return true;
        }
        if (FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                mActivity,
                profile,
                SigninAndHistorySyncActivityLauncherImpl.get(),
                VersionInfo.getProductMajorVersion())) {
            UserEducationUtils.recordOptionalPromoType(OptionalPromoType.FULLSCREEN_SIGNIN_PROMO);
            return true;
        }
        if (DefaultBrowserPromoUtils.getInstance()
                .prepareLaunchPromoIfNeeded(
                        mActivity,
                        mWindowAndroid,
                        TrackerFactory.getTrackerForProfile(profile),
                        DefaultBrowserPromoUtils.DefaultBrowserPromoEntryPoint.CHROME_STARTUP)) {
            UserEducationUtils.recordOptionalPromoType(OptionalPromoType.DEFAULT_BROWSER_PROMO);
            return true;
        }
        if (AppLanguagePromoDialog.maybeShowPrompt(
                mActivity,
                profile,
                mModalDialogManagerSupplier.get(),
                () -> ApplicationLifetime.terminate(true))) {
            UserEducationUtils.recordOptionalPromoType(OptionalPromoType.APP_LANGUAGE_PROMO);
            return true;
        }

        if (AppRatingPromoController.maybeShowPromo(profile, mActivity)) {
            UserEducationUtils.recordOptionalPromoType(OptionalPromoType.APP_RATING_PROMPT);
            return true;
        }

        UserEducationUtils.recordOptionalPromoType(OptionalPromoType.NONE_SHOWN);
        return false;
    }

    private void createBookmarkBarIfNecessary() {
        if (mBookmarkOpener == null) {
            mBookmarkOpener =
                    new BookmarkOpenerImpl(
                            mBookmarkModelSupplier, mActivity, mActivity.getComponentName());
        }

        if (mBookmarkBarCoordinator == null) {
            assert mLayoutManager != null;
            mBookmarkBarCoordinator =
                    new BookmarkBarCoordinator(
                            mActivity,
                            mActivityLifecycleDispatcher,
                            mLayoutManager,
                            mLayoutManager::requestUpdate,
                            mFullscreenManager,
                            mCompositorViewHolderSupplier.asNonNull().get().getResourceManager(),
                            mBrowserControlsManager,
                            result -> updateTopControlsHeight(false),
                            mProfileSupplier,
                            mActivity.findViewById(R.id.bookmark_bar_stub),
                            mActivityTabProvider.get(),
                            mBookmarkOpener,
                            mBookmarkManagerOpenerSupplier,
                            mTopControlsStacker,
                            mActivityTabProvider.asObservable(),
                            getTopUiThemeColorProvider(),
                            mSideUiStateProviderSupplier);
            if (mBookmarkBarVisibilityProvider != null) {
                mBookmarkBarVisibilityProvider.addObserver(mBookmarkBarCoordinator);
            }
            mBookmarkBarHeightSupplier = mBookmarkBarCoordinator::getTopControlHeight;
            mLayoutManager.addSceneOverlay(mBookmarkBarCoordinator.getSceneLayer());
            mLayoutManager.requestUpdate();

            // Requesting a layer update must come after the heightSupplier has been set.
            if (mToolbarManager != null) {
                mToolbarManager.setBookmarkBarHeightSupplier(mBookmarkBarHeightSupplier);
            }
            mTopControlsStacker.requestLayerUpdateSync(false);
        } else {
            mBookmarkBarCoordinator.setVisibility(true);
            // When toggling the visibility of the existing view, the LayoutChangeListener will not
            // be triggered as it is on instantiation, so we update the top controls height here.
            // The height supplier should already be set since the coordinator was not destroyed,
            // and the following method will also request a layer update.
            updateTopControlsHeight(false);
        }
    }

    private void updateBookmarkBarIfNecessary(boolean visible) {
        if (visible) {
            // We create the BookmarkBar, but we do not update the Top Controls height here since
            // the view's height requires a layout pass to be correct (until then it will return the
            // intrinsic height, and being a LinearLayout inside a WRAP_CONTENT ViewStub, this will
            // be 0). The BookmarkBarCoordinator adds a LayoutChangeListener to the view; during
            // onLayoutChange we will have the correct height and update the top controls then.
            createBookmarkBarIfNecessary();
            if (mToolbarManager != null) {
                mToolbarManager.setProgressBarAnchorView(R.id.bookmark_bar);
            }
        } else {
            if (mBookmarkBarCoordinator != null) {
                mBookmarkBarCoordinator.setVisibility(false);
                updateTopControlsHeight(false);
                if (mToolbarManager != null) {
                    mToolbarManager.setProgressBarAnchorView(R.id.control_container);
                }
            }
        }
    }

    @Override
    public boolean getBookmarkBarVisibility() {
        return BookmarkBarUtils.isBookmarkBarVisible(
                mActivity, mProfileSupplier.get(), mXrSpaceModeObservableSupplier.get());
    }

    public int getBookmarkBarHeight() {
        return mBookmarkBarCoordinator != null && mBookmarkBarCoordinator.isVisible()
                ? mBookmarkBarCoordinator.getTopControlHeight()
                : 0;
    }

    public static void setDisableTopControlsAnimationsForTesting(boolean disable) {
        sDisableTopControlsAnimationForTesting = disable;
        ResettersForTesting.register(() -> sDisableTopControlsAnimationForTesting = false);
    }

    /* package */ void initializeBookmarkBarCoordinatorForTesting() {
        createBookmarkBarIfNecessary();
    }

    // MenuOrKeyboardActionHandler implementation
    @Override
    public boolean handleMenuOrKeyboardAction(int id, boolean fromMenu) {
        if (super.handleMenuOrKeyboardAction(id, fromMenu)) return true;
        if (id == R.id.switch_keyboard_focus_row) {
            mKeyboardFocusRowManager.onKeyboardFocusRowSwitch();
            return true;
        } else if (id == R.id.open_tab_strip_context_menu) {
            assumeNonNull(mLayoutManager);
            @Nullable StripLayoutHelperManager stripLayoutHelperManager =
                    mLayoutManager.getStripLayoutHelperManager();
            if (stripLayoutHelperManager == null) return false;
            return stripLayoutHelperManager.openKeyboardFocusedContextMenu();
        } else if (id == R.id.focus_bookmarks) {
            if (mBookmarkBarCoordinator != null && mBookmarkBarCoordinator.isVisible()) {
                mBookmarkBarCoordinator.requestFocus();
            }
            return true;
        } else if (id == R.id.toggle_bookmark_bar) {
            if (BookmarkBarUtils.isActivityStateBookmarkBarCompatible(mActivity)) {
                if (DeviceInfo.isDesktop()) {
                    // Desktop uses the synced UserPref.
                    BookmarkBarUtils.toggleUserPrefsShowBookmarksBar(
                            mProfileSupplier.asNonNull().get(), /* fromKeyboardShortcut= */ true);
                } else {
                    // Tablet uses the local shared pref (but interacts with policy via Profile).
                    BookmarkBarUtils.toggleDevicePrefShowBookmarksBar(
                            mProfileSupplier.asNonNull().get(), /* fromKeyboardShortcut= */ true);
                }
                return true;
            }
        } else if (id == R.id.close_window) {
            mActivity.finishAndRemoveTask();
            return true;
        } else if (id == R.id.glic_menu_id) {
            return toggleGlic(false, GlicInvocationSource.THREE_DOTS_MENU);
        }
        return false;
    }

    /**
     * Toggles the Glic UI.
     *
     * @param preventClose whether to prevent closing the Glic UI if it's already open.
     * @param invocationSource How the UI was triggered.
     * @return whether the UI was successfully toggled.
     */
    @Override
    public boolean toggleGlic(boolean preventClose, @GlicInvocationSource int invocationSource) {
        Profile profile =
                mTabModelSelectorSupplier.asNonNull().get().getCurrentModel().getProfile();
        if (profile == null || !GlicEnabling.isEnabledForProfile(profile)) {
            return false;
        }

        Tab currentTab = mActivityTabProvider.get();
        boolean isNtp = currentTab != null && UrlUtilities.isNtpUrl(currentTab.getUrl());
        GlicMetrics.recordEntryPointClick(invocationSource, isNtp);
        // TODO(crbug.com/489548570): Remove this entry point into SidePanelDevFeature.
        if (mSidePanelDevFeature != null) {
            mSidePanelDevFeature.toggle();
            return true;
        }

        if (mTabBottomSheetManager != null && mTabBottomSheetManager.isInPeekMode()) {
            mTabBottomSheetManager.setSheetExpanded(true);
            return true;
        }

        return GlicKeyedServiceHandler.toggleGlic(
                profile, mChromeAndroidTaskSupplier.get(), preventClose, invocationSource);
    }

    /* package */ KeyboardFocusRowManager getKeyboardFocusRowManagerForTesting() {
        return mKeyboardFocusRowManager;
    }

    private void setActivityTitle(@Nullable Tab tab, boolean isTabSwitcher) {
        // Do not update title after Activity destruction.
        if (mActivity == null) {
            return;
        }

        String title =
                TextUtils.isEmpty(mApplicationLabel)
                        ? mActivity
                                .getResources()
                                .getString(R.string.accessibility_default_app_label)
                        : mApplicationLabel.toString();
        String subTitle;
        if (isTabSwitcher) {
            subTitle =
                    mActivity.getResources().getString(R.string.accessibility_tab_switcher_title);
        } else if (tab != null) {
            subTitle = tab.getTitle();
        } else {
            subTitle = "";
        }

        if (TextUtils.isEmpty(subTitle)) {
            mActivity.setTitle(title);
        } else {
            mActivity.setTitle(title + ": " + subTitle);
        }
    }

    /**
     * Called after a daily refresh theme collection has been applied to the NTP. Triggers fetching
     * the next image for the theme collection to be used for the following day's refresh.
     */
    public void onDailyRefreshThemeCollectionApplied() {
        if (mNtpSyncedThemeManager != null) {
            mNtpSyncedThemeManager.fetchNextThemeCollectionImageAfterDailyRefreshApplied();
        }
    }

    private void waitForTrackerInit(Profile profile) {
        Tracker trackerForProfile = TrackerFactory.getTrackerForProfile(profile);
        if (trackerForProfile.isInitialized()) {
            mTrackerInitializedOneshotSupplier.set(Boolean.TRUE);
        } else {
            trackerForProfile.addOnInitializedCallback(
                    result -> {
                        mTrackerInitializedOneshotSupplier.set(true);
                    });
        }
    }

    /** Returns the {@link OneshotSupplier} for the {@link SideUiStateProvider}. */
    public OneshotSupplier<SideUiStateProvider> getSideUiStateProviderSupplier() {
        return mSideUiStateProviderSupplier;
    }

    @Nullable GlicPromoCoordinator getGlicPromoCoordinatorForTesting() {
        return mGlicPromoCoordinator;
    }
}
