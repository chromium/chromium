// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.app.Fragment;
import android.content.ComponentName;
import android.content.Intent;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.provider.Browser;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.CallSuper;
import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.DeviceInfo;
import org.chromium.base.TraceEvent;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneShotCallback;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.ChromeActionModeHandler;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.IntentHandler.TabOpenType;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.automotivetoolbar.AutomotiveBackButtonToolbarCoordinator;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager.ContextualSearchTabPromotionDelegate;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManagerSupplier;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchObserver;
import org.chromium.chrome.browser.crash.ChromePureJavaExceptionReporter;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.desktop_site.DesktopSiteUtils;
import org.chromium.chrome.browser.device_lock.DeviceLockActivityLauncherImpl;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.dom_distiller.ReaderModeBottomSheetManager;
import org.chromium.chrome.browser.dom_distiller.ReaderModeIphController;
import org.chromium.chrome.browser.download.DownloadMetrics.OpenWithExternalAppsSource;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.findinpage.FindToolbarManager;
import org.chromium.chrome.browser.findinpage.FindToolbarObserver;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenBackPressHandler;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.host_zoom.HostZoomListenerFactory;
import org.chromium.chrome.browser.image_descriptions.ImageDescriptionsController;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthControllerImpl;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthCoordinatorFactory;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.lifecycle.WindowFocusChangedObserver;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustMetrics;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustSignalsCoordinator;
import org.chromium.chrome.browser.messages.ChromeMessageAutodismissDurationProvider;
import org.chromium.chrome.browser.messages.ChromeMessageQueueMediator;
import org.chromium.chrome.browser.messages.MessageContainerCoordinator;
import org.chromium.chrome.browser.messages.MessageContainerObserver;
import org.chromium.chrome.browser.messages.MessagesResourceMapperInitializer;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.edge_to_edge.TopInsetCoordinator;
import org.chromium.chrome.browser.omnibox.OmniboxActionDelegateImpl;
import org.chromium.chrome.browser.omnibox.OmniboxFocusReason;
import org.chromium.chrome.browser.omnibox.geo.GeolocationHeader;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.paint_preview.DemoPaintPreview;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerLauncher;
import org.chromium.chrome.browser.pdf.PdfPage;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.quick_delete.QuickDeleteController;
import org.chromium.chrome.browser.quick_delete.QuickDeleteDelegateImpl;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.readaloud.ReadAloudControllerSupplier;
import org.chromium.chrome.browser.recent_tabs.RestoreTabsFeatureHelper;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin;
import org.chromium.chrome.browser.share.qrcode.QrCodeDialog;
import org.chromium.chrome.browser.share.scroll_capture.ScrollCaptureManager;
import org.chromium.chrome.browser.tab.AccessibilityVisibilityHandler;
import org.chromium.chrome.browser.tab.AutofillSessionLifetimeController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabLoadIfNeededCaller;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tab.TabObscuringHandlerSupplier;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.RecyclerViewPosition;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabwindow.TabWindowInfo;
import org.chromium.chrome.browser.theme.AdjustedTopUiThemeColorProvider;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ToolbarIntentMetadata;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarBehavior;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.toolbar.top.ToolbarActionModeCallback;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer;
import org.chromium.chrome.browser.toolbar.top.tab_strip.StripVisibilityState;
import org.chromium.chrome.browser.ui.activity_recreation.ActivityRecreationController;
import org.chromium.chrome.browser.ui.appmenu.AppMenuBlocker;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinatorFactory;
import org.chromium.chrome.browser.ui.appmenu.AppMenuDelegate;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuObserver;
import org.chromium.chrome.browser.ui.appmenu.AppMenuSubmenuHeaderItemProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuUtil;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerCreator;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerFactory;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils.MissingNavbarInsetsReason;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;
import org.chromium.chrome.browser.ui.system.StatusBarColorController.StatusBarColorProvider;
import org.chromium.chrome.browser.wallet.BoardingPassController;
import org.chromium.components.browser_ui.accessibility.PageZoomBarCoordinator;
import org.chromium.components.browser_ui.accessibility.PageZoomBarCoordinatorDelegate;
import org.chromium.components.browser_ui.accessibility.PageZoomManager;
import org.chromium.components.browser_ui.accessibility.PageZoomManagerDelegate;
import org.chromium.components.browser_ui.accessibility.ZoomEventsObserver;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.ExpandedSheetHelper;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager.AppHeaderObserver;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncherSupplier;
import org.chromium.components.browser_ui.util.ComposedBrowserControlsVisibilityDelegate;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager.ScrimClient;
import org.chromium.components.dom_distiller.core.DomDistillerFeatures;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.ManagedMessageDispatcher;
import org.chromium.components.messages.MessageContainer;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessagesFactory;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.content_public.browser.ActionModeCallbackHelper;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.edge_to_edge.EdgeToEdgeManager;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuController;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuController.SubmenuHeaderFactory;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.Toast;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.function.BooleanSupplier;
import java.util.function.Supplier;

/**
 * The root UI coordinator. This class will eventually be responsible for inflating and managing
 * lifecycle of the main UI components.
 *
 * <p>The specific things this component will manage and how it will hook into Chrome*Activity are
 * still being discussed See https://crbug.com/931496.
 */
public class RootUiCoordinator
        implements DestroyObserver,
                InflationObserver,
                NativeInitObserver,
                MenuOrKeyboardActionController.MenuOrKeyboardActionHandler,
                AppMenuBlocker,
                ContextualSearchTabPromotionDelegate,
                WindowFocusChangedObserver {

    protected final UnownedUserDataSupplier<TabObscuringHandler> mTabObscuringHandlerSupplier =
            new TabObscuringHandlerSupplier();

    private final UnownedUserDataSupplier<DeviceLockActivityLauncher>
            mDeviceLockActivityLauncherSupplier = new DeviceLockActivityLauncherSupplier();

    protected final UnownedUserDataSupplier<ContextualSearchManager>
            mContextualSearchManagerSupplier = new ContextualSearchManagerSupplier();

    protected final UnownedUserDataSupplier<ReadAloudController> mReadAloudControllerSupplier =
            new ReadAloudControllerSupplier();

    protected AppCompatActivity mActivity;
    protected @Nullable AppMenuCoordinator mAppMenuCoordinator;
    private final MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    protected final ActivityWindowAndroid mWindowAndroid;
    private final OneshotSupplier<ChromeAndroidTask> mChromeAndroidTaskSupplier;

    protected final ActivityTabProvider mActivityTabProvider;
    protected ObservableSupplier<ShareDelegate> mShareDelegateSupplier;

    protected @Nullable FindToolbarManager mFindToolbarManager;
    private @Nullable FindToolbarObserver mFindToolbarObserver;

    private OverlayPanelManager mOverlayPanelManager;
    private OverlayPanelManager.OverlayPanelManagerObserver mOverlayPanelManagerObserver;

    protected OneshotSupplier<LayoutStateProvider> mLayoutStateProviderOneShotSupplier;
    protected LayoutStateProvider mLayoutStateProvider;
    private LayoutStateProvider.LayoutStateObserver mLayoutStateObserver;

    /**
     * A controller which is used to show an Incognito re-auth dialog when the feature is available.
     */
    private @Nullable IncognitoReauthController mIncognitoReauthController;

    /**
     * An {@link OneshotSupplierImpl} of the {@link IncognitoReauthController} that can be used by
     * clients to check to see if a re-auth is being shown or not.
     */
    private final OneshotSupplierImpl<IncognitoReauthController>
            mIncognitoReauthControllerOneshotSupplier = new OneshotSupplierImpl<>();

    /** A means of providing the theme color to different features. */
    private TopUiThemeColorProvider mTopUiThemeColorProvider;

    /** A subclass of TopUiThemeColorProvider to provide adjusted tint color. */
    private AdjustedTopUiThemeColorProvider mAdjustedTopUiThemeColorProvider;

    @Nullable private final Callback<Boolean> mOnOmniboxFocusChangedListener;
    protected ToolbarManager mToolbarManager;
    private ModalDialogManagerObserver mModalDialogManagerObserver;

    private BottomSheetManager mBottomSheetManager;
    private ManagedBottomSheetController mBottomSheetController;
    private SnackbarManager mBottomSheetSnackbarManager;

    private ScrimManager mScrimManager;
    private final ToolbarActionModeCallback mActionModeControllerCallback;
    private final ObservableSupplierImpl<Boolean> mOmniboxFocusStateSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<MerchantTrustSignalsCoordinator>
            mMerchantTrustSignalsCoordinatorSupplier = new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<ScrimManager> mScrimManagerSupplier =
            new ObservableSupplierImpl<>();
    protected final ObservableSupplier<Profile> mProfileSupplier;
    protected final ObservableSupplier<BookmarkModel> mBookmarkModelSupplier;
    protected final ObservableSupplier<TabBookmarker> mTabBookmarkerSupplier;
    private final OneshotSupplierImpl<AppMenuCoordinator> mAppMenuSupplier;
    private BottomSheetObserver mBottomSheetObserver;
    protected final CallbackController mCallbackController;
    protected final BrowserControlsManager mBrowserControlsManager;
    private BrowserControlsStateProvider.Observer mBrowserControlsObserver;
    protected final ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    protected final OneshotSupplier<TabSwitcher> mTabSwitcherSupplier;
    protected final OneshotSupplier<TabSwitcher> mIncognitoTabSwitcherSupplier;
    @Nullable protected ManagedMessageDispatcher mMessageDispatcher;
    @Nullable private MessageContainerCoordinator mMessageContainerCoordinator;
    private MessageContainerObserver mMessageContainerObserver;
    @Nullable private ChromeMessageQueueMediator mMessageQueueMediator;
    private LayoutManagerImpl mLayoutManager;
    protected OneshotSupplier<ToolbarIntentMetadata> mIntentMetadataOneshotSupplier;
    protected OneshotSupplierImpl<Boolean> mPromoShownOneshotSupplier = new OneshotSupplierImpl<>();
    @Nullable private VoiceRecognitionHandler.Observer mMicStateObserver;
    private MediaCaptureOverlayController mCaptureController;
    private @Nullable ScrollCaptureManager mScrollCaptureManager;
    protected final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    protected final ObservableSupplier<LayoutManagerImpl> mLayoutManagerImplSupplier;
    protected final ObservableSupplier<@StripVisibilityState Integer> mTabStripVisibilitySupplier;
    protected final ObservableSupplierImpl<LayoutManager> mLayoutManagerSupplier;
    protected final ObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final AppMenuBlocker mAppMenuBlocker;
    private final BooleanSupplier mSupportsAppMenuSupplier;
    protected final BooleanSupplier mSupportsFindInPageSupplier;
    protected final Supplier<TabCreatorManager> mTabCreatorManagerSupplier;
    protected final FullscreenManager mFullscreenManager;
    protected final Supplier<CompositorViewHolder> mCompositorViewHolderSupplier;
    protected StatusBarColorController mStatusBarColorController;
    protected final Supplier<SnackbarManager> mSnackbarManagerSupplier;
    protected final ObservableSupplierImpl<EdgeToEdgeController> mEdgeToEdgeControllerSupplier;
    protected Destroyable mEdgeToEdgeBottomChin;
    protected final @ActivityType int mActivityType;
    protected final Supplier<Boolean> mIsInOverviewModeSupplier;
    private final AppMenuDelegate mAppMenuDelegate;
    private final Supplier<TabContentManager> mTabContentManagerSupplier;
    private final IntentRequestTracker mIntentRequestTracker;
    private final boolean mInitializeUiWithIncognitoColors;
    protected final ObservableSupplierImpl<EphemeralTabCoordinator>
            mEphemeralTabCoordinatorSupplier;
    @Nullable protected final BackPressManager mBackPressManager;
    private final boolean mIsIncognitoReauthPendingOnRestore;
    protected final ExpandedSheetHelper mExpandedBottomSheetHelper;
    protected @Nullable EdgeToEdgeControllerCreator mEdgeToEdgeControllerCreator;
    protected final BottomControlsStacker mBottomControlsStacker;
    protected final TopControlsStacker mTopControlsStacker;
    @NonNull protected final ObservableSupplier<Integer> mOverviewColorSupplier;
    @Nullable private ContextualSearchObserver mReadAloudContextualSearchObserver;
    @Nullable private PageZoomBarCoordinator mPageZoomBarCoordinator;
    @Nullable private ReaderModeBottomSheetManager mReaderModeBottomSheetManager;
    private AppMenuObserver mAppMenuObserver;
    private @Nullable LinkHoverStatusBarCoordinator mLinkHoverStatusBarCoordinator;

    private final OneshotSupplierImpl<ToolbarManager> mToolbarManagerOneshotSupplier =
            new OneshotSupplierImpl<>();
    private ActivityRecreationController mActivityRecreationController;
    private RestoreTabsFeatureHelper mRestoreTabsFeatureHelper;
    private @Nullable EdgeToEdgeController mEdgeToEdgeController;
    private ComposedBrowserControlsVisibilityDelegate mAppBrowserControlsVisibilityDelegate;
    private @Nullable BoardingPassController mBoardingPassController;
    protected final @NonNull EdgeToEdgeManager mEdgeToEdgeManager;
    private AutomotiveBackButtonToolbarCoordinator mAutomotiveBackButtonToolbarCoordinator;
    protected AdaptiveToolbarUiCoordinator mAdaptiveToolbarUiCoordinator;
    private final @Nullable ObservableSupplier<Boolean> mXrSpaceModeObservableSupplier;
    private final boolean mIsTablet;
    private final ObservableSupplierImpl<TopInsetCoordinator> mTopInsetCoordinatorSupplier;
    private @Nullable ToolbarControlContainer mToolbarContainer;
    private @Nullable DesktopWindowStateManager mDesktopWindowStateManager;
    private @Nullable final ExclusiveAccessManager mExclusiveAccessManager;
    private final PageZoomManager mPageZoomManager;
    private @Nullable AppHeaderObserver mAppHeaderObserver;
    protected final ObservableSupplierImpl<ReaderModeIphController>
            mReaderModeIphControllerSupplier = new ObservableSupplierImpl<>();

    /**
     * Create a new {@link RootUiCoordinator} for the given activity.
     *
     * @param activity The activity whose UI the coordinator is responsible for.
     * @param onOmniboxFocusChangedListener callback to invoke when Omnibox focus changes.
     * @param shareDelegateSupplier Supplies the {@link ShareDelegate}.
     * @param tabProvider The {@link ActivityTabProvider} to get current tab of the activity.
     * @param profileSupplier Supplier of the currently applicable profile.
     * @param bookmarkModelSupplier Supplier of the bookmark bridge for the current profile.
     * @param tabBookmarkerSupplier Supplier of {@link TabBookmarker} for bookmarking a given tab.
     * @param tabModelSelectorSupplier Supplies the {@link TabModelSelector}.
     * @param tabSwitcherSupplier Supplier of the {@link TabSwitcher}.
     * @param incognitoTabSwitcherSupplier Supplier of the incognito {@link TabSwitcher}.
     * @param intentMetadataOneshotSupplier Supplier with information about the launching intent.
     * @param layoutStateProviderOneshotSupplier Supplier of the {@link LayoutStateProvider}.
     * @param browserControlsManager Manages the browser controls.
     * @param windowAndroid The current {@link WindowAndroid}.
     * @param chromeAndroidTaskSupplier Supplies an {@link ChromeAndroidTask}.
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
     * @param edgeToEdgeControllerSupplier Supplies an {@link EdgeToEdgeController}.
     * @param topInsetCoordinatorSupplier Suppliers an {@link TopInsetCoordinator}.
     * @param activityType The {@link ActivityType} for the activity.
     * @param isInOverviewModeSupplier Supplies whether the app is in overview mode.
     * @param appMenuDelegate The app menu delegate.
     * @param statusBarColorProvider Provides the status bar color.
     * @param intentRequestTracker Tracks intent requests.
     * @param ephemeralTabCoordinatorSupplier Supplies the {@link EphemeralTabCoordinator}.
     * @param initializeUiWithIncognitoColors Whether to initialize the UI with incognito colors.
     * @param backPressManager The {@link BackPressManager} handling back press.
     * @param savedInstanceState The saved bundle for the last recorded state.
     * @param overviewColorSupplier Notifies when the overview color changes.
     * @param edgeToEdgeManager Manages core edge-to-edge state and logic.
     * @param xrSpaceModeObservableSupplier Supplies current XR space mode status. True for XR full
     *     space mode, false otherwise.
     * @param desktopWindowStateManager Tracks whether in desktop windowing mode
     */
    public RootUiCoordinator(
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
            @NonNull OneshotSupplier<ToolbarIntentMetadata> intentMetadataOneshotSupplier,
            @NonNull OneshotSupplier<LayoutStateProvider> layoutStateProviderOneshotSupplier,
            @NonNull BrowserControlsManager browserControlsManager,
            @NonNull ActivityWindowAndroid windowAndroid,
            @NonNull OneshotSupplier<ChromeAndroidTask> chromeAndroidTaskSupplier,
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
            @NonNull ObservableSupplierImpl<EdgeToEdgeController> edgeToEdgeControllerSupplier,
            @NonNull ObservableSupplierImpl<TopInsetCoordinator> topInsetCoordinatorSupplier,
            @ActivityType int activityType,
            @NonNull Supplier<Boolean> isInOverviewModeSupplier,
            @NonNull AppMenuDelegate appMenuDelegate,
            @NonNull StatusBarColorProvider statusBarColorProvider,
            @NonNull IntentRequestTracker intentRequestTracker,
            @NonNull
                    ObservableSupplierImpl<EphemeralTabCoordinator> ephemeralTabCoordinatorSupplier,
            boolean initializeUiWithIncognitoColors,
            @Nullable BackPressManager backPressManager,
            @Nullable Bundle savedInstanceState,
            @NonNull ObservableSupplier<Integer> overviewColorSupplier,
            @NonNull EdgeToEdgeManager edgeToEdgeManager,
            @Nullable ObservableSupplier<Boolean> xrSpaceModeObservableSupplier,
            @Nullable DesktopWindowStateManager desktopWindowStateManager) {
        mCallbackController = new CallbackController();
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mChromeAndroidTaskSupplier = chromeAndroidTaskSupplier;
        setupUnownedUserDataSuppliers();
        mOnOmniboxFocusChangedListener = onOmniboxFocusChangedListener;
        mBrowserControlsManager = browserControlsManager;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);
        mAppMenuBlocker = appMenuBlocker;
        mSupportsAppMenuSupplier = supportsAppMenuSupplier;
        mSupportsFindInPageSupplier = supportsFindInPage;
        mTabCreatorManagerSupplier = tabCreatorManagerSupplier;
        mFullscreenManager = fullscreenManager;
        mCompositorViewHolderSupplier = compositorViewHolderSupplier;
        mTabContentManagerSupplier = tabContentManagerSupplier;
        mSnackbarManagerSupplier = snackbarManagerSupplier;
        mEdgeToEdgeControllerSupplier = edgeToEdgeControllerSupplier;
        mTopInsetCoordinatorSupplier = topInsetCoordinatorSupplier;
        mActivityType = activityType;
        mIsInOverviewModeSupplier = isInOverviewModeSupplier;
        mAppMenuDelegate = appMenuDelegate;
        mIntentRequestTracker = intentRequestTracker;
        mInitializeUiWithIncognitoColors = initializeUiWithIncognitoColors;
        mBackPressManager = backPressManager;
        mIsIncognitoReauthPendingOnRestore =
                savedInstanceState != null
                        && savedInstanceState.getBoolean(
                                IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING,
                                false);

        mMenuOrKeyboardActionController = menuOrKeyboardActionController;
        mMenuOrKeyboardActionController.registerMenuOrKeyboardActionHandler(this);
        mActivityTabProvider = tabProvider;

        // This little bit of arithmetic is necessary because of Java doesn't like accepting
        // Supplier<BaseImpl> where Supplier<Base> is expected. We should remove the need for
        // LayoutManagerImpl in this class so we can simply use Supplier<LayoutManager>.
        mLayoutManagerSupplier = new ObservableSupplierImpl<>();
        Callback<LayoutManagerImpl> mLayoutManagerSupplierCallback =
                (layoutManager) -> {
                    onLayoutManagerAvailable(layoutManager);
                    mLayoutManagerSupplier.set(layoutManager);
                };
        mLayoutManagerImplSupplier = layoutManagerSupplier;
        mLayoutManagerImplSupplier.addObserver(mLayoutManagerSupplierCallback);
        mTabStripVisibilitySupplier =
                mLayoutManagerImplSupplier.createTransitive(
                        layoutManagerImpl -> {
                            StripLayoutHelperManager stripLayoutHelperManager =
                                    layoutManagerImpl.getStripLayoutHelperManager();
                            return stripLayoutHelperManager != null
                                    ? stripLayoutHelperManager.getStripVisibilityStateSupplier()
                                    : null;
                        });

        mShareDelegateSupplier = shareDelegateSupplier;
        mTabObscuringHandlerSupplier.set(new TabObscuringHandler());
        mDeviceLockActivityLauncherSupplier.set(DeviceLockActivityLauncherImpl.get());
        new AccessibilityVisibilityHandler(
                mActivityLifecycleDispatcher,
                mActivityTabProvider,
                mTabObscuringHandlerSupplier.get());
        new AutofillSessionLifetimeController(
                activity, mActivityLifecycleDispatcher, mActivityTabProvider);
        mProfileSupplier = profileSupplier;
        mBookmarkModelSupplier = bookmarkModelSupplier;
        mTabBookmarkerSupplier = tabBookmarkerSupplier;
        mAppMenuSupplier = new OneshotSupplierImpl<>();
        mActionModeControllerCallback = new ToolbarActionModeCallback();

        mTabModelSelectorSupplier = tabModelSelectorSupplier;

        mOmniboxFocusStateSupplier.set(false);

        mLayoutStateProviderOneShotSupplier = layoutStateProviderOneshotSupplier;
        mLayoutStateProviderOneShotSupplier.onAvailable(
                mCallbackController.makeCancelable(this::setLayoutStateProvider));

        mTabSwitcherSupplier = tabSwitcherSupplier;
        mIncognitoTabSwitcherSupplier = incognitoTabSwitcherSupplier;
        mIntentMetadataOneshotSupplier = intentMetadataOneshotSupplier;
        mOverviewColorSupplier = overviewColorSupplier;

        mIsTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity);
        mTopUiThemeColorProvider =
                new TopUiThemeColorProvider(
                        mActivity,
                        mActivityTabProvider,
                        activityThemeColorSupplier,
                        mIsTablet,
                        shouldAllowThemingInNightMode(),
                        shouldAllowBrightThemeColors(),
                        shouldAllowThemingOnTablets());
        if (NtpCustomizationUtils.canEnableEdgeToEdgeForCustomizedTheme(mIsTablet)) {
            mAdjustedTopUiThemeColorProvider =
                    new AdjustedTopUiThemeColorProvider(
                            mActivity,
                            mActivityTabProvider,
                            activityThemeColorSupplier,
                            mIsTablet,
                            shouldAllowThemingInNightMode(),
                            shouldAllowBrightThemeColors(),
                            shouldAllowThemingOnTablets());
        }

        mDesktopWindowStateManager = desktopWindowStateManager;
        mStatusBarColorController =
                new StatusBarColorController(
                        mActivity,
                        DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity),
                        statusBarColorProvider,
                        mLayoutManagerSupplier,
                        mActivityLifecycleDispatcher,
                        mActivityTabProvider,
                        mTopUiThemeColorProvider,
                        edgeToEdgeManager.getEdgeToEdgeSystemBarColorHelper(),
                        mDesktopWindowStateManager,
                        mOverviewColorSupplier);
        mEphemeralTabCoordinatorSupplier = ephemeralTabCoordinatorSupplier;
        mPageZoomManager =
                new PageZoomManager(
                        new PageZoomManagerDelegate() {
                            @Override
                            public WebContents getWebContents() {
                                if (mActivityTabProvider.get() == null) {
                                    return null;
                                }
                                return mActivityTabProvider.get().getWebContents();
                            }

                            @Override
                            public BrowserContextHandle getBrowserContextHandle() {
                                return mProfileSupplier.get().getOriginalProfile();
                            }

                            @Override
                            public void addZoomEventsObserver(ZoomEventsObserver observer) {
                                HostZoomListenerFactory.getForProfile(
                                                mProfileSupplier.get().getOriginalProfile())
                                        .addObserver(observer);
                            }

                            @Override
                            public void removeZoomEventsObserver(ZoomEventsObserver observer) {
                                // HostZoomListenerFactory stores each HostZoomListener object in a
                                // ProfileKeyedMap. When the profile is destroyed, each
                                // HostZoomFactory will be destroyed, removing all observers stored.
                                if (mProfileSupplier.get() != null) {
                                    HostZoomListenerFactory.getForProfile(
                                                    mProfileSupplier.get().getOriginalProfile())
                                            .removeObserver(observer);
                                }
                            }

                            @Override
                            public void enterImmersiveMode() {
                                DisplayAndroid display =
                                        DisplayAndroid.getNonMultiDisplay(mActivity);
                                mFullscreenManager.onEnterFullscreen(
                                        mActivityTabProvider.get(),
                                        new FullscreenOptions(
                                                /* showNavigationBar= */ false,
                                                /* showStatusBar= */ false,
                                                /* displayId= */ display.getDisplayId()));
                                mAppMenuCoordinator.getAppMenuHandler().hideAppMenu();
                            }

                            @Override
                            public boolean isCurrentTabNull() {
                                return mActivityTabProvider.get() == null;
                            }
                        });

        mPageZoomBarCoordinator =
                new PageZoomBarCoordinator(
                        new PageZoomBarCoordinatorDelegate() {
                            @Override
                            public View getZoomControlView() {
                                ViewStub viewStub =
                                        mActivity.findViewById(R.id.page_zoom_container);
                                return viewStub.inflate();
                            }
                        },
                        mPageZoomManager,
                        /* useSlider= */ ChromeFeatureList.sAndroidSettingsContainment.isEnabled());

        if (ChromeFeatureList.sEnableExclusiveAccessManager.isEnabled()) {
            mExclusiveAccessManager =
                    new ExclusiveAccessManager(
                            mFullscreenManager,
                            mDesktopWindowStateManager);
            mBackPressManager.addHandler(
                    new ExclusiveAccessManagerBackPressHandler(mExclusiveAccessManager),
                    BackPressHandler.Type.FULLSCREEN);
            // Fullscreen manager state, not actual window state, has to be recreated as soon after
            // RootUiCoordinator creations as possible. It is needed to keep renderer in the
            // fullscreen state if recreation was caused by the window move to another display
            // during full screen to another screen call
            mExclusiveAccessManager.setFullscreenPendingState(savedInstanceState);
        } else {
            mExclusiveAccessManager = null;
            mBackPressManager.addHandler(
                    new FullscreenBackPressHandler(mBrowserControlsManager.getFullscreenManager()),
                    BackPressHandler.Type.FULLSCREEN);
        }

        mActivityRecreationController =
                new ActivityRecreationController(
                        mToolbarManagerOneshotSupplier,
                        mLayoutManagerSupplier,
                        mActivityTabProvider,
                        new Handler(),
                        mExclusiveAccessManager);

        mExpandedBottomSheetHelper =
                new ExpandedSheetHelperImpl(mModalDialogManagerSupplier, getTabObscuringHandler());
        mEdgeToEdgeManager = edgeToEdgeManager;
        mBottomControlsStacker =
                new BottomControlsStacker(mBrowserControlsManager, mActivity, mWindowAndroid);
        mTopControlsStacker = new TopControlsStacker(mBrowserControlsManager);
        mXrSpaceModeObservableSupplier = xrSpaceModeObservableSupplier;

        if (BrowserControlsUtils.doSyncMinHeightWithTotalHeightV2()) {
            if (DeviceInfo.isDesktop()
                    || BrowserControlsUtils.doSyncMinHeightWithTotalHeight(mActivity)) {
                mTopControlsStacker.setScrollingDisabled(true);
            } else if (mDesktopWindowStateManager != null) {
                mAppHeaderObserver =
                        new AppHeaderObserver() {
                            @Override
                            public void onDesktopWindowingModeChanged(boolean isInDesktopWindow) {
                                mTopControlsStacker.setScrollingDisabled(isInDesktopWindow);
                            }
                        };
                mDesktopWindowStateManager.addObserver(mAppHeaderObserver);
                var appHeaderState = mDesktopWindowStateManager.getAppHeaderState();
                if (appHeaderState != null) {
                    mAppHeaderObserver.onDesktopWindowingModeChanged(
                            appHeaderState.isInDesktopWindow());
                }
            }
        }
    }

    // TODO(pnoland, crbug.com/865801): remove this in favor of wiring it directly.
    public ToolbarManager getToolbarManager() {
        return mToolbarManager;
    }

    public StatusBarColorController getStatusBarColorController() {
        return mStatusBarColorController;
    }

    // TODO(jinsukkim): remove this in favor of wiring it directly.
    /**
     * @return {@link ThemeColorProvider} for top UI.
     */
    public TopUiThemeColorProvider getTopUiThemeColorProvider() {
        return mTopUiThemeColorProvider;
    }

    /**
     * @return The {@link DesktopWindowStateManager} instance associated with the current activity.
     */
    public @Nullable DesktopWindowStateManager getDesktopWindowStateManager() {
        return mDesktopWindowStateManager;
    }

    public void onAttachFragment(Fragment fragment) {
        if (fragment instanceof QrCodeDialog) {
            QrCodeDialog qrCodeDialog = (QrCodeDialog) fragment;
            qrCodeDialog.setWindowAndroid(mWindowAndroid);
        }
    }

    @Override
    public void onDestroy() {
        // TODO(meiliang): Understand why we need to set most of the class member instances to null
        //  other than the mActivity. If the nulling calls are not necessary, we can remove them.
        mCallbackController.destroy();
        mMenuOrKeyboardActionController.unregisterMenuOrKeyboardActionHandler(this);

        destroyUnownedUserDataSuppliers();
        mActivityLifecycleDispatcher.unregister(this);

        if (mExclusiveAccessManager != null) {
            mExclusiveAccessManager.destroy();
        }

        if (mMessageDispatcher != null) {
            mMessageDispatcher.dismissAllMessages(DismissReason.ACTIVITY_DESTROYED);
            MessagesFactory.detachMessageDispatcher(mMessageDispatcher);
            mMessageDispatcher = null;
        }

        if (mMessageQueueMediator != null) {
            mMessageQueueMediator.destroy();
            mMessageQueueMediator = null;
        }

        if (mMessageContainerCoordinator != null) {
            if (mMessageContainerObserver != null) {
                mMessageContainerCoordinator.removeObserver(mMessageContainerObserver);
            }
            mMessageContainerCoordinator.destroy();
            mMessageContainerCoordinator = null;
        }

        if (mOverlayPanelManager != null) {
            mOverlayPanelManager.removeObserver(mOverlayPanelManagerObserver);
        }

        if (mLayoutStateProvider != null) {
            mLayoutStateProvider.removeObserver(mLayoutStateObserver);
            mLayoutStateProvider = null;
        }

        if (mToolbarManager != null) {
            if (mMicStateObserver != null && mToolbarManager.getVoiceRecognitionHandler() != null) {
                mToolbarManager.getVoiceRecognitionHandler().removeObserver(mMicStateObserver);
            }
            mToolbarManager.destroy();
            mToolbarManager = null;
        }

        if (mAdaptiveToolbarUiCoordinator != null) {
            mAdaptiveToolbarUiCoordinator.destroy();
            mAdaptiveToolbarUiCoordinator = null;
        }

        if (mAppMenuCoordinator != null) {
            mAppMenuCoordinator.unregisterAppMenuBlocker(this);
            mAppMenuCoordinator.unregisterAppMenuBlocker(mAppMenuBlocker);

            if (mAppMenuObserver != null) {
                mAppMenuCoordinator.getAppMenuHandler().removeObserver(mAppMenuObserver);
            }
            mAppMenuCoordinator.destroy();
        }

        if (mTopUiThemeColorProvider != null) {
            mTopUiThemeColorProvider.destroy();
            mTopUiThemeColorProvider = null;
        }

        if (mAdjustedTopUiThemeColorProvider != null) {
            mAdjustedTopUiThemeColorProvider.destroy();
            mAdjustedTopUiThemeColorProvider = null;
        }

        if (mFindToolbarManager != null) mFindToolbarManager.removeObserver(mFindToolbarObserver);

        var modalDialogManager = mModalDialogManagerSupplier.get();
        if (mModalDialogManagerObserver != null && modalDialogManager != null) {
            modalDialogManager.removeObserver(mModalDialogManagerObserver);
        }

        if (mBottomSheetManager != null) mBottomSheetManager.onDestroy();
        if (mBottomSheetController != null) {
            if (mBottomSheetObserver != null) {
                mBottomSheetController.removeObserver(mBottomSheetObserver);
            }
            BottomSheetControllerFactory.detach(mBottomSheetController);
            mBottomSheetController.destroy();
        }

        if (mScrimManager != null) mScrimManager.destroy();
        mScrimManager = null;

        if (mCaptureController != null) {
            mCaptureController.destroy();
            mCaptureController = null;
        }

        var merchantTrustSignalsCoordinator = mMerchantTrustSignalsCoordinatorSupplier.get();
        if (merchantTrustSignalsCoordinator != null) {
            merchantTrustSignalsCoordinator.destroy();
            mMerchantTrustSignalsCoordinatorSupplier.set(null);
        }

        if (mScrollCaptureManager != null) {
            mScrollCaptureManager.destroy();
            mScrollCaptureManager = null;
        }

        if (mIncognitoReauthController != null) {
            mIncognitoReauthController.destroy();
        }

        if (mPageZoomBarCoordinator != null) {
            mPageZoomBarCoordinator.destroy();
            mPageZoomBarCoordinator = null;
        }

        if (mBrowserControlsObserver != null) {
            mBrowserControlsManager.removeObserver(mBrowserControlsObserver);
        }

        if (mActivityRecreationController != null) {
            mActivityRecreationController = null;
        }

        if (mRestoreTabsFeatureHelper != null) {
            mRestoreTabsFeatureHelper.destroy();
            mRestoreTabsFeatureHelper = null;
        }

        var readAloudController = mReadAloudControllerSupplier.get();
        if (readAloudController != null) {
            ContextualSearchManager contextualSearchManager =
                    mContextualSearchManagerSupplier.get();
            if (contextualSearchManager != null) {
                contextualSearchManager.removeObserver(mReadAloudContextualSearchObserver);
            }
            mReadAloudControllerSupplier.set(null);
            readAloudController.destroy();
        }

        var manager = mContextualSearchManagerSupplier.get();
        if (manager != null) {
            manager.destroy();
            mContextualSearchManagerSupplier.set(null);
        }

        if (mEdgeToEdgeController != null) {
            mEdgeToEdgeController.destroy();
            mEdgeToEdgeController = null;
        }
        mEdgeToEdgeControllerSupplier.set(null);

        if (mEdgeToEdgeControllerCreator != null) {
            mEdgeToEdgeControllerCreator.destroy();
            mEdgeToEdgeControllerCreator = null;
        }

        if (mEdgeToEdgeBottomChin != null) {
            mEdgeToEdgeBottomChin.destroy();
        }

        var topInsetCoordinator = mTopInsetCoordinatorSupplier.get();
        if (topInsetCoordinator != null) {
            topInsetCoordinator.destroy();
        }

        if (mBoardingPassController != null) {
            mBoardingPassController.destroy();
            mBoardingPassController = null;
        }

        if (mReaderModeBottomSheetManager != null) {
            mReaderModeBottomSheetManager.destroy();
            mReaderModeBottomSheetManager = null;
        }

        if (mLinkHoverStatusBarCoordinator != null) {
            mLinkHoverStatusBarCoordinator.destroy();
            mLinkHoverStatusBarCoordinator = null;
        }

        if (mAutomotiveBackButtonToolbarCoordinator != null) {
            mAutomotiveBackButtonToolbarCoordinator.destroy();
            mAutomotiveBackButtonToolbarCoordinator = null;
        }
        mBottomControlsStacker.destroy();
        mTopControlsStacker.destroy();

        if (mDesktopWindowStateManager != null) {
            if (mAppHeaderObserver != null) {
                mDesktopWindowStateManager.removeObserver(mAppHeaderObserver);
                mAppHeaderObserver = null;
            }
            mDesktopWindowStateManager.destroy();
            mDesktopWindowStateManager = null;
        }

        mActivity = null;
    }

    private void setupUnownedUserDataSuppliers() {
        var userDataHost = mWindowAndroid.getUnownedUserDataHost();
        mTabObscuringHandlerSupplier.attach(userDataHost);
        mDeviceLockActivityLauncherSupplier.attach(userDataHost);
        mContextualSearchManagerSupplier.attach(userDataHost);
        mReadAloudControllerSupplier.attach(userDataHost);
    }

    private void destroyUnownedUserDataSuppliers() {
        // TabObscuringHandler doesn't have a destroy method.
        mTabObscuringHandlerSupplier.destroy();
        mDeviceLockActivityLauncherSupplier.destroy();
        mContextualSearchManagerSupplier.destroy();
        mReadAloudControllerSupplier.destroy();
    }

    @Override
    public void onPreInflationStartup() {
        initializeBottomSheetController();
    }

    @Override
    public void onInflationComplete() {
        mScrimManager = buildScrimWidget();
        mScrimManagerSupplier.set(mScrimManager);
        initFindToolbarManager();
        initializeToolbar();
    }

    @Override
    public void onPostInflationStartup() {
        initAppMenu();
        initBottomSheetObserver();
        initSnackbarObserver();
        initBrowserControlsObserver();
        var modalDialogManager = mModalDialogManagerSupplier.get();
        if (mAppMenuCoordinator != null && modalDialogManager != null) {
            mModalDialogManagerObserver =
                    new ModalDialogManagerObserver() {
                        @Override
                        public void onDialogAdded(PropertyModel model) {
                            mAppMenuCoordinator.getAppMenuHandler().hideAppMenu();
                        }
                    };
            modalDialogManager.addObserver(mModalDialogManagerObserver);
        }
        new ChromeActionModeHandler(
                mActivityTabProvider,
                (searchText) -> {
                    if (mTabModelSelectorSupplier.get() == null) return;

                    String query =
                            ActionModeCallbackHelper.sanitizeQuery(
                                    searchText, ActionModeCallbackHelper.MAX_SEARCH_QUERY_LENGTH);
                    if (TextUtils.isEmpty(query)) return;

                    Tab tab = mActivityTabProvider.get();
                    TrackerFactory.getTrackerForProfile(tab.getProfile())
                            .notifyEvent(EventConstants.WEB_SEARCH_PERFORMED);

                    mTabModelSelectorSupplier
                            .get()
                            .openNewTab(
                                    generateUrlParamsForSearch(tab, query),
                                    TabLaunchType.FROM_LONGPRESS_FOREGROUND,
                                    tab,
                                    tab.isIncognito());
                },
                showWebSearchInActionMode(),
                mShareDelegateSupplier,
                mBrowserControlsManager,
                mReadAloudControllerSupplier);

        mCaptureController =
                new MediaCaptureOverlayController(
                        mWindowAndroid, mActivity.findViewById(R.id.capture_overlay));

        // Ensure the bottom sheet's container has been laid out at least once before hiding it.
        // TODO(crbug.com/40759801): This should be owned by the BottomSheetControllerImpl, but
        // there are some
        //                complexities around the order of events resulting from waiting for layout.
        ViewGroup sheetContainer = mActivity.findViewById(R.id.sheet_container);
        if (!sheetContainer.isLaidOut()) {
            sheetContainer.addOnLayoutChangeListener(
                    new View.OnLayoutChangeListener() {
                        @Override
                        public void onLayoutChange(
                                View view,
                                int left,
                                int top,
                                int right,
                                int bottom,
                                int oldLeft,
                                int oldTop,
                                int oldRight,
                                int oldBottom) {
                            sheetContainer.setVisibility(View.GONE);
                            sheetContainer.removeOnLayoutChangeListener(this);
                        }
                    });
        } else {
            sheetContainer.setVisibility(View.GONE);
        }
    }

    protected boolean showWebSearchInActionMode() {
        return true;
    }

    @Override
    @CallSuper
    public void onFinishNativeInitialization() {
        Profile profile = mProfileSupplier.get();
        if (profile != null) {
            initProfileDependentFeatures(profile);
        } else {
            new OneShotCallback<>(
                    mProfileSupplier,
                    mCallbackController.makeCancelable(this::initProfileDependentFeatures));
        }

        if (ChromeFeatureList.sEnableExclusiveAccessManager.isEnabled()) {
            mExclusiveAccessManager.initialize(
                    mTabModelSelectorSupplier.get(), mActivity, mActivityTabProvider);
        }

        initMessagesInfra();
        initScrollCapture();
        mAdaptiveToolbarUiCoordinator.onFinishNativeInitialization();

        if (mWindowAndroid.getInsetObserver() != null
                && Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
                && ChromeFeatureList.sEdgeToEdgeMonitorConfigurations.isEnabled()) {
            mEdgeToEdgeControllerCreator =
                    new EdgeToEdgeControllerCreator(
                            new WeakReference<Activity>(mActivity),
                            mWindowAndroid.getInsetObserver(),
                            this::initializeEdgeToEdgeController);
        } else {
            initializeEdgeToEdgeController();
        }
        initBoardingPassDetector();

        if (EphemeralTabCoordinator.isSupported()) {
            Supplier<TabCreator> tabCreator =
                    () ->
                            mTabCreatorManagerSupplier
                                    .get()
                                    .getTabCreator(
                                            mTabModelSelectorSupplier.get().isIncognitoSelected());
            mEphemeralTabCoordinatorSupplier.set(
                    new EphemeralTabCoordinator(
                            mActivity,
                            mWindowAndroid,
                            mActivity.getWindow().getDecorView(),
                            mActivityTabProvider,
                            tabCreator,
                            getBottomSheetController()));
        }
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.READALOUD)) {
            TabModelSelector tabModelSelector = mTabModelSelectorSupplier.get();
            ReadAloudController controller =
                    new ReadAloudController(
                            mActivity,
                            mProfileSupplier,
                            tabModelSelector.getModel(/* incognito= */ false),
                            tabModelSelector.getModel(/* incognito= */ true),
                            getBottomSheetController(),
                            mBottomControlsStacker,
                            mLayoutManagerSupplier,
                            mWindowAndroid,
                            mActivityLifecycleDispatcher,
                            mLayoutStateProviderOneShotSupplier,
                            mFullscreenManager);
            mReadAloudControllerSupplier.set(controller);
            mReadAloudContextualSearchObserver =
                    new ContextualSearchObserver() {
                        @Override
                        public void onShowContextualSearch() {
                            controller.maybeHidePlayer();
                        }

                        @Override
                        public void onHideContextualSearch() {
                            controller.maybeShowPlayer();
                        }
                    };
            ContextualSearchManager contextualSearchManager =
                    mContextualSearchManagerSupplier.get();
            if (contextualSearchManager != null) {
                contextualSearchManager.addObserver(mReadAloudContextualSearchObserver);
            }
        }
        if (DomDistillerFeatures.sReaderModeDistillInApp.isEnabled()) {
            mReaderModeBottomSheetManager =
                    new ReaderModeBottomSheetManager(
                            mActivity,
                            getBottomSheetController(),
                            mActivityTabProvider,
                            mBrowserControlsManager,
                            mTopUiThemeColorProvider);
        }

        if (DeviceInfo.isAutomotive()) {
            mAutomotiveBackButtonToolbarCoordinator =
                    new AutomotiveBackButtonToolbarCoordinator(
                            mActivity,
                            mActivity.findViewById(R.id.automotive_base_frame_layout),
                            mFullscreenManager,
                            mCompositorViewHolderSupplier.get(),
                            mBackPressManager);
        }

        if (mWindowAndroid.getInsetObserver() != null
                && NtpCustomizationUtils.canEnableEdgeToEdgeForCustomizedTheme(
                        mWindowAndroid, mIsTablet)) {
            var topInsetCoordinator =
                    new TopInsetCoordinator(
                            mActivity,
                            mActivityTabProvider,
                            mWindowAndroid.getInsetObserver(),
                            mLayoutStateProviderOneShotSupplier);
            mTopInsetCoordinatorSupplier.set(topInsetCoordinator);
        }
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.LINK_HOVER_STATUS_BAR)) {
            ViewStub statusBarStub = mActivity.findViewById(R.id.link_hover_status_bar_stub);
            mLinkHoverStatusBarCoordinator =
                    new LinkHoverStatusBarCoordinator(
                            mActivity, mActivityTabProvider, statusBarStub);
        }
    }

    protected boolean isContextualSearchEnabled() {
        return true;
    }

    public void createContextualSearchManager(Profile profile) {
        if (!isContextualSearchEnabled()) return;

        mContextualSearchManagerSupplier.set(
                new ContextualSearchManager(
                        mActivity,
                        profile,
                        this,
                        mScrimManager,
                        mActivityTabProvider,
                        mFullscreenManager,
                        mBrowserControlsManager,
                        mWindowAndroid,
                        mTabModelSelectorSupplier.get(),
                        mEdgeToEdgeControllerSupplier));
    }

    public void initContextualSearchManager() {
        var manager = mContextualSearchManagerSupplier.get();
        if (manager == null) return;

        int controlContainerHeightId = getControlContainerHeightResource();
        float toolbarHeightDp =
                controlContainerHeightId == ActivityUtils.NO_RESOURCE_ID
                        ? 0f
                        : mActivity.getResources().getDimension(controlContainerHeightId);
        manager.initialize(
                mActivity.findViewById(android.R.id.content),
                mLayoutManager,
                getBottomSheetController(),
                mCompositorViewHolderSupplier.get(),
                toolbarHeightDp,
                mToolbarManager,
                canContextualSearchPromoteToNewTab(),
                mIntentRequestTracker,
                getDesktopWindowStateManager(),
                mBottomControlsStacker);
    }

    public ObservableSupplier<ContextualSearchManager> getContextualSearchManagerSupplier() {
        return mContextualSearchManagerSupplier;
    }

    /** Whether contextual search panel is opened. */
    public boolean isContextualSearchOpened() {
        var manager = mContextualSearchManagerSupplier.get();
        return manager != null && manager.isSearchPanelOpened();
    }

    /** Hide contextual search panel. */
    public void hideContextualSearch() {
        var manager = mContextualSearchManagerSupplier.get();
        if (manager != null) {
            mContextualSearchManagerSupplier
                    .get()
                    .hideContextualSearch(OverlayPanel.StateChangeReason.UNKNOWN);
        }
    }

    /**
     * @return The resource id that contains how large the browser controls are.
     */
    public int getControlContainerHeightResource() {
        return ActivityUtils.NO_RESOURCE_ID;
    }

    protected boolean canContextualSearchPromoteToNewTab() {
        return false;
    }

    @Override
    public void createContextualSearchTab(String searchUrl) {
        Tab currentTab = mActivityTabProvider.get();
        if (currentTab == null) return;

        TabCreator tabCreator =
                mTabCreatorManagerSupplier.get().getTabCreator(currentTab.isIncognito());
        if (tabCreator == null) return;

        tabCreator.createNewTab(
                new LoadUrlParams(searchUrl, PageTransition.LINK),
                TabLaunchType.FROM_LINK,
                mActivityTabProvider.get());
    }

    /** Handle post native initialization of features that require the Profile to be available. */
    @CallSuper
    protected void initProfileDependentFeatures(Profile currentlySelectedProfile) {
        Profile originalProfile = currentlySelectedProfile.getOriginalProfile();

        // Setup IncognitoReauthController as early as possible, to show the re-auth screen.
        if (IncognitoReauthManager.isIncognitoReauthFeatureAvailable()) {
            initIncognitoReauthController(originalProfile);
        }

        if (DeviceFormFactor.isWindowOnTablet(mWindowAndroid)
                && DesktopSiteUtils.maybeDefaultEnableGlobalSetting(
                        getPrimaryDisplaySizeInInches(), originalProfile, mActivity)) {
            // TODO(crbug.com/40856393): Remove this explicit load when this bug is addressed.
            if (mActivityTabProvider != null && mActivityTabProvider.get() != null) {
                mActivityTabProvider
                        .get()
                        .loadIfNeeded(TabLoadIfNeededCaller.ON_FINISH_NATIVE_INITIALIZATION);
            }
        }

        DesktopSiteUtils.maybeDefaultEnableWindowSetting(mActivity, originalProfile);

        initMerchantTrustSignals(originalProfile);
    }

    private void initMessagesInfra() {
        // TODO(crbug.com/40753426): Move feature flag and parameters into a separate class in
        MessagesResourceMapperInitializer.init();
        MessageContainer container = mActivity.findViewById(R.id.message_container);
        mMessageContainerCoordinator =
                new MessageContainerCoordinator(container, mBrowserControlsManager);
        mMessageContainerObserver =
                new MessageContainerObserver() {
                    @Override
                    public void onShowMessageContainer() {
                        if (mPageZoomBarCoordinator != null) {
                            mPageZoomBarCoordinator.hide();
                        }
                    }

                    @Override
                    public void onHideMessageContainer() {}
                };
        mMessageContainerCoordinator.addObserver(mMessageContainerObserver);
        mMessageDispatcher =
                MessagesFactory.createMessageDispatcher(
                        container,
                        mMessageContainerCoordinator::getMessageTopOffset,
                        mMessageContainerCoordinator::getMessageMaxTranslation,
                        new ChromeMessageAutodismissDurationProvider(),
                        mWindowAndroid::startAnimationOverContent,
                        mWindowAndroid);
        mMessageQueueMediator =
                new ChromeMessageQueueMediator(
                        mBrowserControlsManager,
                        mMessageContainerCoordinator,
                        mActivityTabProvider,
                        mLayoutStateProviderOneShotSupplier,
                        mModalDialogManagerSupplier,
                        getBottomSheetController(),
                        mActivityLifecycleDispatcher,
                        mMessageDispatcher);
        mMessageDispatcher.setDelegate(mMessageQueueMediator);
        MessagesFactory.attachMessageDispatcher(mWindowAndroid, mMessageDispatcher);
    }

    private void initIncognitoReauthController(Profile profile) {
        IncognitoReauthCoordinatorFactory incognitoReauthCoordinatorFactory =
                getIncognitoReauthCoordinatorFactory(profile);
        assert incognitoReauthCoordinatorFactory != null
                : "Sub-classes need to provide a valid factory instance.";
        mIncognitoReauthController =
                new IncognitoReauthControllerImpl(
                        mTabModelSelectorSupplier.get(),
                        mActivityLifecycleDispatcher,
                        mLayoutStateProviderOneShotSupplier,
                        mProfileSupplier,
                        incognitoReauthCoordinatorFactory,
                        () -> mIsIncognitoReauthPendingOnRestore,
                        mActivity.getTaskId());
        mIncognitoReauthControllerOneshotSupplier.set(mIncognitoReauthController);
    }

    /**
     * @return The primary display size of the device, in inches.
     */
    public double getPrimaryDisplaySizeInInches() {
        DisplayAndroid display = DisplayAndroid.getNonMultiDisplay(mActivity);
        return DisplayUtil.getDisplaySizeInInches(display);
    }

    /**
     * This method is meant to be overridden for sub-classes which needs to provide an incognito
     * re-auth view.
     *
     * @return {@link IncognitoReauthCoordiantorFactory} instance.
     */
    protected IncognitoReauthCoordinatorFactory getIncognitoReauthCoordinatorFactory(
            Profile profile) {
        return null;
    }

    private void initMerchantTrustSignals(Profile profile) {
        if (ShoppingServiceFactory.getForProfile(profile).isMerchantViewerEnabled()
                && shouldInitializeMerchantTrustSignals()) {
            MerchantTrustSignalsCoordinator merchantTrustSignalsCoordinator =
                    new MerchantTrustSignalsCoordinator(
                            mActivity,
                            mWindowAndroid,
                            getBottomSheetController(),
                            mActivity.getWindow().getDecorView(),
                            MessageDispatcherProvider.from(mWindowAndroid),
                            mActivityTabProvider,
                            mProfileSupplier,
                            new MerchantTrustMetrics(),
                            mIntentRequestTracker);
            mMerchantTrustSignalsCoordinatorSupplier.set(merchantTrustSignalsCoordinator);
        }
    }

    private void initScrollCapture() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) return;

        mScrollCaptureManager = new ScrollCaptureManager(mActivityTabProvider);
    }

    /**
     * @return Whether the {@link MerchantTrustSignalsCoordinator} should be initialized in the
     *     context of this coordinator's UI.
     */
    protected boolean shouldInitializeMerchantTrustSignals() {
        return false;
    }

    /** Returns the supplier of {@link MerchantTrustSignalsCoordinator}. */
    @NonNull
    public Supplier<MerchantTrustSignalsCoordinator> getMerchantTrustSignalsCoordinatorSupplier() {
        return mMerchantTrustSignalsCoordinatorSupplier;
    }

    /** Generate the LoadUrlParams necessary to load the specified search query. */
    private static LoadUrlParams generateUrlParamsForSearch(Tab tab, String query) {
        Profile profile = tab.getProfile();
        TemplateUrlService service = TemplateUrlServiceFactory.getForProfile(profile);
        String url = service.getUrlForSearchQuery(query);
        String headers = GeolocationHeader.getGeoHeader(url, profile, service);

        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        loadUrlParams.setVerbatimHeaders(headers);
        loadUrlParams.setTransitionType(PageTransition.GENERATED);
        return loadUrlParams;
    }

    /**
     * Triggered when the share menu item is selected.
     * This creates and shows a share intent picker dialog or starts a share intent directly.
     * @param shareDirectly Whether it should share directly with the activity that was most
     *                      recently used to share.
     * @param isIncognito Whether currentTab is incognito.
     */
    @VisibleForTesting
    public void onShareMenuItemSelected(final boolean shareDirectly, final boolean isIncognito) {
        ShareDelegate shareDelegate = mShareDelegateSupplier.get();
        Tab tab = mActivityTabProvider.get();

        if (shareDelegate == null || tab == null) return;

        if (shareDirectly) {
            RecordUserAction.record("MobileMenuDirectShare");
            new UkmRecorder(tab.getWebContents(), "MobileMenu.DirectShare")
                    .addBooleanMetric("HasOccurred")
                    .record();
        } else {
            RecordUserAction.record("MobileMenuShare");
            new UkmRecorder(tab.getWebContents(), "MobileMenu.Share")
                    .addBooleanMetric("HasOccurred")
                    .record();
        }
        shareDelegate.share(tab, shareDirectly, ShareOrigin.OVERFLOW_MENU);
    }

    // MenuOrKeyboardActionHandler implementation

    @Override
    public boolean handleMenuOrKeyboardAction(int id, boolean fromMenu) {
        if (id == R.id.show_menu && mAppMenuCoordinator != null) {
            mAppMenuCoordinator.showAppMenuForKeyboardEvent();
            return true;
        } else if (id == R.id.find_in_page_id) {
            Tab tab = mActivityTabProvider.get();
            // PDF pages require Android pdf viewer API to "find in page".
            if (tab != null && tab.isNativePage() && tab.getNativePage().isPdf()) {
                NativePage pdfPage = tab.getNativePage();
                assert pdfPage instanceof PdfPage;
                return ((PdfPage) pdfPage).findInPage();
            }

            if (mFindToolbarManager == null) return false;

            mFindToolbarManager.showToolbar();

            if (fromMenu) {
                RecordUserAction.record("MobileMenuFindInPage");
                new UkmRecorder(tab.getWebContents(), "MobileMenu.FindInPage")
                        .addBooleanMetric("HasOccurred")
                        .record();
            } else {
                RecordUserAction.record("MobileShortcutFindInPage");
            }
            return true;
        } else if (id == R.id.share_menu_id || id == R.id.direct_share_menu_id) {
            onShareMenuItemSelected(
                    id == R.id.direct_share_menu_id,
                    mTabModelSelectorSupplier.get().isIncognitoSelected());
            return true;
        } else if (id == R.id.paint_preview_show_id) {
            DemoPaintPreview.showForTab(mActivityTabProvider.get());
            return true;
        } else if (id == R.id.get_image_descriptions_id) {
            ImageDescriptionsController.getInstance()
                    .onImageDescriptionsMenuItemSelected(
                            mActivity,
                            mModalDialogManagerSupplier.get(),
                            mActivityTabProvider.get().getWebContents());
            return true;
        } else if (id == R.id.page_zoom_id) {
            Tab tab = mActivityTabProvider.get();
            TrackerFactory.getTrackerForProfile(tab.getProfile())
                    .notifyEvent(EventConstants.PAGE_ZOOM_OPENED);
            mPageZoomBarCoordinator.show(tab.getWebContents());
        } else if (id == R.id.open_with_id) {
            Tab tab = mActivityTabProvider.get();
            assert tab != null && tab.isNativePage() && tab.getNativePage() instanceof PdfPage;
            Uri uri = ((PdfPage) tab.getNativePage()).getUri();
            if (uri == null
                    || !DownloadUtils.openFileWithExternalApps(
                            uri.toString(),
                            MimeTypeUtils.PDF_MIME_TYPE,
                            /* originalUrl= */ null,
                            /* referrer= */ null,
                            mActivity,
                            OpenWithExternalAppsSource.APP_MENU)) {
                Toast.makeText(
                                mActivity,
                                mActivity.getString(R.string.download_cant_open_file),
                                Toast.LENGTH_SHORT)
                        .show();
            }
            return true;
        } else if (id == R.id.esc_key) {
            // Unlike Back presses, which are plumbed through an OnBackPressedCallback provided
            // by View.java, Escape key presses do not have an equivalent callback and must be
            // handled manually. However, in most cases we want Escape key presses to behave the
            // same as Back presses, so we intercept them here and send them to the
            // BackPressManager, but Views can override this for custom behavior. Escape key
            // presses that include modifier keys (e.g. Ctrl), are not sent to BackPressManager.
            if (ChromeFeatureList.sKeyboardEscBackNavigation.isEnabled()) {
                if (mBackPressManager != null) {
                    Boolean result = mBackPressManager.processEscapeKeyEvent();
                    return result != null && result;
                }
            }
        }

        return false;
    }

    // AppMenuBlocker implementation

    @Override
    public boolean canShowAppMenu() {
        // TODO(https:crbug.com/931496): Eventually the ContextualSearchManager,
        // EphemeralTabCoordinator, and FindToolbarManager will all be owned by this class.

        // Do not show the menu if Contextual Search panel is opened.
        var manager = mContextualSearchManagerSupplier.get();
        if (manager != null && manager.isSearchPanelOpened()) {
            return false;
        }

        // Do not show the menu if we are in find in page view.
        if (mFindToolbarManager != null
                && mFindToolbarManager.isShowing()
                && !DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            return false;
        }

        return true;
    }

    /** Returns the {@link MenuButtonCoordinator.VisibilityDelegate}. */
    protected @Nullable MenuButtonCoordinator.VisibilityDelegate getMenuButtonVisibilityDelegate() {
        return null;
    }

    // WindowFocusChangedObserver implementation

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        if (!hasFocus && mPageZoomBarCoordinator != null) {
            // If the window loses focus, dismiss the slider so two windows cannot modify the same
            // value simultaneously.
            mPageZoomBarCoordinator.hide();
        }
    }

    /** Returns the {@link PageZoomManager}. */
    public PageZoomManager getPageZoomManager() {
        return mPageZoomManager;
    }

    // Protected class methods
    protected void onLayoutManagerAvailable(LayoutManagerImpl layoutManager) {
        mLayoutManager = layoutManager;
        if (mOverlayPanelManager != null) {
            mOverlayPanelManager.removeObserver(mOverlayPanelManagerObserver);
        }
        mOverlayPanelManager = layoutManager.getOverlayPanelManager();

        if (mOverlayPanelManagerObserver == null) {
            mOverlayPanelManagerObserver =
                    new OverlayPanelManager.OverlayPanelManagerObserver() {
                        @Override
                        public void onOverlayPanelShown() {
                            if (mFindToolbarManager != null) {
                                mFindToolbarManager.hideToolbar(false);
                            }

                            if (mPageZoomBarCoordinator != null) {
                                // On show overlay panel, hide page zoom dialog
                                mPageZoomBarCoordinator.hide();
                            }
                        }

                        @Override
                        public void onOverlayPanelHidden() {}
                    };
        }

        mOverlayPanelManager.addObserver(mOverlayPanelManagerObserver);
    }

    protected AdaptiveToolbarBehavior createAdaptiveToolbarBehavior(
            Supplier<Tracker> trackerSupplier) {
        assert false : "Should be overriden by an inherited class.";
        return null;
    }

    /**
     * Constructs {@link ToolbarManager} and the handler necessary for controlling the menu on the
     * {@link Toolbar}.
     */
    protected void initializeToolbar() {
        try (TraceEvent te = TraceEvent.scoped("RootUiCoordinator.initializeToolbar")) {
            final View controlContainer = mActivity.findViewById(R.id.control_container);
            assert controlContainer != null;
            mToolbarContainer = (ToolbarControlContainer) controlContainer;

            Callback<Boolean> urlFocusChangedCallback =
                    hasFocus -> {
                        if (mOnOmniboxFocusChangedListener != null) {
                            mOnOmniboxFocusChangedListener.onResult(hasFocus);
                        }
                        if (mMessageQueueMediator != null) {
                            mMessageQueueMediator.onUrlFocusChange(hasFocus);
                        }
                        mOmniboxFocusStateSupplier.set(hasFocus);
                    };
            if (getDesktopWindowStateManager() != null) {
                mToolbarContainer.setAppInUnfocusedDesktopWindow(
                        getDesktopWindowStateManager().isInUnfocusedDesktopWindow());
            }

            Supplier<Tracker> trackerSupplier =
                    () -> {
                        Profile profile = mProfileSupplier.get();
                        return profile == null
                                ? null
                                : TrackerFactory.getTrackerForProfile(profile);
                    };
            mAdaptiveToolbarUiCoordinator =
                    new AdaptiveToolbarUiCoordinator(
                            mActivity, mActivityTabProvider, mModalDialogManagerSupplier);
            mAdaptiveToolbarUiCoordinator.initialize(
                    createAdaptiveToolbarBehavior(trackerSupplier),
                    mActivityLifecycleDispatcher,
                    mTabModelSelectorSupplier,
                    getBottomSheetController(),
                    mSnackbarManagerSupplier,
                    mTabBookmarkerSupplier,
                    mProfileSupplier,
                    mBookmarkModelSupplier,
                    mReadAloudControllerSupplier,
                    mShareDelegateSupplier,
                    /* onShareRunnable= */ () ->
                            mToolbarManager.setUrlBarFocus(false, OmniboxFocusReason.UNFOCUS),
                    mWindowAndroid,
                    trackerSupplier,
                    this::getScrimManager,
                    mReaderModeIphControllerSupplier);

            var omniboxActionDelegate =
                    new OmniboxActionDelegateImpl(
                            mActivity,
                            mActivityTabProvider,
                            // TODO(ender): phase out callbacks when the modules below are
                            // components.
                            // Open URL in an existing, else new regular tab.
                            url -> {
                                Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
                                intent.setComponent(
                                        new ComponentName(mActivity, ChromeLauncherActivity.class));
                                intent.putExtra(
                                        WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
                                mActivity.startActivity(intent);
                            },
                            // Open Incognito Tab callback:
                            () -> {
                                mActivity.startActivity(
                                        IntentHandler.createTrustedOpenNewTabIntent(
                                                mActivity, true));
                            },
                            // Open Password Settings callback:
                            () -> {
                                PasswordManagerLauncher.showPasswordSettings(
                                        mActivity,
                                        mProfileSupplier.get(),
                                        ManagePasswordsReferrer.CHROME_SETTINGS,
                                        mModalDialogManagerSupplier,
                                        /* managePasskeys= */ false);
                            },
                            // Open Quick Delete Dialog callback:
                            () -> {
                                TabModelSelectorBase tabModelSelector =
                                        ArchivedTabModelOrchestrator.getForProfile(
                                                        mProfileSupplier.get())
                                                .getTabModelSelector();
                                assert tabModelSelector != null;
                                QuickDeleteController quickDeleteController =
                                        new QuickDeleteController(
                                                mActivity,
                                                new QuickDeleteDelegateImpl(
                                                        mProfileSupplier, mTabSwitcherSupplier),
                                                mModalDialogManagerSupplier.get(),
                                                mSnackbarManagerSupplier.get(),
                                                mLayoutManager,
                                                mTabModelSelectorSupplier.get(),
                                                tabModelSelector);
                                quickDeleteController.showDialog();
                            },
                            TabWindowManagerSingleton::getInstance,
                            this::bringTabToFront);

            mToolbarManager =
                    new ToolbarManager(
                            mActivity,
                            mBottomControlsStacker,
                            mBrowserControlsManager,
                            mFullscreenManager,
                            mEdgeToEdgeControllerSupplier,
                            mToolbarContainer,
                            mCompositorViewHolderSupplier.get(),
                            urlFocusChangedCallback,
                            mTopUiThemeColorProvider,
                            mAdjustedTopUiThemeColorProvider,
                            mTabObscuringHandlerSupplier.get(),
                            mShareDelegateSupplier,
                            mAdaptiveToolbarUiCoordinator.getButtonDataProviders(),
                            mActivityTabProvider,
                            mScrimManager,
                            mActionModeControllerCallback,
                            mFindToolbarManager,
                            mProfileSupplier,
                            mBookmarkModelSupplier,
                            mLayoutStateProviderOneShotSupplier,
                            mAppMenuSupplier,
                            canShowMenuUpdateBadge(),
                            mTabModelSelectorSupplier,
                            mOmniboxFocusStateSupplier,
                            mPromoShownOneshotSupplier,
                            mWindowAndroid,
                            mChromeAndroidTaskSupplier,
                            mIsInOverviewModeSupplier,
                            mModalDialogManagerSupplier,
                            mStatusBarColorController,
                            mAppMenuDelegate,
                            mActivityLifecycleDispatcher,
                            mBottomSheetController,
                            getDataSharingTabManager(),
                            mTabContentManagerSupplier.get(),
                            mTabCreatorManagerSupplier.get(),
                            getMerchantTrustSignalsCoordinatorSupplier(),
                            omniboxActionDelegate,
                            mEphemeralTabCoordinatorSupplier,
                            mInitializeUiWithIncognitoColors,
                            mBackPressManager,
                            mReadAloudControllerSupplier,
                            getDesktopWindowStateManager(),
                            getMultiInstanceManager(),
                            mTabBookmarkerSupplier,
                            getMenuButtonVisibilityDelegate(),
                            mTopControlsStacker,
                            mTopInsetCoordinatorSupplier,
                            mXrSpaceModeObservableSupplier,
                            mPageZoomManager,
                            mSnackbarManagerSupplier.get());
            if (!mSupportsAppMenuSupplier.getAsBoolean()) {
                mToolbarManager.getToolbar().disableMenuButton();
            }

            var voiceButtonController =
                    mAdaptiveToolbarUiCoordinator.getVoiceToolbarButtonController();
            var voiceRecognitionHandler = mToolbarManager.getVoiceRecognitionHandler();
            if (voiceButtonController != null && voiceRecognitionHandler != null) {
                mMicStateObserver = voiceButtonController::updateMicButtonState;
                voiceRecognitionHandler.addObserver(mMicStateObserver);
            }
            mToolbarManagerOneshotSupplier.set(mToolbarManager);
        }
    }

    protected void addVoiceSearchAdaptiveButton(Supplier<Tracker> trackerSupplier) {
        mAdaptiveToolbarUiCoordinator.addVoiceSearchAdaptiveButton(
                () -> mToolbarManager.getVoiceRecognitionHandler(), trackerSupplier);
    }

    /**
     * Constructs a {@link ScrimManager} and sets up observers. Lifetime of all these objects should
     * match.
     */
    protected ScrimManager buildScrimWidget() {
        ViewGroup coordinator = mActivity.findViewById(R.id.coordinator);
        ScrimManager scrimManager =
                new ScrimManager(mActivity, coordinator, ScrimClient.ROOT_UI_COORDINATOR);
        scrimManager
                .getStatusBarColorSupplier()
                .addObserver(RootUiCoordinator.this::onScrimColorChanged);
        return scrimManager;
    }

    protected void onScrimColorChanged(@ColorInt int scrimColor) {
        mStatusBarColorController.setScrimColor(scrimColor);
    }

    protected void setLayoutStateProvider(LayoutStateProvider layoutStateProvider) {
        assert layoutStateProvider != null;
        assert mLayoutStateProvider == null : "The LayoutStateProvider should set at most once.";

        mLayoutStateProvider = layoutStateProvider;
        mLayoutStateObserver =
                new LayoutStateProvider.LayoutStateObserver() {
                    @Override
                    public void onStartedShowing(int layoutType) {
                        if (layoutType != LayoutType.BROWSING
                                && layoutType != LayoutType.SIMPLE_ANIMATION) {
                            // Hide contextual search.
                            ContextualSearchManager contextualSearchManager =
                                    mContextualSearchManagerSupplier.get();
                            if (contextualSearchManager != null) {
                                contextualSearchManager.dismissContextualSearchBar();
                            }
                        }

                        if (layoutType == LayoutType.TAB_SWITCHER) {
                            // Hide find toolbar and app menu.
                            if (mFindToolbarManager != null) mFindToolbarManager.hideToolbar();
                            hideAppMenu();
                            // Attempt to show the promo sheet for the restore tabs feature.
                            // Do not attempt to show the promo if in incognito mode.
                            if (!mTabModelSelectorSupplier.get().isIncognitoSelected()) {
                                // TODO(crbug.com/40274033): Add support for triggering in incognito
                                // mode.
                                attemptToShowRestoreTabsPromo();
                            }
                        }
                    }

                    @Override
                    public void onFinishedShowing(int layoutType) {
                        if (layoutType == LayoutType.TAB_SWITCHER) {
                            // Ideally we wouldn't allow the app menu to show while animating the
                            // overview mode. This is hard to track, however, because in some
                            // instances #onOverviewModeStartedShowing is called after
                            // #onOverviewModeFinishedShowing (see https://crbug.com/969047).
                            // Once that bug is fixed, we can remove this call to hide in favor of
                            // disallowing app menu shows during animation. Alternatively, we
                            // could expose a way to query whether an animation is in progress.
                            hideAppMenu();
                        }
                    }

                    @Override
                    public void onStartedHiding(int layoutType) {
                        if (layoutType == LayoutType.TAB_SWITCHER) {
                            hideAppMenu();
                        }
                    }

                    @Override
                    public void onFinishedHiding(int layoutType) {
                        if (layoutType != LayoutType.TAB_SWITCHER) {
                            hideAppMenu();
                        }
                    }
                };
        mLayoutStateProvider.addObserver(mLayoutStateObserver);
    }

    private void initAppMenu() {
        // TODO(crbug.com/40613711): Revisit this as part of the broader
        // discussion around activity-specific UI customizations.
        if (mSupportsAppMenuSupplier.getAsBoolean()) {
            SubmenuHeaderFactory submenuHeaderFactory =
                    (clickedItem, backRunnable) -> {
                        PropertyModel.Builder builder =
                                new PropertyModel.Builder(
                                        AppMenuSubmenuHeaderItemProperties.ALL_KEYS);
                        HierarchicalMenuController.populateDefaultHeaderProperties(
                                builder,
                                new AppMenuUtil.AppMenuKeyProvider(),
                                clickedItem.model.get(AppMenuItemProperties.TITLE),
                                backRunnable);
                        builder.with(
                                AppMenuItemProperties.MENU_ITEM_ID, R.id.submenu_header_menu_id);
                        return new ListItem(
                                AppMenuHandler.AppMenuItemType.SUBMENU_HEADER, builder.build());
                    };

            mAppMenuCoordinator =
                    AppMenuCoordinatorFactory.createAppMenuCoordinator(
                            mActivity,
                            mActivityLifecycleDispatcher,
                            mToolbarManager,
                            mAppMenuDelegate,
                            mActivity.getWindow().getDecorView(),
                            mActivity
                                    .getWindow()
                                    .getDecorView()
                                    .findViewById(R.id.menu_anchor_stub),
                            this::getAppRectOnScreen,
                            mWindowAndroid,
                            mBrowserControlsManager,
                            submenuHeaderFactory);
            AppMenuCoordinatorFactory.setExceptionReporter(
                    ChromePureJavaExceptionReporter::reportJavaException);

            mAppMenuCoordinator.registerAppMenuBlocker(this);
            mAppMenuCoordinator.registerAppMenuBlocker(mAppMenuBlocker);

            mAppMenuSupplier.set(mAppMenuCoordinator);

            mAppMenuObserver =
                    new AppMenuObserver() {
                        @Override
                        public void onMenuVisibilityChanged(boolean isVisible) {
                            if (isVisible && mPageZoomBarCoordinator != null) {
                                // On show app menu, hide page zoom dialog
                                mPageZoomBarCoordinator.hide();
                            }
                        }

                        @Override
                        public void onMenuHighlightChanged(boolean highlighting) {}
                    };
            mAppMenuCoordinator.getAppMenuHandler().addObserver(mAppMenuObserver);
        }
    }

    /** Returns {@link Rect} that represents the app client area the app menu should fit in. */
    protected Rect getAppRectOnScreen() {
        Rect appRect = new Rect();
        mActivity.getWindow().getDecorView().getWindowVisibleDisplayFrame(appRect);
        return appRect;
    }

    private void hideAppMenu() {
        if (mAppMenuCoordinator != null) mAppMenuCoordinator.getAppMenuHandler().hideAppMenu();
    }

    protected int getFindToolbarStub() {
        int stubId = R.id.find_toolbar_stub;
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            stubId = R.id.find_toolbar_tablet_stub;
        }
        return stubId;
    }

    private void initFindToolbarManager() {
        if (!mSupportsFindInPageSupplier.getAsBoolean()) return;
        int stubId = getFindToolbarStub();
        mFindToolbarManager =
                new FindToolbarManager(
                        mActivity.findViewById(stubId),
                        mTabModelSelectorSupplier.get(),
                        mWindowAndroid,
                        mActionModeControllerCallback,
                        mBackPressManager);

        mFindToolbarObserver =
                new FindToolbarObserver() {
                    @Override
                    public void onFindToolbarShown() {
                        RootUiCoordinator.this.onFindToolbarShown();
                    }

                    @Override
                    public void onFindToolbarHidden() {
                        RootUiCoordinator.this.onFindToolbarHidden();
                    }
                };

        mFindToolbarManager.addObserver(mFindToolbarObserver);
    }

    /**
     * Called when the find in page toolbar is shown. Sub-classes may override to manage
     * cross-feature interaction, e.g. hide other features when this feature is shown.
     */
    protected void onFindToolbarShown() {
        ContextualSearchManager contextualSearchManager = mContextualSearchManagerSupplier.get();
        if (contextualSearchManager != null) {
            contextualSearchManager.hideContextualSearch(OverlayPanel.StateChangeReason.UNKNOWN);
        }
    }

    /**
     * Called when the find in page toolbar is shown. Sub-classes may override to manage
     * cross-feature interaction, e.g. hide other features when this feature is shown.
     */
    protected void onFindToolbarHidden() {}

    /**
     * @return Whether the "update available" badge can be displayed on menu button(s) in the
     *     context of this coordinator's UI.
     */
    protected boolean canShowMenuUpdateBadge() {
        return false;
    }

    /**
     * Whether the top toolbar theme color provider should allow using the web pages theme if the
     * device is in night mode.
     */
    protected boolean shouldAllowThemingInNightMode() {
        return false;
    }

    /** Whether the top toolbar theme color provider should allow bright theme colors. */
    protected boolean shouldAllowBrightThemeColors() {
        return false;
    }

    /**
     * Whether the top toolbar theme color provider should allow using a web page theme on large
     * form-factors.
     */
    protected boolean shouldAllowThemingOnTablets() {
        return false;
    }

    /**
     * Initialize the {@link BottomSheetController}. The view for this component is not created
     * until content is requested in the sheet.
     */
    private void initializeBottomSheetController() {
        // TODO(crbug.com/40135254): Componentize SnackbarManager so BottomSheetController can own
        // this.
        Callback<View> sheetInitializedCallback =
                (view) -> {
                    mBottomSheetSnackbarManager =
                            new SnackbarManager(
                                    mActivity,
                                    view.findViewById(R.id.bottom_sheet_snackbar_container),
                                    mWindowAndroid,
                                    mEdgeToEdgeControllerSupplier);
                };

        Supplier<OverlayPanelManager> panelManagerSupplier =
                () -> {
                    var compositorViewHolder = mCompositorViewHolderSupplier.get();
                    if (compositorViewHolder != null
                            && compositorViewHolder.getLayoutManager() != null) {
                        return compositorViewHolder.getLayoutManager().getOverlayPanelManager();
                    }
                    return null;
                };

        // TODO(crbug.com/40135255): Initialize after inflation so we don't need to pass in view
        // suppliers.
        mBottomSheetController =
                BottomSheetControllerFactory.createBottomSheetController(
                        () -> mScrimManager,
                        sheetInitializedCallback,
                        mActivity.getWindow(),
                        mWindowAndroid.getKeyboardDelegate(),
                        () -> {
                            if (mActivity != null) {
                                return mActivity.findViewById(R.id.sheet_container);
                            }
                            return null;
                        },
                        () -> {
                            var edgeToEdgeController = mEdgeToEdgeControllerSupplier.get();
                            return edgeToEdgeController == null
                                    ? 0
                                    : edgeToEdgeController.getBottomInset();
                        },
                        getDesktopWindowStateManager());
        BottomSheetControllerFactory.setExceptionReporter(
                ChromePureJavaExceptionReporter::reportJavaException);
        BottomSheetControllerFactory.attach(mWindowAndroid, mBottomSheetController);

        mBottomSheetManager =
                new BottomSheetManager(
                        mBottomSheetController,
                        mActivityTabProvider,
                        mBrowserControlsManager,
                        mExpandedBottomSheetHelper,
                        this::getBottomSheetSnackbarManager,
                        mOmniboxFocusStateSupplier,
                        panelManagerSupplier,
                        mLayoutStateProviderOneShotSupplier);

        // TODO(crbug.com/40208738): Consider moving handler registration to feature code.
        assert mBackPressManager != null
                && !mBackPressManager.has(BackPressHandler.Type.BOTTOM_SHEET);
        BackPressHandler mBottomSheetBackPressHandler =
                mBottomSheetController.getBottomSheetBackPressHandler();
        if (mBottomSheetBackPressHandler != null) {
            mBackPressManager.addHandler(
                    mBottomSheetBackPressHandler, BackPressHandler.Type.BOTTOM_SHEET);
        }
    }

    /**
     * @return whether the Android Edge To Edge Feature is supported for the current activity.
     */
    protected boolean supportsEdgeToEdge() {
        return false;
    }

    /** Setup drawing using Android Edge-to-Edge. */
    @CallSuper
    protected void initializeEdgeToEdgeController() {
        boolean eligible = EdgeToEdgeUtils.recordEligibility(mActivity);

        UmaSessionStats.registerSyntheticFieldTrial(
                "EdgeToEdgeChinEligibility", eligible ? "Eligible" : "Not Eligible");

        if (supportsEdgeToEdge()) {
            assert eligible
                    : "The edge-to-edge controller is being initialized, though it should not be"
                            + " eligible!";
            if (mEdgeToEdgeControllerCreator != null) {
                // Clean up the creator before creating the controller to ensure the creator doesn't
                // receive insets again when the EdgeToEdgeController gets created, as the
                // controller
                // re-triggers inset consumption during its initialization.
                mEdgeToEdgeControllerCreator.destroy();
                mEdgeToEdgeControllerCreator = null;
            }

            mEdgeToEdgeController =
                    EdgeToEdgeControllerFactory.create(
                            mActivity,
                            mWindowAndroid,
                            mActivityTabProvider,
                            mEdgeToEdgeManager,
                            mBrowserControlsManager,
                            mLayoutManagerSupplier,
                            mFullscreenManager);
            mEdgeToEdgeControllerSupplier.set(mEdgeToEdgeController);
            mEdgeToEdgeBottomChin = createEdgeToEdgeBottomChin();

            recordIfMissingNavigationBar();
        }
    }

    private void recordIfMissingNavigationBar() {
        var rootInsets = mActivity.getWindow().getDecorView().getRootWindowInsets();
        assert rootInsets != null;

        var rootInsetsCompat = WindowInsetsCompat.toWindowInsetsCompat(rootInsets);
        Insets navigationBarInsets =
                rootInsetsCompat.getInsets(WindowInsetsCompat.Type.navigationBars());
        if (!navigationBarInsets.equals(Insets.NONE)) {
            return;
        }

        @MissingNavbarInsetsReason int reason;
        if (AppHeaderUtils.isAppInDesktopWindow(getDesktopWindowStateManager())) {
            reason = MissingNavbarInsetsReason.IN_DESKTOP_WINDOW;
        } else if (MultiWindowUtils.getInstance().isInMultiWindowMode(mActivity)) {
            reason = MissingNavbarInsetsReason.IN_MULTI_WINDOW;
        } else if (mFullscreenManager.getPersistentFullscreenMode()) {
            // Fullscreen mode can lead to empty system bar insets. Being in fullscreen mode during
            // activity recreation should be rare.
            reason = MissingNavbarInsetsReason.IN_FULLSCREEN;
        } else if (!MultiWindowUtils.isActivityVisible(mActivity)) {
            // When activity is not visible (during theme recreation) we could get empty system bar
            // insets.
            reason = MissingNavbarInsetsReason.ACTIVITY_NOT_VISIBLE;
        } else if (rootInsetsCompat
                .getInsets(WindowInsetsCompat.Type.systemBars())
                .equals(Insets.NONE)) {
            reason = MissingNavbarInsetsReason.SYSTEM_BAR_INSETS_EMPTY;
        } else {
            reason = MissingNavbarInsetsReason.OTHER;
        }

        EdgeToEdgeUtils.recordIfMissingNavigationBar(reason);
    }

    /** Create a bottom chin for Edge-to-Edge. */
    protected Destroyable createEdgeToEdgeBottomChin() {
        return null;
    }

    /**
     * TODO(jinsukkim): remove/hide this in favor of wiring it directly.
     *
     * @return {@link TabObscuringHandler} object.
     */
    public TabObscuringHandler getTabObscuringHandler() {
        return mTabObscuringHandlerSupplier.get();
    }

    /** @return The {@link BottomSheetController} for this activity. */
    public ManagedBottomSheetController getBottomSheetController() {
        return mBottomSheetController;
    }

    /**
     * @return Supplies the {@link EphemeralTabCoordinator}
     */
    public Supplier<EphemeralTabCoordinator> getEphemeralTabCoordinatorSupplier() {
        return mEphemeralTabCoordinatorSupplier;
    }

    /**
     * @return The {@link FindToolbarManager} controlling find toolbar.
     */
    public @Nullable FindToolbarManager getFindToolbarManager() {
        return mFindToolbarManager;
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

    /**
     * Gets the browser controls manager, creates it unless already created.
     *
     * @deprecated Instead, inject this directly to your constructor. If that's not possible, then
     *     use {@link BrowserControlsManagerSupplier}.
     */
    @NonNull
    @Deprecated
    public BrowserControlsManager getBrowserControlsManager() {
        return mBrowserControlsManager;
    }

    /** Returns the {@link ScrimManager} to control scrims over the activity. */
    public ScrimManager getScrimManager() {
        return mScrimManager;
    }

    /** @return The {@link SnackbarManager} for the {@link BottomSheetController}. */
    public SnackbarManager getBottomSheetSnackbarManager() {
        return mBottomSheetSnackbarManager;
    }

    /**
     * @return The {@link TopControlsStacker} that handles all layers for this instance.
     */
    public TopControlsStacker getTopControlsStacker() {
        return mTopControlsStacker;
    }

    public boolean getBookmarkBarVisibility() {
        return false;
    }

    /**
     * Initializes a glue logic that suppresses Contextual Search and hides the Page Zoom slider
     * while a Bottom Sheet feature is in action.
     */
    private void initBottomSheetObserver() {
        if (mBottomSheetController == null) return;
        mBottomSheetObserver =
                new EmptyBottomSheetObserver() {
                    private boolean mOpened;

                    @Override
                    public void onSheetStateChanged(int newState, int reason) {
                        switch (newState) {
                            case SheetState.PEEK:
                            case SheetState.HALF:
                            case SheetState.FULL:
                                if (!mOpened) {
                                    mOpened = true;
                                    ContextualSearchManager manager =
                                            mContextualSearchManagerSupplier.get();
                                    if (manager != null) manager.onBottomSheetVisible(true);
                                }

                                // On visible bottom sheet, hide page zoom dialog
                                mPageZoomBarCoordinator.hide();
                                break;
                            case SheetState.HIDDEN:
                                mOpened = false;
                                ContextualSearchManager manager =
                                        mContextualSearchManagerSupplier.get();
                                if (manager != null) manager.onBottomSheetVisible(false);
                                break;
                        }
                    }
                };
        mBottomSheetController.addObserver(mBottomSheetObserver);
    }

    /** Initialize logic for hiding page zoom slider when snackbar is showing */
    private void initSnackbarObserver() {
        mSnackbarManagerSupplier
                .get()
                .isShowingSupplier()
                .addObserver(
                        (Boolean isShowing) -> {
                            if (isShowing && mPageZoomBarCoordinator != null) {
                                // On show snackbar, hide page zoom dialog
                                mPageZoomBarCoordinator.hide();
                            }
                        });
    }

    /**
     * Initialize logic for changing page zoom slider margins when browser bottom controls are
     * showing
     */
    private void initBrowserControlsObserver() {
        mBrowserControlsObserver =
                new BrowserControlsStateProvider.Observer() {
                    @Override
                    public void onBottomControlsHeightChanged(
                            int bottomControlsHeight, int bottomControlsMinHeight) {
                        mPageZoomBarCoordinator.onBottomControlsHeightChanged(bottomControlsHeight);
                    }
                };
        mBrowserControlsManager.addObserver(mBrowserControlsObserver);
    }

    public OneshotSupplier<IncognitoReauthController> getIncognitoReauthControllerSupplier() {
        return mIncognitoReauthControllerOneshotSupplier;
    }

    /** Returns the supplier of {@link ReadAloudController}. */
    @NonNull
    public Supplier<ReadAloudController> getReadAloudControllerSupplier() {
        return mReadAloudControllerSupplier;
    }

    /** Returns the {@link AppMenuHandler}. */
    public @Nullable AppMenuHandler getAppMenuHandler() {
        if (mAppMenuCoordinator != null) {
            return mAppMenuCoordinator.getAppMenuHandler();
        }
        return null;
    }

    /** Saves the relevant UI state when the activity is recreated on a device fold transition. */
    public void prepareUiState() {
        mActivityRecreationController.prepareUiState();
    }

    /**
     * Saves relevant information preserved by {@code RootUiCoordinator#prepareUiState()} to the
     * saved instance state bundle that will be used to restore the UI state after the activity is
     * recreated. This is expected to be invoked in {@code Activity#onSaveInstanceState(Bundle)}.
     *
     * @param outState The {@link Bundle} that is used to save state information.
     */
    public void onSaveInstanceState(Bundle outState) {
        assert mTabModelSelectorSupplier.get() != null;
        mActivityRecreationController.saveUiState(outState);
        if (mExclusiveAccessManager != null) {
            mExclusiveAccessManager.saveFullscreenState(outState);
        }
    }

    /**
     * Restores the relevant UI state when the activity is recreated on a device fold transition.
     *
     * @param savedInstanceState The {@link Bundle} that is used to restore the UI state.
     */
    public void restoreUiState(Bundle savedInstanceState) {
        mActivityRecreationController.restoreUiState(savedInstanceState);
    }

    private void attemptToShowRestoreTabsPromo() {
        if (mRestoreTabsFeatureHelper == null) {
            mRestoreTabsFeatureHelper = new RestoreTabsFeatureHelper();
        }

        Supplier<Integer> gtsTabListModelSizeSupplier =
                () -> {
                    var tabSwitcher = mTabSwitcherSupplier.get();
                    if (tabSwitcher != null) {
                        return tabSwitcher.getTabSwitcherTabListModelSize();
                    }
                    return 0;
                };

        Callback<Integer> scrollGTSToRestoredTabsCallback =
                (tabListModelSize) -> {
                    var tabSwitcher = mTabSwitcherSupplier.get();
                    if (tabSwitcher != null) {
                        tabSwitcher.setTabSwitcherRecyclerViewPosition(
                                new RecyclerViewPosition(tabListModelSize, 0));
                    }
                };

        mRestoreTabsFeatureHelper.maybeShowPromo(
                mActivity,
                mProfileSupplier.get(),
                mTabCreatorManagerSupplier.get(),
                getBottomSheetController(),
                gtsTabListModelSizeSupplier,
                scrollGTSToRestoredTabsCallback,
                mModalDialogManagerSupplier);
    }

    private void initBoardingPassDetector() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.BOARDING_PASS_DETECTOR)) {
            mBoardingPassController = new BoardingPassController(mActivityTabProvider);
        }
    }

    private void bringTabToFront(TabWindowInfo tabWindowInfo, GURL url) {
        TabModel tabModel = tabWindowInfo.tabModel;
        int tabId = tabWindowInfo.tab.getId();

        // Switch to the tab directly if it is in same TabModel.
        if (assumeNonNull(mTabModelSelectorSupplier.get()).getCurrentModel() == tabModel) {
            int tabIndex = TabModelUtils.getTabIndexById(tabModel, tabId);
            // In the event the user deleted the tab as part during the interaction with the
            // Omnibox, reject the switch to tab action.
            if (tabIndex == TabModel.INVALID_TAB_INDEX) return;
            tabModel.setIndex(tabIndex, TabSelectionType.FROM_OMNIBOX);
            return;
        }

        Intent intent = new Intent(mActivity, mActivity.getClass());
        intent.setAction(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url.getSpec()));
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, mActivity.getPackageName());
        intent.putExtra(TabOpenType.REUSE_TAB_MATCHING_ID_STRING, tabId);
        IntentHandler.setTabLaunchType(intent, TabLaunchType.FROM_OMNIBOX);
        MultiWindowUtils.launchIntentInMaybeClosedWindow(mActivity, intent, tabWindowInfo.windowId);
    }

    // Testing methods

    public AppMenuCoordinator getAppMenuCoordinatorForTesting() {
        return mAppMenuCoordinator;
    }

    public void destroyActivityForTesting() {
        // Actually destroying or finishing the activity hinders the shutdown process after
        // a test is done. Just null it out to give an effect of |onDestroy| being invoked.
        mActivity = null;
    }

    public BottomControlsStacker getBottomControlsStackerForTesting() {
        return mBottomControlsStacker;
    }

    public @Nullable AdaptiveToolbarUiCoordinator getAdaptiveToolbarUiCoordinatorForTesting() {
        return mAdaptiveToolbarUiCoordinator;
    }

    public DataSharingTabManager getDataSharingTabManager() {
        // This should only be called on an instance of TabbedRootUiCoordinator.
        return null;
    }

    /** Returns the entry point for all scrim interactions. */
    public ObservableSupplier<ScrimManager> getScrimManagerSupplier() {
        return mScrimManagerSupplier;
    }

    /** Returns a supplier of the share delegate. */
    public Supplier<ShareDelegate> getShareDelegateSupplier() {
        return mShareDelegateSupplier;
    }

    public @Nullable MultiInstanceManager getMultiInstanceManager() {
        return null;
    }

    public @Nullable ExclusiveAccessManager getExclusiveAccessManager() {
        return mExclusiveAccessManager;
    }
}
