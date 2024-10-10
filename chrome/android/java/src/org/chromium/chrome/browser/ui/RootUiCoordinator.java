// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.app.Fragment;
import android.content.ComponentName;
import android.content.Intent;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.TraceEvent;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneShotCallback;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.ChromeActionModeHandler;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.bookmarks.AddToBookmarksToolbarButtonController;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.commerce.CommerceBottomSheetContentController;
import org.chromium.chrome.browser.commerce.CommerceBottomSheetContentCoordinator;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.commerce.coupons.DiscountsButtonController;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager.ContextualSearchTabPromotionDelegate;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManagerSupplier;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchObserver;
import org.chromium.chrome.browser.crash.ChromePureJavaExceptionReporter;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.device_lock.DeviceLockActivityLauncherImpl;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.dom_distiller.ReaderModeToolbarButtonController;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.findinpage.FindToolbarManager;
import org.chromium.chrome.browser.findinpage.FindToolbarObserver;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.identity_disc.IdentityDiscController;
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
import org.chromium.chrome.browser.omnibox.OmniboxFocusReason;
import org.chromium.chrome.browser.omnibox.geo.GeolocationHeader;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxActionDelegateImpl;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.VoiceInteractionSource;
import org.chromium.chrome.browser.paint_preview.DemoPaintPreview;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerLauncher;
import org.chromium.chrome.browser.pdf.PdfPage;
import org.chromium.chrome.browser.price_insights.PriceInsightsButtonController;
import org.chromium.chrome.browser.price_tracking.CurrentTabPriceTrackingStateSupplier;
import org.chromium.chrome.browser.price_tracking.PriceTrackingButtonController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.quick_delete.QuickDeleteController;
import org.chromium.chrome.browser.quick_delete.QuickDeleteDelegateImpl;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.readaloud.ReadAloudControllerSupplier;
import org.chromium.chrome.browser.readaloud.ReadAloudToolbarButtonController;
import org.chromium.chrome.browser.recent_tabs.RestoreTabsFeatureHelper;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.segmentation_platform.ContextualPageActionController;
import org.chromium.chrome.browser.share.ShareButtonController;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin;
import org.chromium.chrome.browser.share.ShareUtils;
import org.chromium.chrome.browser.share.page_info_sheet.PageInfoSharingControllerImpl;
import org.chromium.chrome.browser.share.page_info_sheet.PageSummaryButtonController;
import org.chromium.chrome.browser.share.qrcode.QrCodeDialog;
import org.chromium.chrome.browser.share.scroll_capture.ScrollCaptureManager;
import org.chromium.chrome.browser.tab.AccessibilityVisibilityHandler;
import org.chromium.chrome.browser.tab.AutofillSessionLifetimeController;
import org.chromium.chrome.browser.tab.RequestDesktopUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabLoadIfNeededCaller;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tab.TabObscuringHandlerSupplier;
import org.chromium.chrome.browser.tab_ui.RecyclerViewPosition;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider;
import org.chromium.chrome.browser.toolbar.ToolbarIntentMetadata;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.toolbar.VoiceToolbarButtonController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveButtonActionMenuCoordinator;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.OptionalNewTabButtonController;
import org.chromium.chrome.browser.toolbar.adaptive.TranslateToolbarButtonController;
import org.chromium.chrome.browser.toolbar.top.ToolbarActionModeCallback;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer;
import org.chromium.chrome.browser.ui.appmenu.AppMenuBlocker;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinatorFactory;
import org.chromium.chrome.browser.ui.appmenu.AppMenuDelegate;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuObserver;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerFactory;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.chrome.browser.ui.fold_transitions.FoldTransitionController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;
import org.chromium.chrome.browser.ui.system.StatusBarColorController.StatusBarColorProvider;
import org.chromium.chrome.browser.wallet.BoardingPassController;
import org.chromium.components.browser_ui.accessibility.PageZoomCoordinator;
import org.chromium.components.browser_ui.accessibility.PageZoomCoordinatorDelegate;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.ExpandedSheetHelper;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncherSupplier;
import org.chromium.components.browser_ui.util.ComposedBrowserControlsVisibilityDelegate;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.ManagedMessageDispatcher;
import org.chromium.components.messages.MessageContainer;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessagesFactory;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.content_public.browser.ActionModeCallbackHelper;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;
import java.util.List;
import java.util.function.BooleanSupplier;

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

    @Nullable private final Callback<Boolean> mOnOmniboxFocusChangedListener;
    protected ToolbarManager mToolbarManager;
    protected Supplier<Boolean> mCanAnimateBrowserControls;
    private ModalDialogManagerObserver mModalDialogManagerObserver;

    private BottomSheetManager mBottomSheetManager;
    private ManagedBottomSheetController mBottomSheetController;
    private SnackbarManager mBottomSheetSnackbarManager;

    private ScrimCoordinator mScrimCoordinator;
    private List<ButtonDataProvider> mButtonDataProviders;
    @Nullable private AdaptiveToolbarButtonController mAdaptiveToolbarButtonController;
    private ContextualPageActionController mContextualPageActionController;
    private CurrentTabPriceTrackingStateSupplier mCurrentTabPriceTrackingStateSupplier;
    private final ToolbarActionModeCallback mActionModeControllerCallback;
    private final ObservableSupplierImpl<Boolean> mOmniboxFocusStateSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<MerchantTrustSignalsCoordinator>
            mMerchantTrustSignalsCoordinatorSupplier = new ObservableSupplierImpl<>();
    protected final ObservableSupplier<Profile> mProfileSupplier;
    private final ObservableSupplier<BookmarkModel> mBookmarkModelSupplier;
    private final ObservableSupplier<TabBookmarker> mTabBookmarkerSupplier;
    private final OneshotSupplierImpl<AppMenuCoordinator> mAppMenuSupplier;
    private BottomSheetObserver mBottomSheetObserver;
    protected final CallbackController mCallbackController;
    protected final BrowserControlsManager mBrowserControlsManager;
    private BrowserControlsStateProvider.Observer mBrowserControlsObserver;
    protected ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
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
    protected final OneshotSupplierImpl<EphemeralTabCoordinator> mEphemeralTabCoordinatorSupplier;
    @Nullable protected final BackPressManager mBackPressManager;
    private final boolean mIsIncognitoReauthPendingOnRestore;
    protected final ExpandedSheetHelper mExpandedBottomSheetHelper;
    protected final BottomControlsStacker mBottomControlsStacker;
    private final Supplier<Long> mLastUserInteractionTimeSupplier;
    @Nullable private ContextualSearchObserver mReadAloudContextualSearchObserver;
    @Nullable private PageZoomCoordinator mPageZoomCoordinator;
    private AppMenuObserver mAppMenuObserver;

    private final OneshotSupplierImpl<ToolbarManager> mToolbarManagerOneshotSupplier =
            new OneshotSupplierImpl<>();
    private FoldTransitionController mFoldTransitionController;
    private RestoreTabsFeatureHelper mRestoreTabsFeatureHelper;
    private @Nullable EdgeToEdgeController mEdgeToEdgeController;
    private ComposedBrowserControlsVisibilityDelegate mAppBrowserControlsVisibilityDelegate;
    private @Nullable BoardingPassController mBoardingPassController;
    private final @Nullable ObservableSupplier<Integer> mOverviewColorSupplier;
    private final @Nullable View mBaseChromeLayout;
    private CommerceBottomSheetContentCoordinator mCommerceBottomSheetContentCoordinator;

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
     * @param lastUserInteractionTimeSupplier Supplies the last user interaction time.
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
     * @param edgeToEdgeControllerSupplier Supplies an {@link EdgeToEdgeController}.
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
     * @param baseChromeLayout The base view hosting Chrome that certain views (e.g. the omnibox
     *     suggestion list) will position themselves relative to. If null, the content view will be
     *     used.
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
            @NonNull ObservableSupplierImpl<EdgeToEdgeController> edgeToEdgeControllerSupplier,
            @ActivityType int activityType,
            @NonNull Supplier<Boolean> isInOverviewModeSupplier,
            @NonNull AppMenuDelegate appMenuDelegate,
            @NonNull StatusBarColorProvider statusBarColorProvider,
            @NonNull IntentRequestTracker intentRequestTracker,
            @NonNull OneshotSupplierImpl<EphemeralTabCoordinator> ephemeralTabCoordinatorSupplier,
            boolean initializeUiWithIncognitoColors,
            @Nullable BackPressManager backPressManager,
            @Nullable Bundle savedInstanceState,
            @Nullable ObservableSupplier<Integer> overviewColorSupplier,
            @Nullable View baseChromeLayout) {
        mCallbackController = new CallbackController();
        mActivity = activity;
        mWindowAndroid = windowAndroid;
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

        mShareDelegateSupplier = shareDelegateSupplier;
        mTabObscuringHandlerSupplier.set(new TabObscuringHandler());
        mDeviceLockActivityLauncherSupplier.set(DeviceLockActivityLauncherImpl.get());
        new AccessibilityVisibilityHandler(
                mActivityLifecycleDispatcher,
                mActivityTabProvider,
                mTabObscuringHandlerSupplier.get());
        // While Autofill is supported on Android O, meaningful Autofill interactions in Chrome
        // require the compatibility mode introduced in Android P.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            new AutofillSessionLifetimeController(
                    activity, mActivityLifecycleDispatcher, mActivityTabProvider);
        }
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

        mLastUserInteractionTimeSupplier = lastUserInteractionTimeSupplier;
        mTabSwitcherSupplier = tabSwitcherSupplier;
        mIncognitoTabSwitcherSupplier = incognitoTabSwitcherSupplier;
        mIntentMetadataOneshotSupplier = intentMetadataOneshotSupplier;

        boolean isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity);
        mTopUiThemeColorProvider =
                new TopUiThemeColorProvider(
                        mActivity,
                        mActivityTabProvider,
                        activityThemeColorSupplier,
                        isTablet,
                        shouldAllowThemingInNightMode(),
                        shouldAllowBrightThemeColors());

        mStatusBarColorController =
                new StatusBarColorController(
                        mActivity.getWindow(),
                        DeviceFormFactor.isNonMultiDisplayContextOnTablet(/* Context */ mActivity),
                        mActivity,
                        statusBarColorProvider,
                        mLayoutManagerSupplier,
                        mActivityLifecycleDispatcher,
                        mActivityTabProvider,
                        mTopUiThemeColorProvider);
        mEphemeralTabCoordinatorSupplier = ephemeralTabCoordinatorSupplier;

        mPageZoomCoordinator =
                new PageZoomCoordinator(
                        new PageZoomCoordinatorDelegate() {
                            @Override
                            public View getZoomControlView() {
                                ViewStub viewStub =
                                        mActivity.findViewById(R.id.page_zoom_container);
                                return viewStub.inflate();
                            }

                            @Override
                            public BrowserContextHandle getBrowserContextHandle() {
                                return mProfileSupplier.get().getOriginalProfile();
                            }
                        });
        mFoldTransitionController =
                new FoldTransitionController(
                        mToolbarManagerOneshotSupplier,
                        mLayoutManagerSupplier,
                        mActivityTabProvider,
                        new Handler());
        mExpandedBottomSheetHelper =
                new ExpandedSheetHelperImpl(mModalDialogManagerSupplier, getTabObscuringHandler());
        mOverviewColorSupplier = overviewColorSupplier;
        mBaseChromeLayout = baseChromeLayout;
        mBottomControlsStacker = new BottomControlsStacker(mBrowserControlsManager);
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
     * @return The {@link DesktopWindowStateProvider} instance associated with the current activity.
     */
    public @Nullable DesktopWindowStateProvider getDesktopWindowStateProvider() {
        return null;
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

        if (mCurrentTabPriceTrackingStateSupplier != null) {
            mCurrentTabPriceTrackingStateSupplier.destroy();
            mCurrentTabPriceTrackingStateSupplier = null;
        }

        if (mContextualPageActionController != null) {
            mContextualPageActionController.destroy();
            mContextualPageActionController = null;
        }

        if (mAdaptiveToolbarButtonController != null) {
            mAdaptiveToolbarButtonController.destroy();
            mAdaptiveToolbarButtonController = null;
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

        if (mFindToolbarManager != null) mFindToolbarManager.removeObserver(mFindToolbarObserver);

        if (mModalDialogManagerObserver != null && mModalDialogManagerSupplier.hasValue()) {
            mModalDialogManagerSupplier.get().removeObserver(mModalDialogManagerObserver);
        }

        if (mBottomSheetManager != null) mBottomSheetManager.onDestroy();
        if (mBottomSheetController != null) {
            if (mBottomSheetObserver != null) {
                mBottomSheetController.removeObserver(mBottomSheetObserver);
            }
            BottomSheetControllerFactory.detach(mBottomSheetController);
            mBottomSheetController.destroy();
        }

        if (mButtonDataProviders != null) {
            for (ButtonDataProvider provider : mButtonDataProviders) {
                provider.destroy();
            }

            mButtonDataProviders = null;
        }

        if (mScrimCoordinator != null) mScrimCoordinator.destroy();
        mScrimCoordinator = null;

        if (mTabModelSelectorSupplier != null) {
            mTabModelSelectorSupplier = null;
        }

        if (mCaptureController != null) {
            mCaptureController.destroy();
            mCaptureController = null;
        }

        if (mMerchantTrustSignalsCoordinatorSupplier.hasValue()) {
            mMerchantTrustSignalsCoordinatorSupplier.get().destroy();
            mMerchantTrustSignalsCoordinatorSupplier.set(null);
        }

        if (mScrollCaptureManager != null) {
            mScrollCaptureManager.destroy();
            mScrollCaptureManager = null;
        }

        if (mIncognitoReauthController != null) {
            mIncognitoReauthController.destroy();
        }

        if (mPageZoomCoordinator != null) {
            mPageZoomCoordinator.destroy();
            mPageZoomCoordinator = null;
        }

        if (mBrowserControlsObserver != null) {
            mBrowserControlsManager.removeObserver(mBrowserControlsObserver);
        }

        if (mFoldTransitionController != null) {
            mFoldTransitionController = null;
        }

        if (mRestoreTabsFeatureHelper != null) {
            mRestoreTabsFeatureHelper.destroy();
            mRestoreTabsFeatureHelper = null;
        }

        if (mReadAloudControllerSupplier.hasValue()) {
            ContextualSearchManager contextualSearchManager =
                    mContextualSearchManagerSupplier.get();
            if (contextualSearchManager != null) {
                contextualSearchManager.removeObserver(mReadAloudContextualSearchObserver);
            }
            var readAloudController = mReadAloudControllerSupplier.get();
            mReadAloudControllerSupplier.set(null);
            readAloudController.destroy();
        }

        if (mContextualSearchManagerSupplier.hasValue()) {
            mContextualSearchManagerSupplier.get().destroy();
            mContextualSearchManagerSupplier.set(null);
        }

        if (mEdgeToEdgeController != null) {
            mEdgeToEdgeController.destroy();
            mEdgeToEdgeController = null;
        }
        mEdgeToEdgeControllerSupplier.set(null);

        if (mEdgeToEdgeBottomChin != null) {
            mEdgeToEdgeBottomChin.destroy();
        }

        if (mBoardingPassController != null) {
            mBoardingPassController.destroy();
            mBoardingPassController = null;
        }
        mBottomControlsStacker.destroy();
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
        mScrimCoordinator = buildScrimWidget();

        initFindToolbarManager();
        initializeToolbar();
    }

    @Override
    public void onPostInflationStartup() {
        initAppMenu();
        initBottomSheetObserver();
        initSnackbarObserver();
        initBrowserControlsObserver();
        if (mAppMenuCoordinator != null && mModalDialogManagerSupplier.hasValue()) {
            mModalDialogManagerObserver =
                    new ModalDialogManagerObserver() {
                        @Override
                        public void onDialogAdded(PropertyModel model) {
                            mAppMenuCoordinator.getAppMenuHandler().hideAppMenu();
                        }
                    };
            mModalDialogManagerSupplier.get().addObserver(mModalDialogManagerObserver);
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
        if (mProfileSupplier.hasValue()) {
            initProfileDependentFeatures(mProfileSupplier.get());
        } else {
            new OneShotCallback<>(
                    mProfileSupplier,
                    mCallbackController.makeCancelable(this::initProfileDependentFeatures));
        }

        initMessagesInfra();
        initScrollCapture();

        // TODO(crbug.com/350610430) Potentially create the E2EController earlier during startup
        initializeEdgeToEdgeController();
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
                            getBottomSheetController(),
                            canPreviewPromoteToTab()));
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
    }

    /** Preview Tab can be promoted to a normal tab by default. */
    protected boolean canPreviewPromoteToTab() {
        return true;
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
                        mScrimCoordinator,
                        mActivityTabProvider,
                        mFullscreenManager,
                        mBrowserControlsManager,
                        mWindowAndroid,
                        mTabModelSelectorSupplier.get(),
                        mLastUserInteractionTimeSupplier,
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
                mIntentRequestTracker);
    }

    public ObservableSupplier<ContextualSearchManager> getContextualSearchManagerSupplier() {
        return mContextualSearchManagerSupplier;
    }

    /** Whether contextual search panel is opened. */
    public boolean isContextualSearchOpened() {
        return mContextualSearchManagerSupplier.hasValue()
                && mContextualSearchManagerSupplier.get().isSearchPanelOpened();
    }

    /** Hide contextual search panel. */
    public void hideContextualSearch() {
        if (mContextualSearchManagerSupplier.hasValue()) {
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
                && RequestDesktopUtils.maybeDefaultEnableGlobalSetting(
                        getPrimaryDisplaySizeInInches(), originalProfile, mActivity)) {
            // TODO(crbug.com/40856393): Remove this explicit load when this bug is addressed.
            if (mActivityTabProvider != null && mActivityTabProvider.get() != null) {
                mActivityTabProvider
                        .get()
                        .loadIfNeeded(TabLoadIfNeededCaller.ON_FINISH_NATIVE_INITIALIZATION);
            }
        }

        RequestDesktopUtils.maybeDefaultEnableWindowSetting(mActivity, originalProfile);

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
                        if (mPageZoomCoordinator != null) {
                            mPageZoomCoordinator.hide();
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
        double xInches = display.getDisplayWidth() / display.getXdpi();
        double yInches = display.getDisplayHeight() / display.getYdpi();
        return Math.sqrt(Math.pow(xInches, 2) + Math.pow(yInches, 2));
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
     * context of this coordinator's UI.
     **/
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
        String url =
                TemplateUrlServiceFactory.getForProfile(tab.getProfile())
                        .getUrlForSearchQuery(query);
        String headers = GeolocationHeader.getGeoHeader(url, tab);

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
            new UkmRecorder.Bridge()
                    .recordEventWithBooleanMetric(
                            tab.getWebContents(), "MobileMenu.DirectShare", "HasOccurred");
        } else {
            RecordUserAction.record("MobileMenuShare");
            new UkmRecorder.Bridge()
                    .recordEventWithBooleanMetric(
                            tab.getWebContents(), "MobileMenu.Share", "HasOccurred");
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
                new UkmRecorder.Bridge()
                        .recordEventWithBooleanMetric(
                                tab.getWebContents(), "MobileMenu.FindInPage", "HasOccurred");
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
            mPageZoomCoordinator.show(tab.getWebContents());
        }

        return false;
    }

    // AppMenuBlocker implementation

    @Override
    public boolean canShowAppMenu() {
        // TODO(https:crbug.com/931496): Eventually the ContextualSearchManager,
        // EphemeralTabCoordinator, and FindToolbarManager will all be owned by this class.

        // Do not show the menu if Contextual Search panel is opened.
        if (mContextualSearchManagerSupplier.get() != null
                && mContextualSearchManagerSupplier.get().isSearchPanelOpened()) {
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

    // WindowFocusChangedObserver implementation

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        if (!hasFocus && mPageZoomCoordinator != null) {
            // If the window loses focus, dismiss the slider so two windows cannot modify the same
            // value simultaneously.
            mPageZoomCoordinator.hide();
        }
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

                            if (mPageZoomCoordinator != null) {
                                // On show overlay panel, hide page zoom dialog
                                mPageZoomCoordinator.hide();
                            }
                        }

                        @Override
                        public void onOverlayPanelHidden() {}
                    };
        }

        mOverlayPanelManager.addObserver(mOverlayPanelManagerObserver);
    }

    /**
     * Constructs {@link ToolbarManager} and the handler necessary for controlling the menu on the
     * {@link Toolbar}.
     */
    protected void initializeToolbar() {
        try (TraceEvent te = TraceEvent.scoped("RootUiCoordinator.initializeToolbar")) {
            final View controlContainer = mActivity.findViewById(R.id.control_container);
            assert controlContainer != null;
            ToolbarControlContainer toolbarContainer = (ToolbarControlContainer) controlContainer;
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
            if (getDesktopWindowStateProvider() != null) {
                toolbarContainer.setAppInUnfocusedDesktopWindow(
                        getDesktopWindowStateProvider().isInUnfocusedDesktopWindow());
            }

            Supplier<Tracker> trackerSupplier =
                    () -> {
                        Profile profile = mProfileSupplier.get();
                        return profile == null
                                ? null
                                : TrackerFactory.getTrackerForProfile(profile);
                    };

            IdentityDiscController mIdentityDiscController =
                    new IdentityDiscController(
                            mActivity, mActivityLifecycleDispatcher, mProfileSupplier);
            mCurrentTabPriceTrackingStateSupplier =
                    new CurrentTabPriceTrackingStateSupplier(
                            mActivityTabProvider, mProfileSupplier);

            PriceInsightsButtonController priceInsightsButtonController =
                    new PriceInsightsButtonController(
                            mActivity,
                            mActivityTabProvider,
                            mTabModelSelectorSupplier,
                            () -> ShoppingServiceFactory.getForProfile(mProfileSupplier.get()),
                            mModalDialogManagerSupplier.get(),
                            getBottomSheetController(),
                            mSnackbarManagerSupplier.get(),
                            new PriceInsightsDelegateImpl(
                                    mActivity, mCurrentTabPriceTrackingStateSupplier),
                            AppCompatResources.getDrawable(
                                    mActivity, R.drawable.ic_trending_down_24dp),
                            this::getCommerceBottomSheetContentController);
            PriceTrackingButtonController priceTrackingButtonController =
                    new PriceTrackingButtonController(
                            mActivity,
                            mActivityTabProvider,
                            mModalDialogManagerSupplier.get(),
                            getBottomSheetController(),
                            mSnackbarManagerSupplier.get(),
                            mTabBookmarkerSupplier,
                            mProfileSupplier,
                            mBookmarkModelSupplier,
                            mCurrentTabPriceTrackingStateSupplier);
            ReaderModeToolbarButtonController readerModeToolbarButtonController =
                    new ReaderModeToolbarButtonController(
                            mActivity,
                            mActivityTabProvider,
                            mModalDialogManagerSupplier.get(),
                            AppCompatResources.getDrawable(
                                    mActivity, R.drawable.ic_mobile_friendly));
            ReadAloudToolbarButtonController readAloudButtonController =
                    new ReadAloudToolbarButtonController(
                            mActivity,
                            mActivityTabProvider,
                            AppCompatResources.getDrawable(mActivity, R.drawable.ic_play_circle),
                            mReadAloudControllerSupplier,
                            trackerSupplier);

            ShareButtonController shareButtonController =
                    new ShareButtonController(
                            mActivity,
                            AppCompatResources.getDrawable(
                                    mActivity, R.drawable.ic_toolbar_share_offset_24dp),
                            mActivityTabProvider,
                            mShareDelegateSupplier,
                            trackerSupplier,
                            new ShareUtils(),
                            mModalDialogManagerSupplier.get(),
                            () ->
                                    mToolbarManager.setUrlBarFocus(
                                            false, OmniboxFocusReason.UNFOCUS));
            VoiceToolbarButtonController.VoiceSearchDelegate voiceSearchDelegate =
                    new VoiceToolbarButtonController.VoiceSearchDelegate() {
                        @Override
                        public boolean isVoiceSearchEnabled() {
                            VoiceRecognitionHandler voiceRecognitionHandler =
                                    mToolbarManager.getVoiceRecognitionHandler();
                            if (voiceRecognitionHandler == null) return false;
                            return voiceRecognitionHandler.isVoiceSearchEnabled();
                        }

                        @Override
                        public void startVoiceRecognition() {
                            VoiceRecognitionHandler voiceRecognitionHandler =
                                    mToolbarManager.getVoiceRecognitionHandler();
                            if (voiceRecognitionHandler == null) return;
                            voiceRecognitionHandler.startVoiceRecognition(
                                    VoiceInteractionSource.TOOLBAR);
                        }
                    };
            TranslateToolbarButtonController translateToolbarButtonController =
                    new TranslateToolbarButtonController(
                            mActivityTabProvider,
                            AppCompatResources.getDrawable(mActivity, R.drawable.ic_translate),
                            mActivity.getString(R.string.menu_translate),
                            trackerSupplier);
            VoiceToolbarButtonController voiceToolbarButtonController =
                    new VoiceToolbarButtonController(
                            mActivity,
                            AppCompatResources.getDrawable(mActivity, R.drawable.ic_mic_white_24dp),
                            mActivityTabProvider,
                            trackerSupplier,
                            mModalDialogManagerSupplier.get(),
                            voiceSearchDelegate);
            OptionalNewTabButtonController newTabButtonController =
                    new OptionalNewTabButtonController(
                            mActivity,
                            AppCompatResources.getDrawable(mActivity, R.drawable.new_tab_icon),
                            mActivityLifecycleDispatcher,
                            mTabCreatorManagerSupplier,
                            mActivityTabProvider,
                            trackerSupplier);
            AddToBookmarksToolbarButtonController addToBookmarksToolbarButtonController =
                    new AddToBookmarksToolbarButtonController(
                            mActivityTabProvider,
                            mActivity,
                            mActivityLifecycleDispatcher,
                            mTabBookmarkerSupplier,
                            trackerSupplier,
                            mBookmarkModelSupplier);
            AdaptiveToolbarButtonController adaptiveToolbarButtonController =
                    new AdaptiveToolbarButtonController(
                            mActivity,
                            mActivityLifecycleDispatcher,
                            mProfileSupplier,
                            new AdaptiveButtonActionMenuCoordinator(),
                            mWindowAndroid);
            PageSummaryButtonController pageSummaryButtonController =
                    new PageSummaryButtonController(
                            mActivity,
                            mBottomSheetController,
                            mModalDialogManagerSupplier.get(),
                            mActivityTabProvider,
                            PageInfoSharingControllerImpl.getInstance());

            if (ChromeFeatureList.sEnableDiscountInfoApi.isEnabled()) {
                DiscountsButtonController discountsButtonController =
                        new DiscountsButtonController(
                                mActivity,
                                mActivityTabProvider,
                                mModalDialogManagerSupplier.get(),
                                this::getCommerceBottomSheetContentController);
                adaptiveToolbarButtonController.addButtonVariant(
                        AdaptiveToolbarButtonVariant.DISCOUNTS, discountsButtonController);
            }

            adaptiveToolbarButtonController.addButtonVariant(
                    AdaptiveToolbarButtonVariant.NEW_TAB, newTabButtonController);
            adaptiveToolbarButtonController.addButtonVariant(
                    AdaptiveToolbarButtonVariant.SHARE, shareButtonController);
            adaptiveToolbarButtonController.addButtonVariant(
                    AdaptiveToolbarButtonVariant.VOICE, voiceToolbarButtonController);
            adaptiveToolbarButtonController.addButtonVariant(
                    AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS,
                    addToBookmarksToolbarButtonController);
            adaptiveToolbarButtonController.addButtonVariant(
                    AdaptiveToolbarButtonVariant.TRANSLATE, translateToolbarButtonController);
            adaptiveToolbarButtonController.addButtonVariant(
                    AdaptiveToolbarButtonVariant.PRICE_INSIGHTS, priceInsightsButtonController);
            adaptiveToolbarButtonController.addButtonVariant(
                    AdaptiveToolbarButtonVariant.PRICE_TRACKING, priceTrackingButtonController);
            adaptiveToolbarButtonController.addButtonVariant(
                    AdaptiveToolbarButtonVariant.READER_MODE, readerModeToolbarButtonController);
            adaptiveToolbarButtonController.addButtonVariant(
                    AdaptiveToolbarButtonVariant.READ_ALOUD, readAloudButtonController);
            adaptiveToolbarButtonController.addButtonVariant(
                    AdaptiveToolbarButtonVariant.PAGE_SUMMARY, pageSummaryButtonController);
            mContextualPageActionController =
                    new ContextualPageActionController(
                            mProfileSupplier,
                            mActivityTabProvider,
                            adaptiveToolbarButtonController,
                            () -> ShoppingServiceFactory.getForProfile(mProfileSupplier.get()),
                            mBookmarkModelSupplier);
            mButtonDataProviders =
                    Arrays.asList(mIdentityDiscController, adaptiveToolbarButtonController);

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
                                new QuickDeleteController(
                                        mActivity,
                                        new QuickDeleteDelegateImpl(
                                                mProfileSupplier, mTabSwitcherSupplier),
                                        mModalDialogManagerSupplier.get(),
                                        mSnackbarManagerSupplier.get(),
                                        mLayoutManager,
                                        mTabModelSelectorSupplier.get(),
                                        ArchivedTabModelOrchestrator.getForProfile(
                                                        mProfileSupplier.get())
                                                .getTabModelSelector());
                            });

            mToolbarManager =
                    new ToolbarManager(
                            mActivity,
                            mBottomControlsStacker,
                            mBrowserControlsManager,
                            mFullscreenManager,
                            mEdgeToEdgeControllerSupplier,
                            toolbarContainer,
                            mCompositorViewHolderSupplier.get(),
                            urlFocusChangedCallback,
                            mTopUiThemeColorProvider,
                            mTabObscuringHandlerSupplier.get(),
                            mShareDelegateSupplier,
                            mButtonDataProviders,
                            mActivityTabProvider,
                            mScrimCoordinator,
                            mActionModeControllerCallback,
                            mFindToolbarManager,
                            mProfileSupplier,
                            mBookmarkModelSupplier,
                            mCanAnimateBrowserControls,
                            mLayoutStateProviderOneShotSupplier,
                            mAppMenuSupplier,
                            canShowMenuUpdateBadge(),
                            mTabModelSelectorSupplier,
                            mOmniboxFocusStateSupplier,
                            mPromoShownOneshotSupplier,
                            mWindowAndroid,
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
                            mOverviewColorSupplier,
                            mBaseChromeLayout,
                            mReadAloudControllerSupplier,
                            getDesktopWindowStateProvider());
            if (!mSupportsAppMenuSupplier.getAsBoolean()) {
                mToolbarManager.getToolbar().disableMenuButton();
            }

            VoiceRecognitionHandler voiceRecognitionHandler =
                    mToolbarManager.getVoiceRecognitionHandler();
            if (voiceRecognitionHandler != null) {
                mMicStateObserver = voiceToolbarButtonController::updateMicButtonState;
                voiceRecognitionHandler.addObserver(mMicStateObserver);
            }
            mToolbarManagerOneshotSupplier.set(mToolbarManager);
        }
    }

    @Nullable
    private CommerceBottomSheetContentController getCommerceBottomSheetContentController() {
        if (mCommerceBottomSheetContentCoordinator == null
                && ChromeFeatureList.sEnableDiscountInfoApi.isEnabled()) {
            mCommerceBottomSheetContentCoordinator =
                    new CommerceBottomSheetContentCoordinator(mActivity, mBottomSheetController);
        }

        return mCommerceBottomSheetContentCoordinator;
    }

    /**
     * Gives concrete implementation of {@link ScrimCoordinator.SystemUiScrimDelegate} and
     * constructs {@link ScrimCoordinator}.
     */
    protected ScrimCoordinator buildScrimWidget() {
        ViewGroup coordinator = mActivity.findViewById(R.id.coordinator);
        ScrimCoordinator.SystemUiScrimDelegate delegate =
                new ScrimCoordinator.SystemUiScrimDelegate() {
                    @Override
                    public void setStatusBarScrimFraction(float scrimFraction) {
                        RootUiCoordinator.this.setStatusBarScrimFraction(scrimFraction);
                    }

                    @Override
                    public void setNavigationBarScrimFraction(float scrimFraction) {}
                };
        return new ScrimCoordinator(
                mActivity,
                delegate,
                coordinator,
                coordinator.getContext().getColor(R.color.omnibox_focused_fading_background_color));
    }

    protected void setStatusBarScrimFraction(float scrimFraction) {
        mStatusBarColorController.setStatusBarScrimFraction(scrimFraction);
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
                            if (mContextualSearchManagerSupplier.get() != null) {
                                mContextualSearchManagerSupplier.get().dismissContextualSearchBar();
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
                            mWindowAndroid);
            AppMenuCoordinatorFactory.setExceptionReporter(
                    ChromePureJavaExceptionReporter::reportJavaException);

            mAppMenuCoordinator.registerAppMenuBlocker(this);
            mAppMenuCoordinator.registerAppMenuBlocker(mAppMenuBlocker);

            mAppMenuSupplier.set(mAppMenuCoordinator);

            mAppMenuObserver =
                    new AppMenuObserver() {
                        @Override
                        public void onMenuVisibilityChanged(boolean isVisible) {
                            if (isVisible && mPageZoomCoordinator != null) {
                                // On show app menu, hide page zoom dialog
                                mPageZoomCoordinator.hide();
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

    private void initFindToolbarManager() {
        if (!mSupportsFindInPageSupplier.getAsBoolean()) return;

        int stubId = R.id.find_toolbar_stub;
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            stubId = R.id.find_toolbar_tablet_stub;
        }
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
        if (mContextualSearchManagerSupplier.get() != null) {
            mContextualSearchManagerSupplier
                    .get()
                    .hideContextualSearch(OverlayPanel.StateChangeReason.UNKNOWN);
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
                                    mWindowAndroid);
                };

        Supplier<OverlayPanelManager> panelManagerSupplier =
                () -> {
                    if (mCompositorViewHolderSupplier.get() != null
                            && mCompositorViewHolderSupplier.get().getLayoutManager() != null) {
                        return mCompositorViewHolderSupplier
                                .get()
                                .getLayoutManager()
                                .getOverlayPanelManager();
                    }
                    return null;
                };

        // TODO(crbug.com/40135255): Initialize after inflation so we don't need to pass in view
        // suppliers.
        mBottomSheetController =
                BottomSheetControllerFactory.createBottomSheetController(
                        () -> mScrimCoordinator,
                        sheetInitializedCallback,
                        mActivity.getWindow(),
                        mWindowAndroid.getKeyboardDelegate(),
                        () -> mActivity.findViewById(R.id.sheet_container),
                        () -> {
                            return mEdgeToEdgeControllerSupplier.get() == null
                                    ? 0
                                    : mEdgeToEdgeControllerSupplier.get().getBottomInset();
                        },
                        getDesktopWindowStateProvider());
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
        if (BackPressManager.isEnabled()) {
            assert mBackPressManager != null
                    && !mBackPressManager.has(BackPressHandler.Type.BOTTOM_SHEET);
            BackPressHandler mBottomSheetBackPressHandler =
                    mBottomSheetController.getBottomSheetBackPressHandler();
            if (mBottomSheetBackPressHandler != null) {
                mBackPressManager.addHandler(
                        mBottomSheetBackPressHandler, BackPressHandler.Type.BOTTOM_SHEET);
            }
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
            mEdgeToEdgeController =
                    EdgeToEdgeControllerFactory.create(
                            mActivity,
                            mWindowAndroid,
                            mActivityTabProvider,
                            mBrowserControlsManager,
                            mLayoutManagerSupplier,
                            mFullscreenManager);
            mEdgeToEdgeControllerSupplier.set(mEdgeToEdgeController);

            if (EdgeToEdgeUtils.isEdgeToEdgeBottomChinEnabled()) {
                mEdgeToEdgeBottomChin = createEdgeToEdgeBottomChin();
            }
        }
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

    /**
     * @return The {@link ScrimCoordinator} to control activity's primary scrim.
     */
    public ScrimCoordinator getScrimCoordinator() {
        return mScrimCoordinator;
    }

    /** @return The {@link SnackbarManager} for the {@link BottomSheetController}. */
    public SnackbarManager getBottomSheetSnackbarManager() {
        return mBottomSheetSnackbarManager;
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
                                mPageZoomCoordinator.hide();
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
                            if (isShowing && mPageZoomCoordinator != null) {
                                // On show snackbar, hide page zoom dialog
                                mPageZoomCoordinator.hide();
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
                        mPageZoomCoordinator.onBottomControlsHeightChanged(bottomControlsHeight);
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

    /**
     * Saves relevant information that will be used to restore the UI state after the activity is
     * recreated. This is expected to be invoked in {@code Activity#onSaveInstanceState(Bundle)}.
     *
     * @param outState The {@link Bundle} that is used to save state information.
     * @param isRecreatingForTabletModeChange Whether the activity is recreated due to a fold
     *     configuration change. {@code true} if the fold configuration changed, {@code false}
     *     otherwise.
     */
    public void onSaveInstanceState(Bundle outState, boolean isRecreatingForTabletModeChange) {
        assert mTabModelSelectorSupplier.hasValue();
        mFoldTransitionController.saveUiState(
                outState,
                isRecreatingForTabletModeChange,
                mTabModelSelectorSupplier.get().isIncognitoSelected());
    }

    /**
     * Restores the relevant UI state when the activity is recreated on a device fold transition.
     *
     * @param savedInstanceState The {@link Bundle} that is used to restore the UI state.
     */
    public void restoreUiState(Bundle savedInstanceState) {
        mFoldTransitionController.restoreUiState(savedInstanceState);
    }

    private void attemptToShowRestoreTabsPromo() {
        if (mRestoreTabsFeatureHelper == null) {
            mRestoreTabsFeatureHelper = new RestoreTabsFeatureHelper();
        }

        Supplier<Integer> gtsTabListModelSizeSupplier =
                () -> {
                    if (mTabSwitcherSupplier.get() != null) {
                        return mTabSwitcherSupplier.get().getTabSwitcherTabListModelSize();
                    }
                    return 0;
                };

        Callback<Integer> scrollGTSToRestoredTabsCallback =
                (tabListModelSize) -> {
                    if (mTabSwitcherSupplier.get() != null) {
                        mTabSwitcherSupplier
                                .get()
                                .setTabSwitcherRecyclerViewPosition(
                                        new RecyclerViewPosition(tabListModelSize, 0));
                    }
                };

        mRestoreTabsFeatureHelper.maybeShowPromo(
                mActivity,
                mProfileSupplier.get(),
                mTabCreatorManagerSupplier.get(),
                getBottomSheetController(),
                gtsTabListModelSizeSupplier,
                scrollGTSToRestoredTabsCallback);
    }

    private void initBoardingPassDetector() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.BOARDING_PASS_DETECTOR)) {
            mBoardingPassController = new BoardingPassController(mActivityTabProvider);
        }
    }

    // Testing methods

    public AppMenuCoordinator getAppMenuCoordinatorForTesting() {
        return mAppMenuCoordinator;
    }

    public ScrimCoordinator getScrimCoordinatorForTesting() {
        return mScrimCoordinator;
    }

    public void destroyActivityForTesting() {
        // Actually destroying or finishing the activity hinders the shutdown process after
        // a test is done. Just null it out to give an effect of |onDestroy| being invoked.
        mActivity = null;
    }

    public BottomControlsStacker getBottomControlsStackerForTesting() {
        return mBottomControlsStacker;
    }

    public DataSharingTabManager getDataSharingTabManager() {
        // This should only be called on an instance of TabbedRootUiCoordinator.
        return null;
    }
}
