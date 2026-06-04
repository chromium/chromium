// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS;

import android.app.Activity;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.os.Looper;
import android.transition.ChangeBounds;
import android.transition.ChangeTransform;
import android.transition.Fade;
import android.transition.Transition;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.view.View.OnClickListener;
import android.view.View.OnLayoutChangeListener;
import android.view.View.OnLongClickListener;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.activity.BackEventCompat;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AppCompatActivity;
import androidx.coordinatorlayout.widget.CoordinatorLayout;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.ContextUtils;
import org.chromium.base.JavaExceptionReporter;
import org.chromium.base.TimeUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.ValueChangedCallback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.back_press.BackPressMetrics;
import org.chromium.chrome.browser.back_press.BackPressMetrics.NavigationDirection;
import org.chromium.chrome.browser.back_press.BackPressMetrics.PredictiveGestureNavPhase;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkModelObserver;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.bottombar.BottomBarContainerCoordinator;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerType;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.dom_distiller.DomDistillerTabUtils;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.findinpage.FindToolbarManager;
import org.chromium.chrome.browser.findinpage.FindToolbarObserver;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.gesturenav.GestureNavigationUtils;
import org.chromium.chrome.browser.gesturenav.OverscrollGlowCoordinator;
import org.chromium.chrome.browser.gesturenav.TabOnBackGestureHandler;
import org.chromium.chrome.browser.glic.GlicButtonDelegate;
import org.chromium.chrome.browser.history.HistoryManagerUtils;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.homepage.HomepageManager.HomepageStateListener;
import org.chromium.chrome.browser.homepage.HomepagePolicyManager;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponent;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponentSupplier;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustSignalsCoordinator;
import org.chromium.chrome.browser.merchant_viewer.PageInfoStoreInfoController.StoreInfoActionHandler;
import org.chromium.chrome.browser.metrics.UmaActivityObserver;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.ntp.IncognitoNewTabPage;
import org.chromium.chrome.browser.ntp.IncognitoNtpOmniboxAutofocusManager;
import org.chromium.chrome.browser.ntp.IncognitoNtpUtils;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.offlinepages.OfflinePageTabData;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.omnibox.BackKeyBehaviorDelegate;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.LocationBarEmbedderUiOverrides;
import org.chromium.chrome.browser.omnibox.LocationBarFocusScrimHandler;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.OmniboxChipManager;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.OverrideUrlLoadingDelegateImpl;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.omnibox.status.SiteControlsIphController;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownScrollListener;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxActionDelegateImpl;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.page_info.ChromePageInfo;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfCoordinator;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.LoadUrlResult;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.chrome.browser.tab.TabFavicon;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabModelDotInfo;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.OverridableTabCount;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorUtil;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabstrip.TabStripTopControlLayer;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupUi;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupUiOneshotSupplier;
import org.chromium.chrome.browser.theme.BottomUiThemeColorProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider.ThemeColorObserver;
import org.chromium.chrome.browser.theme.ThemeColorProvider.TintObserver;
import org.chromium.chrome.browser.theme.ToolbarThemeColorProvider;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.back_button.BackButtonCoordinator;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsContentDelegate;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator;
import org.chromium.chrome.browser.toolbar.bottom.ScrollingBottomViewResourceFrameLayout;
import org.chromium.chrome.browser.toolbar.extensions.ExtensionsToolbarCoordinator;
import org.chromium.chrome.browser.toolbar.forward_button.ForwardButtonCoordinator;
import org.chromium.chrome.browser.toolbar.home_button.HomeButtonCoordinator;
import org.chromium.chrome.browser.toolbar.load_progress.LoadProgressCoordinator;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButton;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator.VisibilityDelegate;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonState;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonDataProvider;
import org.chromium.chrome.browser.toolbar.top.ActionModeController;
import org.chromium.chrome.browser.toolbar.top.ActionModeController.ActionBarDelegate;
import org.chromium.chrome.browser.toolbar.top.NavigationPopup;
import org.chromium.chrome.browser.toolbar.top.NavigationPopup.HistoryDelegate;
import org.chromium.chrome.browser.toolbar.top.OptionalBrowsingModeButtonController;
import org.chromium.chrome.browser.toolbar.top.TabSwitcherActionMenuCoordinator;
import org.chromium.chrome.browser.toolbar.top.ToggleTabStackButton;
import org.chromium.chrome.browser.toolbar.top.ToggleTabStackButtonCoordinator;
import org.chromium.chrome.browser.toolbar.top.Toolbar;
import org.chromium.chrome.browser.toolbar.top.ToolbarActionModeCallback;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer;
import org.chromium.chrome.browser.toolbar.top.ToolbarLayout;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone;
import org.chromium.chrome.browser.toolbar.top.ToolbarTablet;
import org.chromium.chrome.browser.toolbar.top.ToolbarUtils.ToolbarComponentId;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator;
import org.chromium.chrome.browser.toolbar.top.ViewShiftingActionBarDelegate;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator.TabStripTransitionDelegate;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.chrome.browser.ui.actions.glic.GlicActionCoordinator;
import org.chromium.chrome.browser.ui.actions.tabswitcher.TabSwitcherActionProvider;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuDelegate;
import org.chromium.chrome.browser.ui.appmenu.MenuButtonDelegate;
import org.chromium.chrome.browser.ui.bottombar.BottomBarConfigUtils;
import org.chromium.chrome.browser.ui.bottombar.BottomBarHostManager;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.TopInsetProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;
import org.chromium.chrome.browser.ui.side_ui.SideUiObserver;
import org.chromium.chrome.browser.ui.side_ui.SideUiStateProvider;
import org.chromium.chrome.browser.ui.side_ui.ViewMarginAdjusterForSideUi;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoBarThrottle;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;
import org.chromium.components.browser_ui.accessibility.PageZoomManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncherSupplier;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.IncognitoColors;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.OmniboxFocusReason;
import org.chromium.components.page_info.PageInfoController.OpenedFromSource;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.back_forward_transition.AnimationStage;
import org.chromium.content_public.browser.selection.SelectionDropdownMenuDelegate;
import org.chromium.net.NetError;
import org.chromium.ui.UiUtils;
import org.chromium.ui.animation.transition.ShrinkTransition;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.BackGestureEventSwipeEdge;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.resources.Resource;
import org.chromium.ui.resources.dynamics.DynamicResourceSnapshot;
import org.chromium.ui.util.KeyEventUtils;
import org.chromium.ui.util.MotionEventUtils;
import org.chromium.ui.util.TokenHolder;
import org.chromium.ui.widget.ChromeImageButton;
import org.chromium.ui.widget.ViewRectProvider;
import org.chromium.url.GURL;

import java.util.List;
import java.util.Set;
import java.util.function.Supplier;

/**
 * Contains logic for managing the toolbar visual component. This class manages the interactions
 * with the rest of the application to ensure the toolbar is always visually up to date.
 */
@NullMarked
@SuppressWarnings("NullAway") // TODO(467389696): Remove after ObservableSupplierImpl migration.
public class ToolbarManager
        implements UrlFocusChangeListener,
                ThemeColorObserver,
                TintObserver,
                MenuButtonDelegate,
                TabObscuringHandler.Observer {
    private final IncognitoStateProvider mIncognitoStateProvider;
    private final ToolbarThemeColorProvider mToolbarThemeColorProvider;
    private final @Nullable ToolbarThemeColorProvider mAdjustedToolbarThemeColorProvider;
    private final MonotonicObservableSupplier<EphemeralTabCoordinator>
            mEphemeralTabCoordinatorSupplier;
    private AppThemeColorProvider mAppThemeColorProvider;
    private final BottomUiThemeColorProvider mBottomUiThemeColorProvider;
    private final @Nullable ActionRegistry mActionRegistry;
    private final SettableThemeColorProvider mCustomTabThemeColorProvider;
    private final TopToolbarCoordinator mToolbar;
    private final ToolbarLayout mToolbarLayout;
    private final ToolbarControlContainer mControlContainer;
    private final View mToolbarHairline;
    private final BrowserControlsStateProvider.Observer mBrowserControlsObserver;
    private final FullscreenManager.Observer mFullscreenObserver;
    private final SettableNonNullObservableSupplier<Boolean> mHomepageEnabledSupplier =
            ObservableSuppliers.createNonNull(false);
    private final NonNullObservableSupplier<Boolean> mOmniboxFocusStateSupplier;
    private final SettableNonNullObservableSupplier<Boolean> mIsNtpWithFakeboxShowingSupplier =
            ObservableSuppliers.createNonNull(false);
    private final SettableNonNullObservableSupplier<Boolean> mIsIncognitoNtpShowingSupplier =
            ObservableSuppliers.createNonNull(false);
    private final SettableNonNullObservableSupplier<Boolean> mFindInPageShowingSupplier =
            ObservableSuppliers.createNonNull(false);
    private final SettableNonNullObservableSupplier<Boolean> mIsTabSwitcherFinishedShowingSupplier =
            ObservableSuppliers.createNonNull(false);
    private final SettableNullableObservableSupplier<Tab> mCurrentTabSupplier =
            ObservableSuppliers.createNullable();

    @SuppressWarnings("NullAway") // https://github.com/uber/NullAway/issues/1373
    private final SettableNullableObservableSupplier<@BrowserControlsState Integer>
            mConstraintsSupplier =
                    mCurrentTabSupplier.createTransitiveNullable(
                            tab ->
                                    tab == null
                                            ? null
                                            : TabBrowserControlsConstraintsHelper
                                                    .getObservableConstraints(tab));

    private SettableMonotonicObservableSupplier<BottomControlsCoordinator>
            mTabGroupUiBottomControlsCoordinatorSupplier = ObservableSuppliers.createMonotonic();
    private SettableMonotonicObservableSupplier<BottomControlsCoordinator>
            mBottomAppBarCoordinatorSupplier = ObservableSuppliers.createMonotonic();
    private final SettableNonNullObservableSupplier<Boolean> mSuppressToolbarSceneLayerSupplier =
            ObservableSuppliers.createNonNull(false);
    private final SettableMonotonicObservableSupplier<Long> mCaptureResourceIdSupplier =
            ObservableSuppliers.createMonotonic();
    private @MonotonicNonNull TabModelSelector mTabModelSelector;
    private final Callback<TabModel> mCurrentTabModelObserver;
    private MonotonicObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private ActivityTabProvider.ActivityTabTabObserver mActivityTabTabObserver;
    private final ActivityTabProvider mActivityTabProvider;
    private final ActivityResultTracker mActivityResultTracker;
    private final SnackbarManager mSnackbarManager;
    private final LocationBarModel mLocationBarModel;
    private NullableObservableSupplier<BookmarkModel> mBookmarkModelSupplier;
    private final ValueChangedCallback<@Nullable BookmarkModel> mBookmarkModelSupplierObserver =
            new ValueChangedCallback<>(this::setBookmarkModel);
    private final ToolbarIphController mIphController;
    private @Nullable SiteControlsIphController mSiteControlsIphController;
    private @MonotonicNonNull TemplateUrlService mTemplateUrlService;
    private @MonotonicNonNull TemplateUrlServiceObserver mTemplateUrlObserver;
    private LocationBar mLocationBar;
    private final OneshotSupplierImpl<OmniboxStub> mOmniboxStubSupplier =
            new OneshotSupplierImpl<>();
    private final Supplier<LocationBar> mLocationBarSupplier = () -> mLocationBar;
    private FindToolbarManager mFindToolbarManager;

    private @MonotonicNonNull LayoutManagerImpl mLayoutManager;

    private final BookmarkModelObserver mBookmarksObserver;
    private final FindToolbarObserver mFindToolbarObserver;

    private @MonotonicNonNull LayoutStateProvider mLayoutStateProvider;
    private final LayoutStateProvider.LayoutStateObserver mLayoutStateObserver;
    private OneshotSupplier<LayoutStateProvider> mLayoutStateProviderSupplier;
    private CallbackController mCallbackController = new CallbackController();

    private final ActionBarDelegate mActionBarDelegate;
    private final ActionModeController mActionModeController;
    private final Callback<Boolean> mUrlFocusChangedCallback;
    private final Handler mHandler = new Handler();
    private final AppCompatActivity mActivity;
    private final WindowAndroid mWindowAndroid;
    private final AppMenuDelegate mAppMenuDelegate;
    private final CompositorViewHolder mCompositorViewHolder;
    private final BottomControlsStacker mBottomControlsStacker;
    private final TopControlsStacker mTopControlsStacker;
    private final BrowserControlsSizer mBrowserControlsSizer;
    private final FullscreenManager mFullscreenManager;
    private final MonotonicObservableSupplier<EdgeToEdgeController> mEdgeToEdgeControllerSupplier;
    private final LocationBarFocusScrimHandler mLocationBarFocusHandler;
    private ComponentCallbacks mComponentCallbacks;
    private final LoadProgressCoordinator mProgressBarCoordinator;
    private final ToolbarTabControllerImpl mToolbarTabController;
    private MenuButtonCoordinator mMenuButtonCoordinator;
    private MenuButtonCoordinator mOverviewModeMenuButtonCoordinator;
    private @Nullable HomepageStateListener mHomepageStateListener;
    private final NonNullObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final StatusBarColorController mStatusBarColorController;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final BottomSheetController mBottomSheetController;
    private final @Nullable DataSharingTabManager mDataSharingTabManager;
    private final TabContentManager mTabContentManager;
    private final TabCreatorManager mTabCreatorManager;
    private final TabObscuringHandler mTabObscuringHandler;
    private final Supplier<ShareDelegate> mShareDelegateSupplier;
    private @MonotonicNonNull OnAttachStateChangeListener mAttachStateChangeListener;
    private final @Nullable BackPressHandler mBackPressHandler;
    private final @Nullable BackPressManager mBackPressManager;
    private final UserEducationHelper mUserEducationHelper;
    private final ToolbarLongPressMenuHandler mToolbarLongPressMenuHandler;
    private final OverrideUrlLoadingDelegateImpl mOverrideUrlLoadingDelegate;
    private final TopInsetProvider mTopInsetProvider;
    private final SettableNonNullObservableSupplier<@ControlsPosition Integer>
            mToolbarPositionSupplier = ObservableSuppliers.createNonNull(ControlsPosition.TOP);
    private final OneshotSupplier<ChromeAndroidTask> mChromeAndroidTaskSupplier;

    private @MonotonicNonNull HomeButtonCoordinator mHomeButtonCoordinator;
    private @MonotonicNonNull ToggleTabStackButtonCoordinator mTabSwitcherButtonCoordinator;
    private @MonotonicNonNull TabSwitcherActionProvider mTabSwitcherActionProvider;
    private @MonotonicNonNull BackButtonCoordinator mBackButtonCoordinator;
    private @MonotonicNonNull ForwardButtonCoordinator mForwardButtonCoordinator;
    private @Nullable ExtensionsToolbarCoordinator mExtensionsToolbarCoordinator;

    private final BrowserStateBrowserControlsVisibilityDelegate mControlsVisibilityDelegate;
    private int mFullscreenFocusToken = TokenHolder.INVALID_TOKEN;
    private int mFullscreenFindInPageToken = TokenHolder.INVALID_TOKEN;

    private boolean mInitializedWithNative;
    private @Nullable Runnable mOnInitializedRunnable;
    private @Nullable Runnable mMenuStateObserver;
    private @MonotonicNonNull UpdateMenuItemHelper mUpdateMenuItemHelper;

    private boolean mShouldUpdateToolbarPrimaryColor = true;
    private int mCurrentThemeColor;

    private int mCurrentOrientation;

    private final ScrimManager mScrimManager;

    private final OneshotSupplier<Boolean> mPromoShownOneshotSupplier;
    private final GlicButtonDelegate mToggleGlicCallback;
    private @Nullable GlicActionCoordinator mGlicActionCoordinator;

    private final TabStripTopControlLayer mTabStripTopControlLayer;
    private final @Nullable DesktopWindowStateManager mDesktopWindowStateManager;
    private final OneshotSupplierImpl<TabStripTransitionDelegate>
            mTabStripTransitionDelegateSupplier = new OneshotSupplierImpl<>();

    private @Nullable TabGroupUiOneshotSupplier mTabGroupUiOneshotSupplier;

    private @Nullable SideUiStateProvider mSideUiStateProvider;
    private @Nullable SideUiObserver mSideUiObserver;
    private @Nullable SideUiObserver mControlContainerSideUiObserver;
    private @Nullable SideUiObserver mProgressBarSideUiObserver;

    private final MonotonicObservableSupplier<TabBookmarker> mTabBookmarkerSupplier;
    private final SettableNonNullObservableSupplier<Boolean> mBackPressStateSupplier =
            ObservableSuppliers.createNonNull(false);

    private final SettableNonNullObservableSupplier<Boolean> mToolbarNavControlsEnabledSupplier =
            ObservableSuppliers.createNonNull(true);

    private boolean mIsDestroyed;

    private final boolean mIsCustomTab;
    private final boolean mIsTablet;

    private final MonotonicObservableSupplier<ReadAloudController> mReadAloudControllerSupplier;
    private final Runnable mReadAloudReadabilityCallback = this::onReadAloudReadabilityUpdated;

    private boolean mBackGestureInProgress;
    private boolean mStartNavDuringOngoingGesture;
    private final WindowAndroid.ProgressBarConfig.Provider mProgressBarConfigProvider;
    // Supplier of the offset, relative to the bottom of the viewport, of the bottom-anchored
    // toolbar. This value is only meaningful when the current ControlsPosition is BOTTOM.
    private final SettableNonNullObservableSupplier<Integer> mBottomToolbarControlsOffsetSupplier =
            ObservableSuppliers.createNonNull(0);
    private final FormFieldFocusedSupplier mFormFieldFocusedSupplier =
            new FormFieldFocusedSupplier();
    private final View mProgressBarContainer;
    private final ToolbarProgressBarLayer mToolbarProgressBarLayer;
    private @Nullable Supplier<Integer> mBookmarkBarHeightSupplier;
    private boolean mInTabSwitcherTransition;

    private @Nullable Tab mLastTab;

    private @Nullable StripLayoutHelperManager mStripLayoutHelperManager;
    private @Nullable MiniOriginBarController mMiniOriginBarController;
    private @Nullable ToolbarPositionController mToolbarPositionController;
    private @Nullable UndoBarThrottle mUndoBarThrottle;
    private @Nullable IncognitoNtpOmniboxAutofocusManager mIncognitoNtpOmniboxAutofocusManager;
    private final @Nullable BottomBarHostManager mBottomBarHostManager;

    private OverridableTabCount mOverridableTabCount;
    private int mIncognitoNtpViewIdForA11y = View.NO_ID;
    private @Nullable OverscrollGlowCoordinator mOverscrollGlowCoordinator;
    private final NullableObservableSupplier<Profile> mProfileSupplier;
    private final Callback<Boolean> mOnXrSpaceModeChanged = this::onXrSpaceModeChanged;
    private final NonNullObservableSupplier<Boolean> mXrSpaceModeObservableSupplier;
    private final SettableNonNullObservableSupplier<Float>
            mNtpSearchBoxTransitionPercentageSupplier = ObservableSuppliers.createNonNull(0f);

    private static class TabObscuringCallback implements Callback<Boolean> {
        private final TabObscuringHandler mTabObscuringHandler;

        /** A token held while the toolbar/omnibox is obscuring all visible tabs. */
        private TabObscuringHandler.@Nullable Token mTabObscuringToken;

        public TabObscuringCallback(TabObscuringHandler handler) {
            mTabObscuringHandler = handler;
        }

        @Override
        public void onResult(Boolean visible) {
            if (visible) {
                // It's possible for the scrim to unfocus and refocus without the
                // visibility actually changing. In this case we have to make sure we
                // unregister the previous token before acquiring a new one.
                TabObscuringHandler.Token oldToken = mTabObscuringToken;
                mTabObscuringToken =
                        mTabObscuringHandler.obscure(TabObscuringHandler.Target.TAB_CONTENT);
                if (oldToken != null) {
                    mTabObscuringHandler.unobscure(oldToken);
                }
            } else {
                if (mTabObscuringToken != null) {
                    mTabObscuringHandler.unobscure(mTabObscuringToken);
                    mTabObscuringToken = null;
                }
            }
        }
    }

    private class OnBackPressHandler implements BackPressHandler {
        private @Nullable TabOnBackGestureHandler mHandler;
        private boolean mIsGestureMode;
        private @BackGestureEventSwipeEdge int mInitialEdge;
        private boolean mIsInProgress;

        @Override
        public boolean invokeBackActionOnEscape() {
            // Escape key presses should not navigate back in tab history. We do not also implement
            // a custom {@link BackPressHandler#handleEscPress()} since we don't want anything to
            // happen and for the manager to move to the next priority handler.
            return false;
        }

        @Override
        public int handleBackPress() {
            mIsInProgress = false;
            if (mIsGestureMode && mBackGestureInProgress) {
                BackPressMetrics.recordNavStatusDuringGesture(
                        mStartNavDuringOngoingGesture, mActivity.getWindow());
                BackPressMetrics.recordPredictiveGestureNav(
                        mHandler != null, PredictiveGestureNavPhase.COMPLETED);
            }

            int res = BackPressResult.SUCCESS;

            if (mHandler != null) {
                assert mIsGestureMode : "Must be in gesture mode if transition handler is alive";
                mHandler.onBackInvoked(mIsGestureMode);
            } else {
                res = ToolbarManager.this.handleBackPress();
            }
            mBackGestureInProgress = false;
            mIsGestureMode = false;
            mHandler = null;
            return res;
        }

        @Override
        public NonNullObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
            return ToolbarManager.this.mBackPressStateSupplier;
        }

        @Override
        public void handleOnBackCancelled() {
            mIsInProgress = false;
            if (mIsGestureMode && mBackGestureInProgress) {
                BackPressMetrics.recordNavStatusDuringGesture(
                        mStartNavDuringOngoingGesture, mActivity.getWindow());
                BackPressMetrics.recordPredictiveGestureNav(
                        mHandler != null, PredictiveGestureNavPhase.CANCELLED);
            }
            if (mHandler != null) {
                mHandler.onBackCancelled(mIsGestureMode);
            }
            mBackGestureInProgress = false;
            mIsGestureMode = false;
            mHandler = null;
            if (mOverscrollGlowCoordinator != null && mOverscrollGlowCoordinator.isShowing()) {
                mOverscrollGlowCoordinator.releaseGlow();
            }
        }

        @Override
        public void handleOnBackProgressed(BackEventCompat backEvent) {
            if (mOverscrollGlowCoordinator != null && mOverscrollGlowCoordinator.isShowing()) {
                mOverscrollGlowCoordinator.pullGlow(backEvent.getTouchX(), backEvent.getTouchY());
            }
            if (mHandler == null) return;
            if (!mIsInProgress) {
                handleOnBackStarted(backEvent);
                return;
            }

            if (mInitialEdge != backEvent.getSwipeEdge()) {
                handleOnBackCancelled();
                handleOnBackStarted(backEvent);
                return;
            }

            mHandler.onBackProgressed(
                    backEvent.getProgress(),
                    backEvent.getSwipeEdge() == BackEventCompat.EDGE_LEFT
                            ? BackGestureEventSwipeEdge.LEFT
                            : BackGestureEventSwipeEdge.RIGHT,
                    isForward(),
                    mIsGestureMode);
        }

        @Override
        public void handleOnBackStarted(BackEventCompat backEvent) {
            if (mOverscrollGlowCoordinator != null && mOverscrollGlowCoordinator.isShowing()) {
                mOverscrollGlowCoordinator.resetGlow();
            }

            mIsInProgress = true;
            mIsGestureMode = UiUtils.isGestureNavigationMode(mActivity.getWindow());
            mInitialEdge = backEvent.getSwipeEdge();
            Tab tab = mActivityTabProvider.get();
            assert tab != null;
            // For 3-button mode, record metrics only when back is triggered by swiping.
            // See NavigationHandler.java.
            if (mIsGestureMode) {
                WebContents webContents = tab.getWebContents();
                assumeNonNull(webContents);
                BackPressMetrics.recordNavStatusOnGestureStart(
                        webContents.hasUncommittedNavigationInPrimaryMainFrame(),
                        mActivity.getWindow());
            }

            mStartNavDuringOngoingGesture = false;
            mBackGestureInProgress = true;

            assert tab.canGoBack()
                    : String.format(
                            "Should be able to navigate back; edge %s; gesture mode %s",
                            backEvent.getSwipeEdge(), mIsGestureMode);

            // This means the user is pressing a back button in 3-button mode.
            // The transition should only be triggered by swipe rather than a button press.
            // TODO(crbug.com/376306986): add some tests to ensure the this handler is not
            // initialized in 3-button mode.
            final boolean withTransition = shouldStartTransition(backEvent);
            if (withTransition) {
                // Always force to show the top control at the start of the gesture.
                TabBrowserControlsConstraintsHelper.update(
                        mLocationBarModel.getTab(),
                        BrowserControlsState.SHOWN,
                        /* animate= */ true);
                mHandler = TabOnBackGestureHandler.from(tab);
                mHandler.onBackStarted(
                        backEvent.getProgress(),
                        backEvent.getSwipeEdge() == BackEventCompat.EDGE_LEFT
                                ? BackGestureEventSwipeEdge.LEFT
                                : BackGestureEventSwipeEdge.RIGHT,
                        isForward(),
                        mIsGestureMode);
            }
            if (mIsGestureMode) {
                BackPressMetrics.recordPredictiveGestureNav(
                        withTransition, PredictiveGestureNavPhase.ACTIVATED);
            }
        }

        private boolean shouldStartTransition(BackEventCompat backEvent) {
            Tab tab = mActivityTabProvider.get();
            if (!mIsGestureMode || tab == null) return false;
            if (!GestureNavigationUtils.allowTransition(tab, false)) {
                return false;
            }

            final boolean navigateForward = isForward();
            final boolean navigable = navigateForward ? tab.canGoForward() : tab.canGoBack();
            return navigable
                    && TabOnBackGestureHandler.shouldAnimateNavigationTransition(
                            navigateForward, backEvent.getSwipeEdge());
        }
    }

    /**
     * Creates a ToolbarManager object.
     *
     * @param activity The Android activity.
     * @param controlsSizer The {@link BrowserControlsSizer} for the activity.
     * @param fullscreenManager The {@link FullscreenManager} for the activity.
     * @param edgeToEdgeControllerSupplier Supplies an {@link EdgeToEdgeController} needed for
     *     Bottom Controls Toolbar.
     * @param controlContainer The container of the toolbar.
     * @param compositorViewHolder Class that holds a {@link CompositorView}.
     * @param urlFocusChangedCallback The callback to be notified when the URL focus changes.
     * @param toolbarThemeColorProvider The ThemeColorProvider object for the toolbar.
     * @param adjustedToolbarThemeColorProvider The ThemeColorProvider object for top UI which may
     *     adjust tint colors.
     * @param tabObscuringHandler Delegate object handling obscuring views.
     * @param shareDelegateSupplier Supplier for ShareDelegate.
     * @param buttonDataProviders The list of button data providers for the optional toolbar button
     *     in the browsing mode toolbar, given in precedence order.
     * @param tabProvider The {@link ActivityTabProvider} for accessing current activity tab.
     * @param scrimManager A means of showing the scrim.
     * @param toolbarActionModeCallback Callback that communicates changes in the conceptual mode of
     *     toolbar interaction.
     * @param findToolbarManager The manager for the find in page function.
     * @param profileSupplier Supplier of the currently applicable profile.
     * @param bookmarkModelSupplier Supplier of the bookmark bridge for the current profile.
     *     TODO(crbug.com/40131776): Use OneShotSupplier once it is ready.
     * @param layoutStateProviderSupplier Supplier of the {@link LayoutStateProvider}.
     * @param appMenuCoordinatorSupplier Supplier of the {@link AppMenuCoordinator}.
     * @param canShowUpdateBadge Whether the update Chrome badge can be shown on the app menu.
     * @param tabModelSelectorSupplier Supplier of the {@link TabModelSelector}.
     * @param omniboxFocusStateSupplier Supplier to access the focus state of the omnibox.
     * @param promoShownOneshotSupplier Supplier for whether a promo was shown on startup.
     * @param windowAndroid The {@link WindowAndroid} associated with the ToolbarManager.
     * @param chromeAndroidTaskSupplier Supplier of the {@link ChromeAndroidTask}.
     * @param isInOverviewModeSupplier Supplies whether the app is currently in overview mode.
     * @param modalDialogManagerSupplier Supplies the {@link ModalDialogManager}.
     * @param statusBarColorController The {@link StatusBarColorController} for the app.
     * @param appMenuDelegate Allows interacting with the app menu.
     * @param activityLifecycleDispatcher Allows monitoring the activity lifecycle.
     * @param bottomSheetController Controls the state of the bottom sheet.
     * @param dataSharingTabManager The {@link} DataSharingTabManager managing communication between
     *     UI and DataSharing services.
     * @param tabContentManager Manages the content of tabs.
     * @param tabCreatorManager Manages the creation of tabs.
     * @param merchantTrustSignalsCoordinatorSupplier Supplier of {@link
     *     MerchantTrustSignalsCoordinator}.
     * @param ephemeralTabCoordinatorSupplier Supplies the {@link EphemeralTabCoordinator}.
     * @param initializeWithIncognitoColors Whether the toolbar should be initialized with incognito
     * @param backPressManager The {@link BackPressManager} handling back press gesture.
     * @param desktopWindowStateManager The {@link DesktopWindowStateManager} instance.
     * @param lockTopControlsTokenJar {@link TokenHolder} from TopControlsLockCoordinator, used to
     *     ensure control lock state does not change.
     * @param tabBookmarkerSupplier Supplier of {@link TabBookmarker} for bookmarking a given tab.
     * @param menuButtonVisibilityDelegate Delegate for handling the visibility of the menu button.
     * @param topControlsStacker TopControlsStacker to manage the view's y-offset.
     * @param topInsetProvider The {@link TopInsetProvider} instance.
     * @param xrSpaceModeObservableSupplier Supplies current XR space mode status. True for XR full
     *     space mode, false otherwise.
     * @param pageZoomManager The {@link PageZoomManager} used to manage the page zoom.
     * @param omniboxChipManager The {@link OmniboxChipManager} to show chips in the omnibox.
     * @param bottomBarHostManager The {@link BottomBarHostManager} to manage the bottom bar.
     */
    public ToolbarManager(
            AppCompatActivity activity,
            BottomControlsStacker bottomControlsStacker,
            BrowserControlsSizer controlsSizer,
            FullscreenManager fullscreenManager,
            MonotonicObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier,
            ToolbarControlContainer controlContainer,
            CompositorViewHolder compositorViewHolder,
            Callback<Boolean> urlFocusChangedCallback,
            ToolbarThemeColorProvider toolbarThemeColorProvider,
            @Nullable ToolbarThemeColorProvider adjustedToolbarThemeColorProvider,
            BottomUiThemeColorProvider bottomUiThemeColorProvider,
            IncognitoStateProvider incognitoStateProvider,
            TabObscuringHandler tabObscuringHandler,
            MonotonicObservableSupplier<ShareDelegate> shareDelegateSupplier,
            List<ButtonDataProvider> buttonDataProviders,
            ActivityTabProvider tabProvider,
            ScrimManager scrimManager,
            ToolbarActionModeCallback toolbarActionModeCallback,
            FindToolbarManager findToolbarManager,
            MonotonicObservableSupplier<Profile> profileSupplier,
            NullableObservableSupplier<BookmarkModel> bookmarkModelSupplier,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            OneshotSupplier<AppMenuCoordinator> appMenuCoordinatorSupplier,
            boolean canShowUpdateBadge,
            MonotonicObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            NonNullObservableSupplier<Boolean> omniboxFocusStateSupplier,
            OneshotSupplier<Boolean> promoShownOneshotSupplier,
            WindowAndroid windowAndroid,
            ActivityResultTracker activityResultTracker,
            DeviceLockActivityLauncher deviceLockActivityLauncher,
            OneshotSupplier<ChromeAndroidTask> chromeAndroidTaskSupplier,
            Supplier<Boolean> isInOverviewModeSupplier,
            NonNullObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            StatusBarColorController statusBarColorController,
            AppMenuDelegate appMenuDelegate,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            BottomSheetController bottomSheetController,
            @Nullable DataSharingTabManager dataSharingTabManager,
            TabContentManager tabContentManager,
            TabCreatorManager tabCreatorManager,
            MonotonicObservableSupplier<MerchantTrustSignalsCoordinator>
                    merchantTrustSignalsCoordinatorSupplier,
            OmniboxActionDelegateImpl omniboxActionDelegate,
            MonotonicObservableSupplier<EphemeralTabCoordinator> ephemeralTabCoordinatorSupplier,
            boolean initializeWithIncognitoColors,
            @Nullable BackPressManager backPressManager,
            MonotonicObservableSupplier<ReadAloudController> readAloudControllerSupplier,
            @Nullable DesktopWindowStateManager desktopWindowStateManager,
            @Nullable TokenHolder lockTopControlsTokenJar,
            MonotonicObservableSupplier<TabBookmarker> tabBookmarkerSupplier,
            @Nullable VisibilityDelegate menuButtonVisibilityDelegate,
            TopControlsStacker topControlsStacker,
            TopInsetProvider topInsetProvider,
            NonNullObservableSupplier<Boolean> xrSpaceModeObservableSupplier,
            PageZoomManager pageZoomManager,
            SnackbarManager snackbarManager,
            @Nullable OmniboxChipManager omniboxChipManager,
            @Nullable BottomBarHostManager bottomBarHostManager,
            @Nullable ActionRegistry actionRegistry,
            GlicButtonDelegate toggleGlicCallback) {
        TraceEvent.begin("ToolbarManager.ToolbarManager");
        mActionRegistry = actionRegistry;
        mToggleGlicCallback = toggleGlicCallback;
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mCompositorViewHolder = compositorViewHolder;
        mBottomControlsStacker = bottomControlsStacker;
        mTopControlsStacker = topControlsStacker;
        mBrowserControlsSizer = controlsSizer;
        mFullscreenManager = fullscreenManager;
        mEdgeToEdgeControllerSupplier = edgeToEdgeControllerSupplier;
        mActionBarDelegate =
                new ViewShiftingActionBarDelegate(
                        activity.getSupportActionBar(),
                        controlContainer,
                        activity.findViewById(R.id.action_bar_black_background));
        mScrimManager = scrimManager;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mOmniboxFocusStateSupplier = omniboxFocusStateSupplier;
        mPromoShownOneshotSupplier = promoShownOneshotSupplier;
        mAppMenuDelegate = appMenuDelegate;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mStatusBarColorController = statusBarColorController;
        mUrlFocusChangedCallback = urlFocusChangedCallback;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mBottomSheetController = bottomSheetController;
        mDataSharingTabManager = dataSharingTabManager;
        mTabContentManager = tabContentManager;
        mTabCreatorManager = tabCreatorManager;
        mTabObscuringHandler = tabObscuringHandler;
        mShareDelegateSupplier = shareDelegateSupplier;
        mEphemeralTabCoordinatorSupplier = ephemeralTabCoordinatorSupplier;
        mUserEducationHelper = new UserEducationHelper(mActivity, profileSupplier, mHandler);
        mDesktopWindowStateManager = desktopWindowStateManager;
        mOverrideUrlLoadingDelegate = new OverrideUrlLoadingDelegateImpl();
        mTabBookmarkerSupplier = tabBookmarkerSupplier;
        mTopInsetProvider = topInsetProvider;
        mIsTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity);
        mOverridableTabCount = new OverridableTabCount(tabModelSelectorSupplier);
        mProfileSupplier = profileSupplier;
        mChromeAndroidTaskSupplier = chromeAndroidTaskSupplier;
        mBottomBarHostManager = bottomBarHostManager;

        mToolbarLayout = mActivity.findViewById(R.id.toolbar);
        NewTabPageDelegate ntpDelegate = createNewTabPageDelegate();
        mIsCustomTab = mToolbarLayout instanceof CustomTabToolbar;

        mLocationBarModel =
                new LocationBarModel(
                        activity,
                        ntpDelegate,
                        DomDistillerTabUtils::getFormattedUrlFromOriginalDistillerUrl,
                        new LocationBarModel.OfflineStatus() {
                            @Override
                            public boolean isShowingTrustedOfflinePage(Tab tab) {
                                return OfflinePageTabData.isShowingTrustedOfflinePage(tab);
                            }

                            @Override
                            public boolean isOfflinePage(Tab tab) {
                                TraceEvent.begin("isOfflinePage");
                                boolean ret = OfflinePageTabData.isShowingOfflinePage(tab);
                                TraceEvent.end("isOfflinePage");
                                return ret;
                            }
                        },
                        mToolbarPositionSupplier,
                        /* matchTrustedCdnUrl= */ mIsCustomTab);
        mControlContainer = controlContainer;
        mToolbarHairline = mControlContainer.findViewById(R.id.toolbar_hairline);

        mBookmarkModelSupplier = bookmarkModelSupplier;
        mBookmarkModelSupplier.addSyncObserverAndPostIfNonNull(mBookmarkModelSupplierObserver);

        mIphController = new ToolbarIphController(activity, mUserEducationHelper);

        mLayoutStateProviderSupplier = layoutStateProviderSupplier;
        mLayoutStateProviderSupplier.onAvailable(
                mCallbackController.makeCancelable(this::setLayoutStateProvider));

        mComponentCallbacks =
                new ComponentCallbacks() {
                    @Override
                    public void onConfigurationChanged(Configuration configuration) {
                        int newOrientation = configuration.orientation;
                        if (newOrientation == mCurrentOrientation) {
                            return;
                        }
                        mCurrentOrientation = newOrientation;
                        onOrientationChange();
                    }

                    @Override
                    public void onLowMemory() {}
                };
        mActivity.registerComponentCallbacks(mComponentCallbacks);

        mIncognitoStateProvider = incognitoStateProvider;
        mBottomUiThemeColorProvider = bottomUiThemeColorProvider;
        mToolbarThemeColorProvider = toolbarThemeColorProvider;
        mToolbarThemeColorProvider.addThemeColorObserver(this);
        mAdjustedToolbarThemeColorProvider = adjustedToolbarThemeColorProvider;
        if (mAdjustedToolbarThemeColorProvider != null) {
            mAdjustedToolbarThemeColorProvider.addThemeColorObserver(this);
        }

        final boolean isDefaultDisplay = DisplayUtil.isContextInDefaultDisplay(mActivity);
        mAppThemeColorProvider =
                new AppThemeColorProvider(
                        /* context= */ mActivity,
                        ToolbarFeatures.isAppHeaderCustomizationSupported(
                                        mIsTablet, isDefaultDisplay)
                                ? mActivityLifecycleDispatcher
                                : null,
                        mDesktopWindowStateManager);
        // Observe tint changes to update sub-components that rely on the tint (crbug.com/40688818).
        mAppThemeColorProvider.addTintObserver(this);
        mCustomTabThemeColorProvider = new SettableThemeColorProvider(/* context= */ mActivity);

        mActivityTabProvider = tabProvider;
        mActivityResultTracker = activityResultTracker;
        mSnackbarManager = snackbarManager;

        mToolbarTabController =
                new ToolbarTabControllerImpl(
                        mLocationBarModel::getTab,
                        () -> TrackerFactory.getTrackerForProfile(mProfileSupplier),
                        () -> mTabGroupUiBottomControlsCoordinatorSupplier.get(),
                        this::homepageUrl,
                        this::updateButtonStatus,
                        mActivityTabProvider,
                        mTabCreatorManager,
                        mLocationBarModel::isOffTheRecord,
                        () -> {
                            HomepageManager.getInstance()
                                    .openHomepage(
                                            mLocationBarModel.getTab(),
                                            mTabCreatorManager,
                                            mLocationBarModel.isOffTheRecord());
                        },
                        (homePageUrl) -> {
                            HomepageManager.getInstance().recordHomeNavigationMetrics(homePageUrl);
                        });

        if (backPressManager != null) {
            mBackPressHandler = new OnBackPressHandler();
            backPressManager.addHandler(mBackPressHandler, BackPressHandler.Type.TAB_HISTORY);
            mBackPressManager = backPressManager;
        } else {
            mBackPressHandler = null;
            mBackPressManager = null;
        }

        BrowserStateBrowserControlsVisibilityDelegate controlsVisibilityDelegate =
                mBrowserControlsSizer.getBrowserVisibilityDelegate();
        assert controlsVisibilityDelegate != null;
        mControlsVisibilityDelegate = controlsVisibilityDelegate;
        ThemeColorProvider browsingModeThemeColorProvider = getBrowsingModeThemeColorProvider();
        ThemeColorProvider browsingModeThemeColorProviderWithAdjustableTint =
                getBrowsingModeThemeColorProviderWithAdjustableTint();
        ThemeColorProvider overviewModeThemeColorProvider = mAppThemeColorProvider;

        Runnable requestFocusRunnable = compositorViewHolder::requestFocus;
        ThemeColorProvider menuButtonThemeColorProvider =
                mIsCustomTab
                        ? mCustomTabThemeColorProvider
                        : browsingModeThemeColorProviderWithAdjustableTint;

        Supplier<@Nullable MenuButtonState> menuButtonStateSupplier =
                () -> {
                    if (mUpdateMenuItemHelper == null) return null;
                    return mUpdateMenuItemHelper.getUiState().buttonState;
                };
        Runnable onMenuButtonClicked =
                () -> {
                    if (mUpdateMenuItemHelper == null) return;
                    mUpdateMenuItemHelper.onMenuButtonClicked();
                };

        mMenuButtonCoordinator =
                new MenuButtonCoordinator(
                        mActivity,
                        appMenuCoordinatorSupplier,
                        mControlsVisibilityDelegate,
                        mWindowAndroid,
                        this::setUrlBarFocus,
                        requestFocusRunnable,
                        canShowUpdateBadge,
                        isInOverviewModeSupplier,
                        menuButtonThemeColorProvider,
                        mIncognitoStateProvider,
                        menuButtonStateSupplier,
                        onMenuButtonClicked,
                        R.id.menu_button_wrapper,
                        menuButtonVisibilityDelegate,
                        /* isWebApp= */ false);
        if (canShowUpdateBadge) mMenuStateObserver = mMenuButtonCoordinator.getStateObserver();

        // TODO(crbug.com/351005760): Investigate the feasibility of replacing
        // mOverviewModeMenuButtonCoordinator with mMenuButtonCoordinator when Hub is enabled.
        mOverviewModeMenuButtonCoordinator =
                new MenuButtonCoordinator(
                        mActivity,
                        appMenuCoordinatorSupplier,
                        mControlsVisibilityDelegate,
                        mWindowAndroid,
                        this::setUrlBarFocus,
                        requestFocusRunnable,
                        canShowUpdateBadge,
                        isInOverviewModeSupplier,
                        overviewModeThemeColorProvider,
                        mIncognitoStateProvider,
                        menuButtonStateSupplier,
                        onMenuButtonClicked,
                        R.id.none,
                        menuButtonVisibilityDelegate,
                        /* isWebApp= */ false);

        ToggleTabStackButton tabSwitcherButton =
                mControlContainer.findViewById(R.id.tab_switcher_button);
        if (tabSwitcherButton != null) {
            mTabSwitcherButtonCoordinator =
                    new ToggleTabStackButtonCoordinator(
                            mActivity,
                            tabSwitcherButton,
                            mUserEducationHelper,
                            mPromoShownOneshotSupplier,
                            mLayoutStateProviderSupplier,
                            mActivityTabProvider.asObservable(),
                            mTabModelSelectorSupplier,
                            browsingModeThemeColorProviderWithAdjustableTint,
                            mIncognitoStateProvider);
        }

        NavigationPopup.HistoryDelegate historyDelegate =
                (tab) -> {
                    WindowAndroid windowAndroidFromTab = tab.getWindowAndroid();
                    assumeNonNull(windowAndroidFromTab);
                    Activity activityFromTab = windowAndroidFromTab.getActivity().get();
                    assert activityFromTab != null;
                    HistoryManagerUtils.showHistoryManager(activityFromTab, tab, tab.getProfile());
                };

        View homeButton = controlContainer.findViewById(R.id.home_button);
        if (homeButton != null) {
            mHomeButtonCoordinator =
                    new HomeButtonCoordinator(
                            mActivity,
                            homeButton,
                            (metaState, buttonState) -> {
                                if (ntpDelegate.isCurrentlyVisible()) {
                                    // Record the clicking action on the home button.
                                    BrowserUiUtils.recordModuleClickHistogram(
                                            ModuleTypeOnStartAndNtp.HOME_BUTTON);
                                }
                                setUrlBarFocus(false, OmniboxFocusReason.UNFOCUS);

                                boolean hasControl = KeyEventUtils.isCtrlOn(metaState);
                                boolean hasShift = KeyEventUtils.isShiftOn(metaState);
                                boolean isMiddleClick =
                                        MotionEventUtils.isTertiaryButton(buttonState);

                                if ((hasControl && hasShift) || (isMiddleClick && hasShift)) {
                                    mToolbarTabController.openHomepageInNewTab(
                                            /* foregroundNewTab= */ true);
                                } else if (hasControl || isMiddleClick) {
                                    mToolbarTabController.openHomepageInNewTab(
                                            /* foregroundNewTab= */ false);
                                } else if (hasShift) {
                                    mToolbarTabController.openHomepageInNewWindow();
                                } else {
                                    mToolbarTabController.openHomepage();
                                }

                                Tracker tracker =
                                        TrackerFactory.getTrackerForProfile(mProfileSupplier);
                                boolean isPartnerHomepageEnabled =
                                        PartnerBrowserCustomizations.getInstance()
                                                .isHomepageProviderAvailableAndEnabled();
                                if (tracker != null && isPartnerHomepageEnabled) {
                                    tracker.notifyEvent(
                                            EventConstants.PARTNER_HOME_PAGE_BUTTON_PRESSED);
                                }
                            },
                            this::onHomeButtonMenuClick,
                            HomepagePolicyManager::isHomepageLocationManaged,
                            browsingModeThemeColorProviderWithAdjustableTint,
                            mIncognitoStateProvider,
                            mActionRegistry);
        }

        ChromeImageButton backButton = mControlContainer.findViewById(R.id.back_button);
        if (backButton != null) {
            mBackButtonCoordinator =
                    new BackButtonCoordinator(
                            backButton,
                            this::back,
                            browsingModeThemeColorProvider,
                            mIncognitoStateProvider,
                            mActivityTabProvider.asObservable(),
                            mToolbarNavControlsEnabledSupplier,
                            /* onNavigationPopupShown= */ () -> {},
                            historyDelegate,
                            /* isWebApp= */ false);
        }

        ChromeImageButton forwardButton = mControlContainer.findViewById(R.id.forward_button);
        if (forwardButton != null) {
            mForwardButtonCoordinator =
                    new ForwardButtonCoordinator(
                            mActivity,
                            mLocationBarModel,
                            mToolbarTabController,
                            mLocationBarSupplier,
                            mActivityLifecycleDispatcher,
                            forwardButton,
                            historyDelegate,
                            browsingModeThemeColorProvider,
                            mIncognitoStateProvider);
        }

        mToolbarLongPressMenuHandler =
                new ToolbarLongPressMenuHandler(
                        /* context= */ mActivity,
                        profileSupplier,
                        mIsCustomTab,
                        this::shouldSuppressToolbarLongPress,
                        mActivityLifecycleDispatcher,
                        mWindowAndroid,
                        () -> mLocationBarModel.getUrlBarData().url,
                        () -> getUrlBarViewRectProvider(),
                        this::onSendTabToSelfClicked);
        OnLongClickListener onLongClickListener =
                mToolbarLongPressMenuHandler.getOnLongClickListener();

        ViewStub progressBarStub = mActivity.findViewById(R.id.progress_bar_stub);
        if (ChromeFeatureList.sAndroidProgressBarVisualUpdate.isEnabled()) {
            ViewGroup.LayoutParams progressBarParams = progressBarStub.getLayoutParams();
            progressBarParams.height =
                    mActivity
                            .getResources()
                            .getDimensionPixelSize(R.dimen.toolbar_progress_bar_increased_height);
            progressBarStub.setLayoutParams(progressBarParams);
        }

        if (ChromeFeatureList.sAndroidAnimatedProgressBarInBrowser.isEnabled()
                && ChromeFeatureList.sAndroidApb144Patch4.isEnabled()) {
            CoordinatorLayout.LayoutParams progressBarParams =
                    (CoordinatorLayout.LayoutParams) progressBarStub.getLayoutParams();
            progressBarParams.gravity = Gravity.BOTTOM;
            progressBarStub.setLayoutParams(progressBarParams);
        }

        mProgressBarContainer = progressBarStub.inflate();
        ToolbarProgressBar progressBar =
                mProgressBarContainer.findViewById(R.id.toolbar_progress_bar);
        progressBar.setAnimatingView(
                mProgressBarContainer.findViewById(R.id.progress_bar_animating_view));
        mBrowserControlsSizer.addObserver(progressBar);
        mToolbarProgressBarLayer =
                new ToolbarProgressBarLayer(
                        mControlContainer,
                        mProgressBarContainer,
                        progressBar,
                        mToolbarHairline,
                        mToolbarPositionSupplier,
                        () ->
                                mBookmarkBarHeightSupplier != null
                                                && mBookmarkBarHeightSupplier.get() == 0
                                        ? 0
                                        : R.id.bookmark_bar,
                        topControlsStacker,
                        bottomControlsStacker,
                        ToolbarPositionController.isToolbarPositionCustomizationEnabled(
                                mActivity, mIsCustomTab));

        if (mHomeButtonCoordinator != null) {
            browsingModeThemeColorProviderWithAdjustableTint.addTintObserver(
                    mHomeButtonCoordinator);
        }
        mTabStripTopControlLayer =
                new TabStripTopControlLayer(
                        mToolbarLayout.getTabStripHeightFromResource(),
                        mTopControlsStacker,
                        mBrowserControlsSizer,
                        mControlContainer,
                        lockTopControlsTokenJar);
        mToolbar =
                createTopToolbarCoordinator(
                        controlContainer,
                        buttonDataProviders,
                        browsingModeThemeColorProviderWithAdjustableTint,
                        mIncognitoStateProvider,
                        initializeWithIncognitoColors,
                        mConstraintsSupplier,
                        onLongClickListener,
                        progressBar,
                        historyDelegate,
                        topControlsStacker,
                        mTabStripTopControlLayer,
                        profileSupplier,
                        activityResultTracker,
                        deviceLockActivityLauncher,
                        snackbarManager);
        mActionModeController =
                new ActionModeController(
                        mActivity,
                        mActionBarDelegate,
                        toolbarActionModeCallback,
                        mTabStripTopControlLayer.getSupplier());

        tabObscuringHandler.addObserver(this);

        Runnable clickDelegate = () -> setUrlBarFocus(false, OmniboxFocusReason.UNFOCUS);
        View scrimTarget = mCompositorViewHolder;
        mLocationBarFocusHandler =
                new LocationBarFocusScrimHandler(
                        scrimManager,
                        new TabObscuringCallback(tabObscuringHandler),
                        /* context= */ activity,
                        mLocationBarModel,
                        clickDelegate,
                        scrimTarget,
                        mTabStripTopControlLayer.getSupplier(),
                        mBottomControlsStacker);

        if (mIsCustomTab) {
            CustomTabToolbar customTabToolbar = ((CustomTabToolbar) mToolbarLayout);
            mLocationBar =
                    customTabToolbar.createLocationBar(
                            mLocationBarModel,
                            mActionModeController.getActionModeCallback(),
                            modalDialogManagerSupplier,
                            mEphemeralTabCoordinatorSupplier,
                            mControlsVisibilityDelegate,
                            mTabCreatorManager.getTabCreator(
                                    mIncognitoStateProvider.isIncognitoSelected()));
        } else {
            ChromePageInfo toolbarPageInfo =
                    new ChromePageInfo(
                            modalDialogManagerSupplier,
                            null,
                            OpenedFromSource.TOOLBAR,
                            SupplierUtils.upcast(
                                    merchantTrustSignalsCoordinatorSupplier,
                                    StoreInfoActionHandler.class),
                            mEphemeralTabCoordinatorSupplier,
                            mTabCreatorManager.getTabCreator(
                                    mIncognitoStateProvider.isIncognitoSelected()));
            OmniboxSuggestionsDropdownScrollListener scrollListener =
                    mToolbarLayout instanceof OmniboxSuggestionsDropdownScrollListener
                            ? (OmniboxSuggestionsDropdownScrollListener) mToolbarLayout
                            : null;

            Supplier<Integer> bottomWindowPaddingSupplier =
                    () -> {
                        var edgeToEdgeController = mEdgeToEdgeControllerSupplier.get();
                        return edgeToEdgeController != null
                                ? edgeToEdgeController.getBottomInsetPx()
                                : 0;
                    };

            View bottomContainerView = mActivity.findViewById(R.id.bottom_container);
            // TODO(crbug.com/448691376): Change LocationBarCoordinator profileSupplier argument to
            // Supplier<@Nullable Profile> and updated the rest of the code.
            // TODO(crbug.com/448691376): Change LocationBarCoordinator modalDialogManagerSupplier
            // argument to Supplier<ModalDialogManager> and updated the rest of the code.
            LocationBarCoordinator locationBarCoordinator =
                    new LocationBarCoordinator(
                            mActivity.findViewById(R.id.location_bar),
                            mToolbarLayout,
                            profileSupplier,
                            mLocationBarModel,
                            mActionModeController.getActionModeCallback(),
                            windowAndroid,
                            mActivityTabProvider.asObservable(),
                            modalDialogManagerSupplier,
                            shareDelegateSupplier,
                            mIncognitoStateProvider,
                            activityLifecycleDispatcher,
                            mOverrideUrlLoadingDelegate,
                            new BackKeyBehaviorDelegate() {},
                            toolbarPageInfo::show,
                            IntentHandler::bringTabGroupToFront,
                            NewTabPageUma::recordOmniboxNavigation,
                            (url) -> {
                                BookmarkModel bridge = mBookmarkModelSupplier.get();
                                return bridge != null && bridge.isBookmarked(url);
                            },
                            () ->
                                    mToolbar.getCurrentOptionalButtonVariant()
                                            == AdaptiveToolbarButtonVariant.VOICE,
                            omniboxActionDelegate,
                            mControlsVisibilityDelegate,
                            backPressManager,
                            scrollListener,
                            tabModelSelectorSupplier,
                            topInsetProvider,
                            mToolbarLayout,
                            new LocationBarEmbedderUiOverrides(),
                            mActivity.findViewById(R.id.coordinator),
                            bottomWindowPaddingSupplier,
                            onLongClickListener,
                            mBrowserControlsSizer,
                            ToolbarPositionController.isToolbarPositionCustomizationEnabled(
                                    mActivity, mIsCustomTab),
                            pageZoomManager,
                            TabFavicon::getBitmap,
                            snackbarManager,
                            bottomContainerView,
                            omniboxChipManager,
                            mLocationBarFocusHandler,
                            mUserEducationHelper);
            mToolbarLayout.setLocationBarCoordinator(locationBarCoordinator);
            mToolbarLayout.setBrowserControlsVisibilityDelegate(mControlsVisibilityDelegate);
            mToolbarLayout.setBrowserControlsStateProvider(mBrowserControlsSizer);
            mLocationBar = locationBarCoordinator;
            locationBarCoordinator.setOnStatusViewHiddenForPageInfoRemoval(
                    () -> {
                        if (mSiteControlsIphController == null) {
                            mSiteControlsIphController =
                                    new SiteControlsIphController(
                                            mUserEducationHelper,
                                            getMenuButtonView(),
                                            appMenuCoordinatorSupplier.get().getAppMenuHandler());
                        }
                        mSiteControlsIphController.showIph();
                    });
        }

        var omnibox = mLocationBar.getOmniboxStub();
        if (omnibox != null) {
            mOmniboxStubSupplier.set(omnibox);
            omnibox.addUrlFocusChangeListener(this);
            omnibox.addUrlFocusChangeListener(mStatusBarColorController);
        }
        mLocationBar.addOmniboxSuggestionsDropdownScrollListener(mStatusBarColorController);

        mProgressBarCoordinator =
                new LoadProgressCoordinator(
                        mActivityTabProvider.asObservable(), mToolbar.getProgressBar());
        mToolbar.setToolbarColorObserver(statusBarColorController);

        mActivityTabTabObserver =
                new ActivityTabProvider.ActivityTabTabObserver(mActivityTabProvider) {
                    private @Nullable NavigationHandle mLastNavigation;
                    private @Nullable String mLastUrl;
                    private @Nullable String mCurrentUrl;
                    private long mLastNavigationTimestamp;

                    @Override
                    public void onObservingDifferentTab(@Nullable Tab tab) {
                        // ActivityTabProvider will null out the tab passed to
                        // onObservingDifferentTab when the tab is non-interactive (e.g. when
                        // entering the TabSwitcher).
                        // In those cases we actually still want to use the most recently selected
                        // tab, but will update the URL.
                        onBackPressStateChanged();
                        onBackForwardTransitionAnimationChange();
                        applyIncognitoNtpAccessibilityOrder(tab);
                        if (mBackPressHandler != null) mBackPressHandler.handleOnBackCancelled();
                        mBackGestureInProgress = false;
                        if (tab == null) {
                            mLocationBarModel.notifyUrlChanged(false);
                            return;
                        }
                        // Switching tabs.
                        if (mLastTab != tab) {
                            // Update mLastTab.
                            mLastTab = tab;
                        }

                        refreshSelectedTab(tab);
                        onTabOrModelChanged();
                        maybeTriggerCacheRefreshForZeroSuggest(tab.getUrl());
                    }

                    /**
                     * Trigger ZeroSuggest cache refresh in case user is accessing a new tab page.
                     * Avoid issuing multiple concurrent server requests for the same event to
                     * reduce server pressure.
                     */
                    private void maybeTriggerCacheRefreshForZeroSuggest(GURL url) {
                        if (url != null) {
                            mLocationBarModel.notifyZeroSuggestRefresh();
                        }
                    }

                    @Override
                    public void onSSLStateUpdated(Tab tab) {
                        onBackPressStateChanged();
                        if (mLocationBarModel.getTab() == null) return;

                        assert tab == mLocationBarModel.getTab();
                        mLocationBarModel.notifySecurityStateChanged();
                        mLocationBarModel.notifyUrlChanged(false);
                    }

                    @Override
                    public void onTitleUpdated(Tab tab) {
                        onBackPressStateChanged();
                        mLocationBarModel.notifyTitleChanged();
                    }

                    @Override
                    public void onUrlUpdated(Tab tab) {
                        // Update the SSL security state as a result of this notification as it will
                        // sometimes be the only update we receive.
                        updateTabLoadingState(true);
                        onBackPressStateChanged();

                        // A URL update is a decent enough indicator that the toolbar widget is in
                        // a stable state to capture its bitmap for use in fullscreen.
                        mControlContainer.setReadyForBitmapCapture(true);
                    }

                    @Override
                    public void onShown(Tab tab, @TabSelectionType int type) {
                        onBackPressStateChanged();
                        if (tab.getUrl().isEmpty()) return;
                        mControlContainer.setReadyForBitmapCapture(true);
                    }

                    @Override
                    public void onCrash(Tab tab) {
                        mLocationBarModel.notifyOnCrash();
                        updateTabLoadingState(false);
                        updateButtonStatus();
                    }

                    @Override
                    public void onLoadStarted(Tab tab, boolean toDifferentDocument) {
                        onBackPressStateChanged();
                        if (!toDifferentDocument) return;
                        updateTabLoadingState(true);
                    }

                    @Override
                    public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                        onBackPressStateChanged();
                        if (!toDifferentDocument) return;
                        updateTabLoadingState(true);
                        mLocationBarModel.onPageLoadStopped();
                        mToolbar.onPageLoadStopped();
                    }

                    @Override
                    public void onContentChanged(Tab tab) {
                        mFormFieldFocusedSupplier.onWebContentsChanged(tab.getWebContents());
                        mLocationBarModel.notifyContentChanged();
                        checkIfNtpLoaded();
                        mToolbar.onTabContentViewChanged();
                        mLocationBar.maybeShowOrClearCursorInLocationBar();
                        // Paint preview status might have been changed. Update the omnibox chip.
                        mLocationBarModel.notifySecurityStateChanged();
                        onBackPressStateChanged();
                        applyIncognitoNtpAccessibilityOrder(tab);
                    }

                    @Override
                    public void onLoadUrl(
                            Tab tab, LoadUrlParams params, LoadUrlResult loadUrlResult) {
                        onBackPressStateChanged();
                        NewTabPage ntp = getNewTabPageForCurrentTab();
                        if (ntp == null) return;
                        if (!UrlUtilities.isNtpUrl(params.getUrl())
                                && loadUrlResult.tabLoadStatus
                                        != Tab.TabLoadStatus.PAGE_LOAD_FAILED) {
                            ntp.setUrlFocusAnimationsDisabled(true);
                            onTabOrModelChanged();
                        }
                    }

                    private boolean hasPendingNonNtpNavigation(Tab tab) {
                        WebContents webContents = tab.getWebContents();
                        if (webContents == null) return false;

                        NavigationController navigationController =
                                webContents.getNavigationController();
                        if (navigationController == null) return false;

                        NavigationEntry pendingEntry = navigationController.getPendingEntry();
                        if (pendingEntry == null) return false;

                        return !UrlUtilities.isNtpUrl(pendingEntry.getUrl());
                    }

                    @Override
                    public void onDidFinishNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigation) {
                        onBackPressStateChanged();
                        if (navigation.hasCommitted() && !navigation.isSameDocument()) {
                            String newUrl = navigation.getUrl().getSpec();

                            if (mLastUrl != null
                                    && mLastUrl.equals(newUrl)
                                    && !mLastUrl.equals(mCurrentUrl)) {
                                assumeNonNull(mLastNavigation);

                                // Backfalsing detected, emit metrics.
                                // NavigationHandle#isBack or #isForward is true when the
                                // navigation is caused by Tab#goBack or #goForward, such as
                                // the back/forward button in toolbar, the navigation gesture,
                                // the back button in system navigation bar.
                                // 0: forward nav, 1: backward nav, 2: neither.
                                @NavigationDirection int direction = NavigationDirection.NEITHER;
                                if (mLastNavigation.isBack() && !navigation.isBack()) {
                                    direction = NavigationDirection.FORWARD;
                                } else if (mLastNavigation.isForward() && navigation.isBack()) {
                                    direction = NavigationDirection.BACKWARD;
                                }
                                if (TimeUtils.elapsedRealtimeMillis() - mLastNavigationTimestamp
                                        <= 3 * 1000) {
                                    // Only record if two consecutive navigations happen with 3
                                    // seconds.
                                    BackPressMetrics.recordStrictBackFalsing(direction);
                                }
                                BackPressMetrics.recordBackFalsing(direction);
                            }
                            // Update the URLs and index.
                            mLastUrl = mCurrentUrl;
                            mCurrentUrl = newUrl;
                            mLastNavigation = navigation;
                            mLastNavigationTimestamp = TimeUtils.elapsedRealtimeMillis();

                            mToolbar.onNavigatedToDifferentPage();
                            maybeTriggerCacheRefreshForZeroSuggest(navigation.getUrl());
                            mBottomControlsStacker.notifyDidFinishNavigationInPrimaryMainFrame();
                        }

                        // If the load failed due to a different navigation, there is no need to
                        // reset the location bar animations.
                        if (navigation.errorCode() != NetError.OK
                                && !hasPendingNonNtpNavigation(tab)) {
                            NewTabPage ntp = getNewTabPageForCurrentTab();
                            if (ntp == null) return;

                            ntp.setUrlFocusAnimationsDisabled(false);
                            onTabOrModelChanged();
                        }
                    }

                    @Override
                    public void onDidFinishNavigationEnd() {
                        onBackPressStateChanged();
                        mLocationBarModel.notifyDidFinishNavigationEnd();
                    }

                    @Override
                    public void onDidStartNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigationHandle) {
                        assert tab == mLocationBarModel.getTab();
                        mStartNavDuringOngoingGesture |= mBackGestureInProgress;
                        BackPressMetrics.recordNavigateBetweenChromeNativePages(
                                UrlUtilities.isChromeNativeUrl(tab.getUrl())
                                        && UrlUtilities.isChromeNativeUrl(
                                                navigationHandle.getUrl()));
                        onBackPressStateChanged();
                        mLocationBarModel.notifyDidStartNavigation(
                                navigationHandle.isSameDocument());
                        if (mIsCustomTab && !navigationHandle.isSameDocument()) {
                            ((CustomTabToolbar) mToolbarLayout).resetOptionalButtonState();
                        }
                    }

                    @Override
                    public void onNavigationEntriesDeleted(Tab tab) {
                        onBackPressStateChanged();
                    }

                    @Override
                    public void onNavigationStateChanged() {
                        onBackPressStateChanged();
                    }

                    @Override
                    public void didFirstVisuallyNonEmptyPaint(Tab tab) {
                        mToolbar.onDidFirstVisuallyNonEmptyPaint();
                    }

                    @Override
                    public void didBackForwardTransitionAnimationChange(Tab tab) {
                        onBackForwardTransitionAnimationChange();
                    }

                    @Override
                    public void onContentViewScrollingStateChanged(boolean scrolling) {
                        mToolbar.onContentViewScrollingStateChanged(scrolling);
                    }
                };

        mCurrentTabModelObserver =
                (tabModel) -> {
                    if (mTabModelSelector != null) {
                        refreshSelectedTab(mTabModelSelector.getCurrentTab());
                    }
                };

        mBookmarksObserver =
                new BookmarkModelObserver() {
                    @Override
                    public void bookmarkModelChanged() {
                        updateBookmarkButtonStatus();
                    }
                };

        mBrowserControlsObserver =
                new BrowserControlsStateProvider.Observer() {
                    private @Nullable OnLayoutChangeListener mLayoutChangeListener;

                    @Override
                    public void onControlsOffsetChanged(
                            int topOffset,
                            int topControlsMinHeightOffset,
                            boolean topControlsMinHeightChanged,
                            int bottomOffset,
                            int bottomControlsMinHeightOffset,
                            boolean bottomControlsMinHeightChanged,
                            boolean requestNewFrame,
                            boolean isVisibilityForced) {
                        // Controls need to be offset to match the composited layer, which is
                        // anchored below the minimum height. In other words, the top of the toolbar
                        // composited layer is anchored at the bottom of the minimum height.
                        // https://crbug.com/40736668 wait until the background is cleared so that
                        // the height won't be measured by the background image.
                        if (mControlContainer.getBackground() == null) {
                            setControlContainerTopMargin(getToolbarExtraYOffset());
                        } else if (mLayoutChangeListener == null) {
                            mLayoutChangeListener =
                                    (view,
                                            left,
                                            top,
                                            right,
                                            bottom,
                                            oldLeft,
                                            oldTop,
                                            oldRight,
                                            oldBottom) -> {
                                        if (mControlContainer.getBackground() == null) {
                                            setControlContainerTopMargin(getToolbarExtraYOffset());
                                            mControlContainer.removeOnLayoutChangeListener(
                                                    mLayoutChangeListener);
                                            mLayoutChangeListener = null;
                                        }
                                    };
                            mControlContainer.addOnLayoutChangeListener(mLayoutChangeListener);
                        }
                    }

                    @Override
                    public void onControlsPositionChanged(@ControlsPosition int controlsPosition) {
                        Tab currentTab = mActivityTabProvider.get();
                        applyIncognitoNtpAccessibilityOrder(currentTab);
                    }
                };
        mBrowserControlsSizer.addObserver(mBrowserControlsObserver);

        mFullscreenObserver =
                new FullscreenManager.Observer() {
                    @Override
                    public void onEnterFullscreen(Tab tab, FullscreenOptions options) {
                        if (mFindToolbarManager != null) mFindToolbarManager.hideToolbar();
                    }
                };
        mFullscreenManager.addObserver(mFullscreenObserver);

        mFindToolbarObserver =
                new FindToolbarObserver() {
                    @Override
                    public void onFindToolbarShown() {
                        mToolbar.handleFindLocationBarStateChange(true);
                        mFindInPageShowingSupplier.set(true);
                        if (mControlsVisibilityDelegate != null) {
                            mFullscreenFindInPageToken =
                                    mControlsVisibilityDelegate
                                            .showControlsPersistentAndClearOldToken(
                                                    mFullscreenFindInPageToken);
                        }
                    }

                    @Override
                    public void onFindToolbarHidden() {
                        mToolbar.handleFindLocationBarStateChange(false);
                        mFindInPageShowingSupplier.set(false);
                        if (mControlsVisibilityDelegate != null) {
                            mControlsVisibilityDelegate.releasePersistentShowingToken(
                                    mFullscreenFindInPageToken);
                        }
                    }
                };

        mLayoutStateObserver =
                new LayoutStateProvider.LayoutStateObserver() {
                    @Override
                    public void onStartedShowing(@LayoutType int layoutType) {
                        if (layoutType == LayoutType.TAB_SWITCHER) {
                            mInTabSwitcherTransition = true;
                        }
                        updateForLayout(layoutType);
                    }

                    @Override
                    public void onFinishedShowing(int layoutType) {
                        if (layoutType == LayoutType.TAB_SWITCHER) {
                            mInTabSwitcherTransition = false;
                            mToolbar.onTabSwitcherTransitionFinished();
                            mIsTabSwitcherFinishedShowingSupplier.set(true);
                        }
                        mToolbar.onTransitionEnd();
                        if (layoutType == LayoutType.BROWSING) {
                            maybeShowUrlBarCursorIfHardwareKeyboardAvailable();
                        }
                    }

                    @Override
                    public void onStartedHiding(@LayoutType int layoutType) {
                        if (layoutType == LayoutType.TAB_SWITCHER) {
                            mInTabSwitcherTransition = true;
                            mLocationBarModel.updateForNonStaticLayout();
                            mToolbar.setTabSwitcherMode(false);
                            mToolbarNavControlsEnabledSupplier.set(true);
                            mIsTabSwitcherFinishedShowingSupplier.set(false);
                            updateButtonStatus();
                        }
                        mToolbar.onTransitionStart();
                    }

                    @Override
                    public void onFinishedHiding(@LayoutType int layoutType) {
                        if (layoutType == LayoutType.TAB_SWITCHER) {
                            mInTabSwitcherTransition = false;
                            mToolbar.onTabSwitcherTransitionFinished();
                            updateButtonStatus();

                            if (mIsTablet) {
                                checkIfNtpLoaded();
                                mLocationBar.maybeShowOrClearCursorInLocationBar();
                            }
                        }
                    }
                };

        mFindToolbarManager = findToolbarManager;
        mFindToolbarManager.addObserver(mFindToolbarObserver);

        Callback<@Nullable Profile> profileObserver =
                new Callback<@Nullable Profile>() {
                    @Override
                    @SuppressWarnings("NullAway") // https://github.com/uber/NullAway/issues/1202
                    public void onResult(@Nullable Profile profile) {
                        assert profile != null;
                        mTemplateUrlService = TemplateUrlServiceFactory.getForProfile(profile);
                        mTemplateUrlService.runWhenLoaded(
                                ToolbarManager.this::registerTemplateUrlObserver);
                        mProfileSupplier.removeObserver(this);
                    }
                };
        profileSupplier.addSyncObserverAndPostIfNonNull(profileObserver);
        mReadAloudControllerSupplier = readAloudControllerSupplier;
        mReadAloudControllerSupplier.addSyncObserverAndPostIfNonNull(
                readAloudController -> {
                    if (readAloudController != null) {
                        readAloudController.addReadabilityUpdateListener(
                                mReadAloudReadabilityCallback);
                    }
                });

        if (mDesktopWindowStateManager != null) {
            mDesktopWindowStateManager.addObserver(mControlContainer);
        }

        mProgressBarConfigProvider =
                new WindowAndroid.ProgressBarConfig.Provider() {
                    @Override
                    public WindowAndroid.ProgressBarConfig getProgressBarConfig() {
                        WindowAndroid.ProgressBarConfig config =
                                new WindowAndroid.ProgressBarConfig();
                        config.backgroundColor = mToolbar.getProgressBar().getBackgroundColor();
                        config.heightPhysical = mToolbar.getProgressBar().getDefaultHeight();
                        config.color = mToolbar.getProgressBar().getForegroundColor();
                        if (mToolbarHairline != null) {
                            config.hairlineHeightPhysical = mToolbarHairline.getHeight();
                            config.hairlineColor = mToolbar.getToolbarHairlineColor();
                        }
                        return config;
                    }
                };
        mWindowAndroid.setProgressBarConfigProvider(mProgressBarConfigProvider);
        initializeToolbarPositionController();

        mXrSpaceModeObservableSupplier = xrSpaceModeObservableSupplier;
        mXrSpaceModeObservableSupplier.addSyncObserver(mOnXrSpaceModeChanged);

        mControlContainer
                .getToolbarResourceAdapter()
                .addOnResourceReadyCallback(
                        (Resource r) -> {
                            // {@link DynamicResourceSnapshot} saves an id to return for
                            // createNativeResource. Otherwise this might be creating a new native
                            // resource, which we wouldn't want.
                            assert r instanceof DynamicResourceSnapshot;
                            mCaptureResourceIdSupplier.set(r.createNativeResource());
                        });

        TraceEvent.end("ToolbarManager.ToolbarManager");
    }

    /**
     * Sets the supplier for the {@link SideUiStateProvider}. Will only be called if the
     * EnableAndroidSidePanel feature flag is enabled.
     *
     * <p>TODO(crbug.com/493289413): Update JavaDoc after feature is launched.
     *
     * @param sideUiStateProviderSupplier The {@link OneshotSupplier} for {@link
     *     SideUiStateProvider}.
     */
    public void setSideUiStateProviderSupplier(
            OneshotSupplier<SideUiStateProvider> sideUiStateProviderSupplier) {
        sideUiStateProviderSupplier.onAvailable(this::setSideUiStateProvider);
    }

    private void setSideUiStateProvider(SideUiStateProvider sideUiStateProvider) {
        removeSideUiObservers();

        mSideUiStateProvider = sideUiStateProvider;

        mSideUiObserver =
                (sideUiSpecs) -> {
                    // Can be null after destroy(), empty specs are passed when the observer
                    // is removed.
                    if (mFindToolbarManager != null) {
                        mFindToolbarManager.onSideUiSpecsChanged(sideUiSpecs);
                    }
                };
        mSideUiStateProvider.addObserver(mSideUiObserver);

        mControlContainerSideUiObserver = new ToolbarMarginAdjusterForSideUi(mControlContainer);
        mSideUiStateProvider.addObserver(mControlContainerSideUiObserver);
        mProgressBarSideUiObserver = new ViewMarginAdjusterForSideUi(mProgressBarContainer);
        mSideUiStateProvider.addObserver(mProgressBarSideUiObserver);
    }

    private static class ToolbarMarginAdjusterForSideUi extends ViewMarginAdjusterForSideUi {
        ToolbarMarginAdjusterForSideUi(View view) {
            super(view);
        }

        @Override
        public Set<Transition> createTransitions() {
            return Set.of(
                    new ChangeBounds(),
                    new Fade(),
                    new ChangeTransform(),
                    new ShrinkTransition(/* goneScale= */ 0.6f, /* visibleScale= */ 1.0f));
        }

        @Override
        public @Nullable Transition onPreSideUiSpecsChange(SideUiSpecs sideUiSpecs) {
            Transition transition = super.onPreSideUiSpecsChange(sideUiSpecs);
            super.triggerSynchronousMeasureAndLayout();
            return transition;
        }
    }

    private boolean shouldSuppressToolbarLongPress() {
        return mOmniboxFocusStateSupplier.get()
                || (mToolbarPositionController != null
                        && mToolbarPositionController.doesPrefMismatchPosition())
                || (isNewTabPage() && !isIncognitoNewTabPageCurrentlyVisible());
    }

    private void back(int metaState, int buttonState) {
        setUrlBarFocus(false, OmniboxFocusReason.UNFOCUS);
        boolean hasControl = KeyEventUtils.isCtrlOn(metaState);
        boolean hasShift = KeyEventUtils.isShiftOn(metaState);
        boolean isMiddleClick = MotionEventUtils.isTertiaryButton(buttonState);

        if ((hasControl && hasShift) || (isMiddleClick && hasShift)) {
            // Holding ALT is allowed as well (reference desktop behavior).
            final boolean isSuccess =
                    mToolbarTabController.backInNewTab(/* foregroundNewTab= */ true);
            if (isSuccess) RecordUserAction.record("MobileToolbarBackInNewForegroundTab");
        } else if (hasControl || isMiddleClick) {
            final boolean isSuccess =
                    mToolbarTabController.backInNewTab(/* foregroundNewTab= */ false);
            if (isSuccess) RecordUserAction.record("MobileToolbarBackInNewBackgroundTab");
        } else if (hasShift) {
            final boolean isSuccess = mToolbarTabController.backInNewWindow();
            if (isSuccess) RecordUserAction.record("MobileToolbarBackInNewForegroundWindow");
        } else {
            final boolean isSuccess = mToolbarTabController.back();
            if (isSuccess) RecordUserAction.record("MobileToolbarBack");
        }
    }

    private void initializeToolbarPositionController() {
        if (!ToolbarPositionController.isToolbarPositionCustomizationEnabled(
                mActivity, mIsCustomTab)) {
            return;
        }

        mIsNtpWithFakeboxShowingSupplier.set(
                getNewTabPageForCurrentTab() != null
                        && getNewTabPageForCurrentTab().isLocationBarShownInNtp());
        mIsIncognitoNtpShowingSupplier.set(isIncognitoNewTabPageCurrentlyVisible());
        mIsTabSwitcherFinishedShowingSupplier.set(
                mLayoutStateProvider != null
                        && mLayoutStateProvider.getActiveLayoutType() == LayoutType.TAB_SWITCHER);
        MonotonicObservableSupplier<ManualFillingComponent> manualFillingComponentSupplier =
                ManualFillingComponentSupplier.from(mWindowAndroid);
        assert manualFillingComponentSupplier != null;
        KeyboardAccessoryStateSupplier keyboardAccessoryStateSupplier =
                new KeyboardAccessoryStateSupplier(
                        manualFillingComponentSupplier, mControlContainer.getView());
        SettableNonNullObservableSupplier<Integer> controlContainerTranslationSupplier =
                ObservableSuppliers.createNonNull(0);
        SettableNonNullObservableSupplier<Integer> controlContainerHeightSupplier =
                ObservableSuppliers.createNonNull(mControlContainer.getToolbarHeight());

        mControlContainer.setOnHeightChangedListener(controlContainerHeightSupplier);

        // TODO(crbug.com/448691376): Change ToolbarPositionController profileSupplier argument
        // to Supplier<@Nullable Profile> and updated the rest of the code.
        @SuppressWarnings("UseSharedPreferencesManagerFromChromeCheck")
        SharedPreferences sharedPreferences = ContextUtils.getAppSharedPreferences();
        mToolbarPositionController =
                new ToolbarPositionController(
                        mBrowserControlsSizer,
                        sharedPreferences,
                        mIsNtpWithFakeboxShowingSupplier,
                        mIsIncognitoNtpShowingSupplier,
                        mIsTabSwitcherFinishedShowingSupplier,
                        mOmniboxFocusStateSupplier,
                        mFormFieldFocusedSupplier.getObservable(),
                        mFindInPageShowingSupplier,
                        keyboardAccessoryStateSupplier.getInsetSupplier(),
                        mWindowAndroid.getKeyboardDelegate(),
                        mControlContainer,
                        mToolbarLayout,
                        mTopControlsStacker,
                        mBottomControlsStacker,
                        mBottomSheetController,
                        mBottomToolbarControlsOffsetSupplier,
                        mProgressBarContainer,
                        controlContainerTranslationSupplier,
                        controlContainerHeightSupplier,
                        mTopInsetProvider,
                        new Handler(Looper.getMainLooper()),
                        mActivity,
                        mToolbarPositionSupplier,
                        (MonotonicObservableSupplier<Profile>) mProfileSupplier,
                        mActivityTabProvider,
                        assertNonNull(mWindowAndroid.getInsetObserver())
                                .getSupplierForKeyboardInset(),
                        mWindowAndroid);

        mMiniOriginBarController =
                new MiniOriginBarController(
                        mLocationBar,
                        mFormFieldFocusedSupplier,
                        mBottomSheetController,
                        mWindowAndroid.getKeyboardDelegate(),
                        mActivity,
                        mControlContainer,
                        mSuppressToolbarSceneLayerSupplier,
                        mBrowserControlsSizer,
                        assertNonNull(mWindowAndroid.getInsetObserver()),
                        controlContainerTranslationSupplier,
                        keyboardAccessoryStateSupplier.getIsSheetShowingSupplier(),
                        this::isUrlBarFocused);
    }

    // TODO(b/315204103): add tests
    private void onReadAloudReadabilityUpdated() {
        // Update the button if ReadAloud is set as the customized button.
        if (ChromeSharedPreferences.getInstance().readInt(ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS)
                        == AdaptiveToolbarButtonVariant.READ_ALOUD
                && mInitializedWithNative) {
            updateButtonStatus();
        }
    }

    @Override
    public void updateObscured(boolean obscureTabContent, boolean obscureToolbar) {
        mControlContainer.setImportantForAccessibility(
                obscureToolbar
                        ? View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
                        : View.IMPORTANT_FOR_ACCESSIBILITY_AUTO);
    }

    /**
     * Handle a layout change event.
     *
     * @param layoutType The layout being switched to.
     */
    private void updateForLayout(@LayoutType int layoutType) {
        if (layoutType == LayoutType.TAB_SWITCHER) {
            mLocationBarModel.updateForNonStaticLayout();
            mToolbar.setTabSwitcherMode(true);
            mToolbarNavControlsEnabledSupplier.set(false);
            updateButtonStatus();
        }
        mIsTabSwitcherFinishedShowingSupplier.set(
                layoutType == LayoutType.TAB_SWITCHER && !mInTabSwitcherTransition);
        mToolbar.setContentAttached(layoutType == LayoutType.BROWSING);
    }

    private TopToolbarCoordinator createTopToolbarCoordinator(
            ToolbarControlContainer controlContainer,
            List<ButtonDataProvider> buttonDataProviders,
            ThemeColorProvider browsingModeThemeColorProvider,
            IncognitoStateProvider incognitoStateProvider,
            boolean initializeWithIncognitoColors,
            NullableObservableSupplier<@BrowserControlsState Integer> constraintsSupplier,
            @Nullable OnLongClickListener onLongClickListener,
            ToolbarProgressBar progressBar,
            HistoryDelegate historyDelegate,
            TopControlsStacker topControlsStacker,
            TabStripTopControlLayer tabStripTopControlLayer,
            MonotonicObservableSupplier<Profile> profileSupplier,
            ActivityResultTracker activityResultTracker,
            DeviceLockActivityLauncher deviceLockActivityLauncher,
            SnackbarManager snackbarManager) {
        TopToolbarCoordinator toolbar =
                new TopToolbarCoordinator(
                        controlContainer,
                        mToolbarLayout,
                        mLocationBarModel,
                        mToolbarTabController,
                        mUserEducationHelper,
                        buttonDataProviders,
                        mLayoutStateProviderSupplier,
                        browsingModeThemeColorProvider,
                        incognitoStateProvider,
                        mMenuButtonCoordinator,
                        assertNonNull(mMenuButtonCoordinator.getMenuButtonHelperSupplier()),
                        mTabSwitcherButtonCoordinator,
                        mOverridableTabCount.getObservable(),
                        mHomepageEnabledSupplier,
                        mCompositorViewHolder::getResourceManager,
                        historyDelegate,
                        initializeWithIncognitoColors,
                        constraintsSupplier,
                        mCompositorViewHolder.getInMotionSupplier(),
                        mControlsVisibilityDelegate,
                        mFullscreenManager,
                        mTabObscuringHandler,
                        mDesktopWindowStateManager,
                        mTabStripTransitionDelegateSupplier,
                        tabStripTopControlLayer,
                        onLongClickListener,
                        progressBar,
                        mActivityTabProvider.asObservable(),
                        mToolbarNavControlsEnabledSupplier,
                        mBackButtonCoordinator,
                        mForwardButtonCoordinator,
                        mHomeButtonCoordinator,
                        topControlsStacker,
                        mBrowserControlsSizer,
                        () ->
                                MultiWindowUtils.getInstanceCount(
                                        PersistedInstanceType.ACTIVE
                                                | PersistedInstanceType.OFF_THE_RECORD),
                        profileSupplier,
                        mOmniboxStubSupplier,
                        SigninAndHistorySyncActivityLauncherImpl.get(),
                        mWindowAndroid,
                        activityResultTracker,
                        deviceLockActivityLauncher,
                        mBottomSheetController,
                        mModalDialogManagerSupplier.get(),
                        snackbarManager);

        mHomepageStateListener =
                () -> {
                    mHomepageEnabledSupplier.set(
                            HomepageManager.getInstance()
                                    .shouldShowHomeButtonOnToolbar(isNewTabPage()));
                };

        HomepageManager.getInstance().addListener(mHomepageStateListener);
        mHomepageStateListener.onHomepageStateUpdated();

        return toolbar;
    }

    /**
     * Menu click handler on home button and records if user long presses on home button to edit
     * homepage on the new tab page.
     *
     * @param context {@link Context} used for launching a settings activity.
     */
    private void onHomeButtonMenuClick(Context context) {
        boolean isNtp = isNewTabPage();
        HomepageManager.getInstance().onMenuClick(context);
        if (isNtp) {
            BrowserUiUtils.recordModuleLongClickHistogram(ModuleTypeOnStartAndNtp.HOME_BUTTON);
        }
    }

    // TODO(crbug.com/430518884): Remove when the proper fix is implemented.
    /**
     * Sets accessibility order for incognito ntp.
     *
     * @param tab Chrome Tab.
     */
    private void applyIncognitoNtpAccessibilityOrder(@Nullable Tab tab) {
        final boolean isIncognitoNtp =
                tab != null
                        && tab.isIncognitoBranded()
                        && UrlUtilities.isNtpUrl(tab.getUrl())
                        && tab.getView() != null;

        if (isIncognitoNtp) {
            assumeNonNull(tab);
            View ntpView = tab.getView();
            assumeNonNull(ntpView);
            int ntpViewId = ntpView.getId();
            @ControlsPosition int controlsPosition = mBrowserControlsSizer.getControlsPosition();

            switch (controlsPosition) {
                case ControlsPosition.TOP:
                    mIncognitoNtpViewIdForA11y = View.NO_ID;
                    mToolbar.setAccessibilityTraversalAfter(View.NO_ID);
                    ntpView.setAccessibilityTraversalAfter(R.id.toolbar);
                    break;
                case ControlsPosition.BOTTOM:
                    mIncognitoNtpViewIdForA11y = ntpViewId;
                    ntpView.setAccessibilityTraversalAfter(View.NO_ID);
                    mToolbar.setAccessibilityTraversalAfter(ntpViewId);
                    break;
                default:
                    break;
            }
        } else {
            // Not on Incognito NTP. If accessibility order was previously
            // set with ntp view id, reset it.
            if (mIncognitoNtpViewIdForA11y != View.NO_ID
                    && mToolbar.getAccessibilityTraversalAfter() == mIncognitoNtpViewIdForA11y) {
                mToolbar.setAccessibilityTraversalAfter(View.NO_ID);
            }
            mIncognitoNtpViewIdForA11y = View.NO_ID;
        }
    }

    /** Returns a supplier that provides the NTP search box transition percentage. */
    public NonNullObservableSupplier<Float> getNtpSearchBoxTransitionPercentageSupplier() {
        return mNtpSearchBoxTransitionPercentageSupplier;
    }

    /** Returns the {@link OverridableTabCount} object to overwrite the tab count in the toolbar. */
    public OverridableTabCount getOverridableTabCount() {
        return mOverridableTabCount;
    }

    // Base abstract implementation of NewTabPageDelegate for phone/table toolbar layout.
    private abstract class ToolbarNtpDelegate implements NewTabPageDelegate {
        protected @Nullable NewTabPage mVisibleNtp;

        @Override
        public boolean wasShowingNtp() {
            return mVisibleNtp != null;
        }

        @Override
        public boolean isCurrentlyVisible() {
            return isNewTabPage();
        }

        @Override
        public boolean isIncognitoNewTabPageCurrentlyVisible() {
            return ToolbarManager.this.isIncognitoNewTabPageCurrentlyVisible();
        }

        @Override
        public boolean dispatchTouchEvent(MotionEvent ev) {
            assert mVisibleNtp != null;
            // No null check -- the toolbar should not be moved if we are not on an NTP.
            return mVisibleNtp.getView().dispatchTouchEvent(ev);
        }

        @Override
        public boolean isLocationBarShown() {
            NewTabPage ntp = getNewTabPageForCurrentTab();
            return ntp != null && ntp.isLocationBarShownInNtp();
        }

        @Override
        public boolean transitioningAwayFromLocationBar() {
            return mVisibleNtp != null
                    && mVisibleNtp.isLocationBarShownInNtp()
                    && !isLocationBarShown();
        }

        @Override
        public boolean hasCompletedFirstLayout() {
            NewTabPage newTabPage = getNewTabPageForCurrentTab();
            return newTabPage != null && newTabPage.hasCompletedFirstLayout();
        }

        @Override
        public void setSearchBoxScrollListener(Callback<Float> scrollCallback) {
            NewTabPage newVisibleNtp = getNewTabPageForCurrentTab();
            if (mVisibleNtp != null) {
                mVisibleNtp.setSearchBoxScrollListener(null);
                mNtpSearchBoxTransitionPercentageSupplier.set(0f);
            }
            mVisibleNtp = newVisibleNtp;
            if (mVisibleNtp != null && shouldUpdateListener()) {
                mVisibleNtp.setSearchBoxScrollListener(
                        (fraction) -> {
                            mNtpSearchBoxTransitionPercentageSupplier.set(fraction);
                            scrollCallback.onResult(fraction);
                        });
            }
        }

        // Boolean predicate that tells if the NewTabPage.OnSearchBoxScrollListener
        // should be updated or not
        protected abstract boolean shouldUpdateListener();

        @Override
        public void getSearchBoxBounds(Rect bounds, Point translation) {
            assert getNewTabPageForCurrentTab() != null;
            getNewTabPageForCurrentTab().getSearchBoxBounds(bounds, translation);
        }

        @Override
        public int getSearchBoxBoundsVerticalInset() {
            NewTabPage ntp = getNewTabPageForCurrentTab();
            return ntp != null ? ntp.getSearchBoxBoundsVerticalInset() : 0;
        }

        @Override
        public void setSearchBoxAlpha(float alpha) {
            assert getNewTabPageForCurrentTab() != null;
            getNewTabPageForCurrentTab().setSearchBoxAlpha(alpha);
        }

        @Override
        public void setSearchProviderLogoAlpha(float alpha) {
            assert getNewTabPageForCurrentTab() != null;
            getNewTabPageForCurrentTab().setSearchProviderLogoAlpha(alpha);
        }

        @Override
        public void setUrlFocusChangeAnimationPercent(float fraction) {
            NewTabPage ntp = getNewTabPageForCurrentTab();
            if (ntp != null) ntp.setUrlFocusChangeAnimationPercent(fraction);
        }
    }

    private NewTabPageDelegate createNewTabPageDelegate() {
        if (mToolbarLayout instanceof ToolbarPhone) {
            return new ToolbarNtpDelegate() {
                @Override
                protected boolean shouldUpdateListener() {
                    assumeNonNull(mVisibleNtp);
                    return mVisibleNtp.isLocationBarShownInNtp();
                }
            };
        } else if (mToolbarLayout instanceof ToolbarTablet) {
            return new ToolbarNtpDelegate() {
                @Override
                public void setSearchBoxScrollListener(Callback<Float> scrollCallback) {
                    if (mVisibleNtp == getNewTabPageForCurrentTab()) return;
                    super.setSearchBoxScrollListener(scrollCallback);
                }

                @Override
                protected boolean shouldUpdateListener() {
                    return true;
                }
            };
        }
        return NewTabPageDelegate.EMPTY;
    }

    private @Nullable NewTabPage getNewTabPageForCurrentTab() {
        if (mLocationBarModel.hasTab()) {
            Tab tab = mLocationBarModel.getTab();
            assumeNonNull(tab);
            NativePage nativePage = tab.getNativePage();
            if (nativePage instanceof NewTabPage) return (NewTabPage) nativePage;
        }
        return null;
    }

    private boolean isIncognitoNewTabPageCurrentlyVisible() {
        if (mLocationBarModel.hasTab()) {
            Tab tab = mLocationBarModel.getTab();
            assumeNonNull(tab);
            NativePage nativePage = tab.getNativePage();
            return nativePage instanceof IncognitoNewTabPage;
        }
        return false;
    }

    private boolean isWebUiNtp() {
        if (mLocationBarModel.hasTab()) {
            Tab tab = mLocationBarModel.getTab();
            return tab != null && UrlUtilities.isNtpUrl(tab.getUrl());
        }
        return false;
    }

    private boolean isNewTabPage() {
        return getNewTabPageForCurrentTab() != null || isWebUiNtp();
    }

    /**
     * @return Whether the UrlBar currently has focus.
     */
    public boolean isUrlBarFocused() {
        if (mLocationBar == null || mLocationBar.getOmniboxStub() == null) {
            return false;
        }
        return mLocationBar.getOmniboxStub().isUrlBarFocused();
    }

    /** Returns the UrlBar text excluding the autocomplete text. */
    public String getUrlBarTextWithoutAutocomplete() {
        assert mLocationBar instanceof LocationBarCoordinator
                : "LocationBar should be an instance of LocationBarCoordinator.";
        return ((LocationBarCoordinator) mLocationBar).getUrlBarTextWithoutAutocomplete();
    }

    /** Returns the {@link ViewRectProvider} for the UrlBar. */
    public ViewRectProvider getUrlBarViewRectProvider() {
        assert mLocationBar instanceof LocationBarCoordinator
                : "LocationBar should be an instance of LocationBarCoordinator.";
        return ((LocationBarCoordinator) mLocationBar).getUrlBarViewRectProvider();
    }

    /** Enable the bottom controls. */
    public void enableBottomControls() {
        enableTabGroupUiControls();
        if (BottomBarConfigUtils.isBottomBarEnabled(mActivity)) {
            enableBottomBar();
        }
    }

    private void enableTabGroupUiControls() {
        View tabGroupUiContainer =
                ((ViewStub) mActivity.findViewById(R.id.tab_group_ui_container_stub)).inflate();
        assert mTabGroupUiOneshotSupplier == null;
        assert mUndoBarThrottle != null;
        assert mLayoutManager != null;
        assert mTabModelSelector != null;
        assert mDataSharingTabManager != null;
        assert mBottomUiThemeColorProvider != null : "BottomUiThemeColorProvider should be set";
        mTabGroupUiOneshotSupplier =
                new TabGroupUiOneshotSupplier(
                        mActivityTabProvider,
                        mTabModelSelector,
                        mActivity,
                        tabGroupUiContainer.findViewById(R.id.bottom_container_slot),
                        mBrowserControlsSizer,
                        mScrimManager,
                        mOmniboxFocusStateSupplier,
                        mBottomSheetController,
                        mDataSharingTabManager,
                        mTabContentManager,
                        mTabCreatorManager,
                        mLayoutStateProviderSupplier,
                        mModalDialogManagerSupplier.get(),
                        mBottomUiThemeColorProvider,
                        mUndoBarThrottle,
                        mTabBookmarkerSupplier,
                        mShareDelegateSupplier);
        var tabGroupUiBottomControlsCoordinator =
                new BottomControlsCoordinator(
                        mWindowAndroid,
                        mLayoutManager,
                        mCompositorViewHolder.getResourceManager(),
                        mBottomControlsStacker,
                        mControlsVisibilityDelegate,
                        mFullscreenManager,
                        mEdgeToEdgeControllerSupplier,
                        mActivityTabProvider.asObservable(),
                        (ScrollingBottomViewResourceFrameLayout) tabGroupUiContainer,
                        LayerType.TABSTRIP_TOOLBAR,
                        R.dimen.tab_group_ui_height,
                        SupplierUtils.upcast(
                                mTabGroupUiOneshotSupplier, BottomControlsContentDelegate.class),
                        mTabObscuringHandler,
                        mLayoutManager.getOverlayPanelManager().getPanelStateSupplier(),
                        mConstraintsSupplier,
                        /* readAloudRestoringSupplier= */ () -> {
                            final var readAloud = mReadAloudControllerSupplier.get();
                            return readAloud != null && readAloud.isRestoringPlayer();
                        });
        if (mInitializedWithNative) {
            tabGroupUiBottomControlsCoordinator.initializeWithNative();
        }
        mTabGroupUiBottomControlsCoordinatorSupplier.set(tabGroupUiBottomControlsCoordinator);
        if (mBackPressManager != null) {
            mBackPressManager.addHandler(
                    tabGroupUiBottomControlsCoordinator, BackPressHandler.Type.BOTTOM_CONTROLS);
        }
    }

    private void enableBottomBar() {
        View bottomAppBarContainer =
                ((ViewStub) mActivity.findViewById(R.id.bottom_app_bar_container_stub)).inflate();
        assert mLayoutManager != null;

        var bottomBarContainerOneshotSupplier =
                new OneshotSupplierImpl<BottomControlsContentDelegate>();

        BottomBarContainerCoordinator bottomBarContainerCoordinator =
                new BottomBarContainerCoordinator(
                        bottomAppBarContainer.findViewById(R.id.bottom_container_slot),
                        mUserEducationHelper,
                        mBottomControlsStacker::requestLayerUpdate,
                        assumeNonNull(mActionRegistry),
                        mCurrentTabSupplier,
                        mAppThemeColorProvider,
                        mHomepageEnabledSupplier,
                        mProfileSupplier,
                        mOmniboxFocusStateSupplier);
        bottomBarContainerOneshotSupplier.set(bottomBarContainerCoordinator);

        if (mBottomBarHostManager != null) {
            mBottomBarHostManager.registerBottomBar(
                    bottomBarContainerCoordinator.getBottomBar(),
                    bottomBarContainerCoordinator::attachBottomBarView);
        }

        var bottomAppBarCoordinator =
                new BottomControlsCoordinator(
                        mWindowAndroid,
                        mLayoutManager,
                        mCompositorViewHolder.getResourceManager(),
                        mBottomControlsStacker,
                        mControlsVisibilityDelegate,
                        mFullscreenManager,
                        mEdgeToEdgeControllerSupplier,
                        mActivityTabProvider.asObservable(),
                        (ScrollingBottomViewResourceFrameLayout) bottomAppBarContainer,
                        LayerType.BOTTOM_APP_BAR,
                        R.dimen.bottom_bar_height,
                        bottomBarContainerOneshotSupplier,
                        mTabObscuringHandler,
                        mLayoutManager.getOverlayPanelManager().getPanelStateSupplier(),
                        mConstraintsSupplier,
                        /* readAloudRestoringSupplier= */ () -> {
                            final var readAloud = mReadAloudControllerSupplier.get();
                            return readAloud != null && readAloud.isRestoringPlayer();
                        });
        if (mInitializedWithNative) {
            bottomAppBarCoordinator.initializeWithNative();
        }
        mBottomAppBarCoordinatorSupplier.set(bottomAppBarCoordinator);
    }

    /**
     * TODO(https://crbug.com/1164216): Remove this getter in favor of extracting tab group feature
     * details from ChromeTabbedActivity directly.
     *
     * @return The coordinator for the tab group UI if it exists.
     */
    public @Nullable TabGroupUi getTabGroupUi() {
        return mTabGroupUiOneshotSupplier != null ? mTabGroupUiOneshotSupplier.get() : null;
    }

    /**
     * Initialize the manager with the components that had native initialization dependencies.
     *
     * <p>Calling this must occur after the native library have completely loaded.
     *
     * @param layoutManager A {@link LayoutManagerImpl} instance used to watch for scene changes.
     * @param stripLayoutHelperManager {@link StripLayoutHelperManager} instance used to manage the
     *     tab strip.
     * @param openGridTabSwitcherHandler The {@link Runnable} for the grid tab switcher.
     * @param bookmarkClickHandler The {@link OnClickListener} for the bookmark button.
     * @param customTabsBackClickHandler The {@link OnClickListener} for the custom tabs back
     *     button.
     * @param archivedTabCountSupplier Supplies the number of archived tabs.
     * @param tabModelNotificationDotSupplier Supplies whether the tab switcher button should show a
     *     notification dot.
     * @param undoBarThrottle For suppressing the undo bar.
     * @param contextMenuPopulatorFactory The {@link ContextMenuPopulatorFactory} to use.
     */
    public void initializeWithNative(
            LayoutManagerImpl layoutManager,
            @Nullable StripLayoutHelperManager stripLayoutHelperManager,
            @Nullable Runnable openGridTabSwitcherHandler,
            @Nullable OnClickListener bookmarkClickHandler,
            @Nullable OnClickListener customTabsBackClickHandler,
            @Nullable NonNullObservableSupplier<Integer> archivedTabCountSupplier,
            NonNullObservableSupplier<TabModelDotInfo> tabModelNotificationDotSupplier,
            @Nullable UndoBarThrottle undoBarThrottle,
            @Nullable ContextMenuPopulatorFactory contextMenuPopulatorFactory,
            @Nullable SelectionDropdownMenuDelegate selectionDropdownMenuDelegate) {
        TraceEvent.begin("ToolbarManager.initializeWithNative");
        assert !mInitializedWithNative;
        TabModelSelector tabModelSelector = mTabModelSelectorSupplier.get();
        assert tabModelSelector != null;

        mStripLayoutHelperManager = stripLayoutHelperManager;
        mUndoBarThrottle = undoBarThrottle;

        mTabModelSelector = tabModelSelector;
        if (mActionRegistry != null) {
            PropertyModel newTabModel = mActionRegistry.get(ActionId.NEW_TAB).get();
            assert newTabModel != null : "NEW_TAB action should be registered";
            newTabModel.set(
                    ActionProperties.ON_PRESS_CALLBACK,
                    v -> {
                        Tab currentTab = mActivityTabProvider.get();
                        if (currentTab != null
                                && BottomBarConfigUtils.isBottomBarEnabled(mActivity)) {
                            TrackerFactory.getTrackerForProfile(currentTab.getProfile())
                                    .notifyEvent(EventConstants.ANDROID_BOTTOM_BAR_NEW_TAB_USED);
                        }
                        TabCreatorUtil.launchNtp(
                                mTabCreatorManager.getTabCreator(
                                        tabModelSelector.isIncognitoSelected()));
                    });
        }

        if (mActionRegistry != null) {
            mGlicActionCoordinator =
                    new GlicActionCoordinator(
                            mActivity,
                            mActionRegistry,
                            mToggleGlicCallback,
                            mActivityTabProvider.asObservable(),
                            mChromeAndroidTaskSupplier,
                            mBrowserControlsSizer,
                            mTabModelSelectorSupplier,
                            mSnackbarManager);
        }

        Profile profile = tabModelSelector.getModel(false).getProfile();
        assert profile != null;

        mOverrideUrlLoadingDelegate.setOpenGridTabSwitcherCallback(openGridTabSwitcherHandler);

        ViewStub extensionsToolbarStub =
                mControlContainer.findViewById(R.id.extensions_toolbar_container_stub);
        if (extensionsToolbarStub != null) {
            ChromeAndroidTask task = mChromeAndroidTaskSupplier.get();
            // ChromeAndroidTask is available only on Desktop Android.
            if (task != null) {
                mExtensionsToolbarCoordinator =
                        ExtensionsToolbarCoordinator.maybeCreate(
                                mActivity,
                                extensionsToolbarStub,
                                mWindowAndroid,
                                task,
                                assertNonNull(mTabModelSelector.getCurrentModel().getProfile()),
                                mActivityTabProvider.asObservable(),
                                mTabCreatorManager.getTabCreator(false),
                                getBrowsingModeThemeColorProvider(),
                                (ToolbarTablet) mToolbarLayout,
                                contextMenuPopulatorFactory,
                                selectionDropdownMenuDelegate,
                                mTabModelSelector,
                                mModalDialogManagerSupplier.get());
                if (mExtensionsToolbarCoordinator != null) {
                    mToolbar.setExtensionsToolbarCoordinator(mExtensionsToolbarCoordinator);
                }
            }
        }

        // Must be initialized before Toolbar attempts to use it.
        mLocationBarModel.initializeWithNative();
        if (mTabSwitcherButtonCoordinator != null) {
            OnLongClickListener tabSwitcherLongClickListener =
                    createTabSwitcherLongClickListener(profile, openGridTabSwitcherHandler);
            mTabSwitcherButtonCoordinator.initializeWithNative(
                    v -> openGridTabSwitcherHandler.run(),
                    tabSwitcherLongClickListener,
                    mOverridableTabCount.getObservable(),
                    archivedTabCountSupplier,
                    tabModelNotificationDotSupplier,
                    () -> TabArchiveSettings.setIphShownThisSession(true),
                    () -> TabArchiveSettings.setIphShownThisSession(false));
        }

        if (mActionRegistry != null) {
            OnLongClickListener tabSwitcherLongClickListener =
                    createTabSwitcherLongClickListener(profile, openGridTabSwitcherHandler);

            mTabSwitcherActionProvider =
                    new TabSwitcherActionProvider(
                            mActionRegistry,
                            mUserEducationHelper,
                            assumeNonNull(mTabModelSelectorSupplier.get()),
                            mIncognitoStateProvider,
                            mOverridableTabCount,
                            tabModelNotificationDotSupplier,
                            mPromoShownOneshotSupplier,
                            archivedTabCountSupplier,
                            mLayoutStateProviderSupplier,
                            () -> openGridTabSwitcherHandler.run(),
                            v -> tabSwitcherLongClickListener.onLongClick(v),
                            () -> TabArchiveSettings.setIphShownThisSession(true),
                            () -> TabArchiveSettings.setIphShownThisSession(false));
        }

        if (stripLayoutHelperManager != null) {
            mTabStripTopControlLayer.initializeWithNative(stripLayoutHelperManager);
        }
        mToolbar.initializeWithNative(
                profile,
                layoutManager::requestUpdate,
                bookmarkClickHandler,
                customTabsBackClickHandler,
                layoutManager,
                mActivityTabProvider.asObservable(),
                mBrowserControlsSizer,
                mToolbarThemeColorProvider,
                mBottomToolbarControlsOffsetSupplier,
                mSuppressToolbarSceneLayerSupplier,
                mToolbarProgressBarLayer::onProgressBarInfoUpdate,
                mCaptureResourceIdSupplier,
                mTabStripTopControlLayer);

        // This call is mainly to ensure the tab strip height stays the same as the internal value
        // from TabStripTransitionCoordinator. When canForceTopChromeHeightAdjustmentOnStartup()
        // returns true, TabStripTopControlLayer is already adjusted to this internal height at an
        // earlier timing, so this call can be skipped.
        // TODO(crbug.com/450970998): This call can be remove once feature launch.
        if (!BrowserControlsUtils.isForceTopChromeHeightAdjustmentOnStartupEnabled(mActivity)) {
            mTabStripTopControlLayer.set(mToolbar.getTabStripHeight());
        }

        mAttachStateChangeListener =
                new OnAttachStateChangeListener() {
                    @Override
                    public void onViewDetachedFromWindow(View v) {}

                    @Override
                    public void onViewAttachedToWindow(View v) {
                        // As we have only just registered for notifications, any that were sent
                        // prior to this may have been missed. Calling refreshSelectedTab in case
                        // we missed the initial selection notification.
                        refreshSelectedTab(mActivityTabProvider.get());
                    }
                };

        mToolbar.addOnAttachStateChangeListener(mAttachStateChangeListener);

        mLayoutManager = layoutManager;

        if (stripLayoutHelperManager != null) {
            mControlContainer.setToolbarContainerDragListener(
                    stripLayoutHelperManager.getDragListener());

            mTabStripTransitionDelegateSupplier.set(stripLayoutHelperManager);
            stripLayoutHelperManager.setIsTabStripHiddenByHeightTransition(
                    mToolbar.getTabStripHeight() == 0);
        }

        mUpdateMenuItemHelper = UpdateMenuItemHelper.getInstance(profile);
        if (mMenuStateObserver != null) {
            mUpdateMenuItemHelper.registerObserver(mMenuStateObserver);
        }

        mIncognitoNtpOmniboxAutofocusManager =
                IncognitoNtpOmniboxAutofocusManager.maybeCreate(
                        mActivity,
                        getOmniboxStub(),
                        mLayoutManager,
                        tabModelSelector,
                        IncognitoNtpUtils::getIncognitoNtpView,
                        IncognitoNtpUtils::getIncognitoNtpScrollView,
                        IncognitoNtpUtils::getIncognitoNtpContentMetrics);

        mInitializedWithNative = true;
        tabModelSelector
                .getCurrentTabModelSupplier()
                .addSyncObserverAndPostIfNonNull(mCurrentTabModelObserver);
        refreshSelectedTab(mActivityTabProvider.get());
        maybeShowUrlBarCursorIfHardwareKeyboardAvailable();
        mAppThemeColorProvider.setIncognitoStateProvider(mIncognitoStateProvider);

        BottomControlsCoordinator tabGroupUiBottomControlsCoordinator =
                mTabGroupUiBottomControlsCoordinatorSupplier.get();
        if (tabGroupUiBottomControlsCoordinator != null) {
            tabGroupUiBottomControlsCoordinator.initializeWithNative();
        }
        BottomControlsCoordinator bottomAppBarCoordinator = mBottomAppBarCoordinatorSupplier.get();
        if (bottomAppBarCoordinator != null) {
            bottomAppBarCoordinator.initializeWithNative();
        }

        if (mOnInitializedRunnable != null) {
            mOnInitializedRunnable.run();
            mOnInitializedRunnable = null;
        }

        // Allow bitmap capturing once everything has been initialized.
        Tab currentTab = tabModelSelector.getCurrentTab();
        if (currentTab != null
                && currentTab.getWebContents() != null
                && !currentTab.getUrl().isEmpty()) {
            mControlContainer.setReadyForBitmapCapture(true);
        }

        TraceEvent.end("ToolbarManager.initializeWithNative");
    }

    /**
     * @return The toolbar interface that this manager handles.
     */
    public Toolbar getToolbar() {
        return mToolbar;
    }

    @Override
    public @Nullable View getMenuButtonView() {
        if (mMenuButtonCoordinator == null) return null;
        MenuButton button = mMenuButtonCoordinator.getMenuButton();
        if (button == null) return null;
        return button.getImageButton();
    }

    /**
     * TODO(twellington): Try to remove this method. It's only used to return an in-product help
     * bubble anchor view... which should be moved out of tab and perhaps into the status bar icon
     * component.
     *
     * @return The view containing the security icon.
     */
    public View getSecurityIconView() {
        return mLocationBar.getSecurityIconView();
    }

    /**
     * Adds a custom action button to the {@link Toolbar}, if it is supported.
     *
     * @param drawable The {@link Drawable} to use as the background for the button.
     * @param description The content description for the custom action button.
     * @param listener The {@link OnClickListener} to use for clicks to the button.
     * @param type The {@link ButtonType} of the action button.
     * @see #updateCustomActionButton
     */
    public void addCustomActionButton(
            Drawable drawable, String description, OnClickListener listener, int type) {
        mToolbar.addCustomActionButton(drawable, description, listener, type);
    }

    /**
     * Updates the visual appearance of a custom action button in the {@link Toolbar}, if it is
     * supported.
     *
     * @param index The index of the button to update.
     * @param drawable The {@link Drawable} to use as the background for the button.
     * @param description The content description for the custom action button.
     * @see #addCustomActionButton
     */
    public void updateCustomActionButton(int index, Drawable drawable, String description) {
        mToolbar.updateCustomActionButton(index, drawable, description);
    }

    /**
     * Sets the delegate for the optional button.
     *
     * @param delegate The {@link OptionalBrowsingModeButtonController.Delegate}.
     */
    public void setOptionalButtonDelegate(OptionalBrowsingModeButtonController.Delegate delegate) {
        mToolbar.setOptionalButtonDelegate(delegate);
    }

    /** Call to tear down all of the toolbar dependencies. */
    @SuppressWarnings("NullAway")
    public void destroy() {
        mIsDestroyed = true;

        if (mIncognitoNtpOmniboxAutofocusManager != null) {
            mIncognitoNtpOmniboxAutofocusManager.destroy();
            mIncognitoNtpOmniboxAutofocusManager = null;
        }

        var omnibox = mLocationBar.getOmniboxStub();
        if (omnibox != null) {
            omnibox.removeUrlFocusChangeListener(this);
            omnibox.removeUrlFocusChangeListener(mStatusBarColorController);
        }
        mLocationBar.removeOmniboxSuggestionsDropdownScrollListener(mStatusBarColorController);

        if (mInitializedWithNative) {
            mFindToolbarManager.removeObserver(mFindToolbarObserver);
        }
        if (mTabModelSelectorSupplier != null) {
            mTabModelSelectorSupplier = null;
        }
        if (mTabModelSelector != null) {
            mTabModelSelector.getCurrentTabModelSupplier().removeObserver(mCurrentTabModelObserver);
        }
        if (mTabGroupUiOneshotSupplier != null) {
            mTabGroupUiOneshotSupplier.destroy();
            mTabGroupUiOneshotSupplier = null;
        }
        if (mBookmarkModelSupplier != null) {
            BookmarkModel bridge = mBookmarkModelSupplier.get();
            if (bridge != null) bridge.removeObserver(mBookmarksObserver);

            mBookmarkModelSupplier.removeObserver(mBookmarkModelSupplierObserver);
            mBookmarkModelSupplier = null;
        }
        if (mTemplateUrlObserver != null) {
            mTemplateUrlService.removeObserver(mTemplateUrlObserver);
            mTemplateUrlObserver = null;
        }
        if (mLayoutStateProvider != null) {
            mLayoutStateProvider.removeObserver(mLayoutStateObserver);
            mLayoutStateProvider = null;
        }

        if (mLayoutStateProviderSupplier != null) {
            mLayoutStateProviderSupplier = null;
        }

        if (mLayoutManager != null) {
            mLayoutManager = null;
        }

        HomepageManager.getInstance().removeListener(mHomepageStateListener);

        BottomControlsCoordinator tabGroupUiBottomControlsCoordinator =
                mTabGroupUiBottomControlsCoordinatorSupplier.get();
        if (tabGroupUiBottomControlsCoordinator != null) {
            tabGroupUiBottomControlsCoordinator.destroy();
            mTabGroupUiBottomControlsCoordinatorSupplier = null;
        }

        BottomControlsCoordinator bottomAppBarCoordinator = mBottomAppBarCoordinatorSupplier.get();
        if (bottomAppBarCoordinator != null) {
            bottomAppBarCoordinator.destroy();
            mBottomAppBarCoordinatorSupplier = null;
        }

        if (mLocationBar != null) {
            mLocationBar.destroy();
            mLocationBar = null;
        }

        if (mAttachStateChangeListener != null) {
            mToolbar.removeOnAttachStateChangeListener(mAttachStateChangeListener);
            mAttachStateChangeListener = null;
        }
        mTabStripTopControlLayer.destroy();
        mToolbar.destroy();
        mToolbarLongPressMenuHandler.destroy();
        mLocationBarModel.destroy();
        mHandler.removeCallbacksAndMessages(null); // Cancel delayed tasks.
        mBrowserControlsSizer.removeObserver(mBrowserControlsObserver);
        mFullscreenManager.removeObserver(mFullscreenObserver);

        if (mToolbarThemeColorProvider != null) {
            mToolbarThemeColorProvider.removeThemeColorObserver(this);
        }

        if (mAdjustedToolbarThemeColorProvider != null) {
            mAdjustedToolbarThemeColorProvider.removeThemeColorObserver(this);
        }

        if (mAppThemeColorProvider != null) {
            mAppThemeColorProvider.removeTintObserver(this);
            mAppThemeColorProvider.destroy();
            mAppThemeColorProvider = null;
        }

        if (mActivityTabTabObserver != null) {
            mActivityTabTabObserver.destroy();
            mActivityTabTabObserver = null;
        }
        mToolbarProgressBarLayer.destroy();
        if (mProgressBarCoordinator != null) mProgressBarCoordinator.destroy();

        if (mFindToolbarManager != null) {
            mFindToolbarManager.removeObserver(mFindToolbarObserver);
            mFindToolbarManager = null;
        }

        if (mMenuButtonCoordinator != null) {
            if (mMenuStateObserver != null && mUpdateMenuItemHelper != null) {
                mUpdateMenuItemHelper.unregisterObserver(mMenuStateObserver);
            }
            mMenuStateObserver = null;

            mMenuButtonCoordinator.destroy();
            mMenuButtonCoordinator = null;
        }

        if (mBackButtonCoordinator != null) {
            mBackButtonCoordinator.destroy();
        }

        if (mHomeButtonCoordinator != null) {
            mHomeButtonCoordinator.destroy();
        }

        if (mOverviewModeMenuButtonCoordinator != null) {
            mOverviewModeMenuButtonCoordinator.destroy();
            mOverviewModeMenuButtonCoordinator = null;
        }

        mUpdateMenuItemHelper = null;

        if (mTabSwitcherButtonCoordinator != null) {
            mTabSwitcherButtonCoordinator.destroy();
            mTabSwitcherButtonCoordinator = null;
        }

        if (mTabSwitcherActionProvider != null) {
            mTabSwitcherActionProvider.destroy();
            mTabSwitcherActionProvider = null;
        }

        if (mGlicActionCoordinator != null) {
            mGlicActionCoordinator.destroy();
            mGlicActionCoordinator = null;
        }

        if (mExtensionsToolbarCoordinator != null) {
            mExtensionsToolbarCoordinator.destroy();
            mExtensionsToolbarCoordinator = null;
        }

        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }

        if (mReadAloudControllerSupplier.get() != null) {
            mReadAloudControllerSupplier
                    .get()
                    .removeReadabilityUpdateListener(mReadAloudReadabilityCallback);
        }

        if (mDesktopWindowStateManager != null) {
            mDesktopWindowStateManager.removeObserver(mControlContainer);
        }

        if (mToolbarPositionController != null) {
            mToolbarPositionController.destroy();
            mToolbarPositionController = null;
        }

        if (mMiniOriginBarController != null) {
            mMiniOriginBarController.destroy();
            mMiniOriginBarController = null;
        }

        if (mOverridableTabCount != null) {
            mOverridableTabCount.destroy();
            mOverridableTabCount = null;
        }

        removeSideUiObservers();

        mTabObscuringHandler.removeObserver(this);

        mActivity.unregisterComponentCallbacks(mComponentCallbacks);
        mComponentCallbacks = null;

        mControlContainer.destroy();
        mConstraintsSupplier.destroy();
        mLocationBarFocusHandler.destroy();

        mWindowAndroid.setProgressBarConfigProvider(null);

        mXrSpaceModeObservableSupplier.removeObserver(mOnXrSpaceModeChanged);
    }

    private void removeSideUiObservers() {
        if (mSideUiStateProvider != null) {
            if (mSideUiObserver != null) {
                mSideUiStateProvider.removeObserver(mSideUiObserver);
            }
            if (mControlContainerSideUiObserver != null) {
                mSideUiStateProvider.removeObserver(mControlContainerSideUiObserver);
            }
            if (mProgressBarSideUiObserver != null) {
                mSideUiStateProvider.removeObserver(mProgressBarSideUiObserver);
            }
        }
    }

    /** Called when the orientation of the activity has changed. */
    private void onOrientationChange() {
        if (mActionModeController != null) mActionModeController.showControlsOnOrientationChange();
    }

    @VisibleForTesting
    String homepageUrl() {
        Profile profile = mProfileSupplier.get();
        boolean isIncognito = profile != null && profile.isOffTheRecord();
        GURL homepageGurl = HomepageManager.getInstance().getHomepageGurl(isIncognito);
        assert !homepageGurl.isEmpty();
        return homepageGurl.getSpec();
    }

    private void registerTemplateUrlObserver() {
        assert mTemplateUrlObserver == null;
        assert mTemplateUrlService != null;
        mTemplateUrlObserver =
                new TemplateUrlServiceObserver() {
                    private @Nullable TemplateUrl mSearchEngine =
                            mTemplateUrlService.getDefaultSearchEngineTemplateUrl();

                    @Override
                    public void onTemplateURLServiceChanged() {
                        TemplateUrl searchEngine =
                                mTemplateUrlService.getDefaultSearchEngineTemplateUrl();
                        if ((mSearchEngine == null && searchEngine == null)
                                || (mSearchEngine != null && mSearchEngine.equals(searchEngine))) {
                            return;
                        }

                        mSearchEngine = searchEngine;
                        mToolbar.onDefaultSearchEngineChanged();
                        checkIfNtpShowingWithNoPendingLoad();
                    }
                };
        mTemplateUrlService.addObserver(mTemplateUrlObserver);
    }

    // TODO(crbug.com/40585866): remove the below two methods if possible.
    public boolean back() {
        return mToolbarTabController.back();
    }

    public boolean forward() {
        return mToolbarTabController.forward();
    }

    /**
     * Triggered when the URL input field has gained or lost focus.
     *
     * @param hasFocus Whether the URL field has gained focus.
     */
    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        // Detect and report Omnibox sessions originating from CCT if Search in CCT is disabled.
        // This is important to reduce CCT ActivityType bleeding into Chrome Browser.
        if (!ChromeFeatureList.sSearchInCCT.isEnabled()
                && UmaActivityObserver.getCurrentActivityType() == ActivityType.CUSTOM_TAB) {
            // TODO(b/339910285): Remove this once bleeding is negligile or eliminated completely.
            JavaExceptionReporter.reportException(
                    new Exception(
                            "NOT A CRASH: Unexpected ActivityType reported by"
                                    + " UmaActivityObserver"));
        }

        mToolbar.onUrlFocusChange(hasFocus);

        if (mFindToolbarManager != null && hasFocus) mFindToolbarManager.hideToolbar();

        if (mControlsVisibilityDelegate == null) return;
        if (hasFocus) {
            mFullscreenFocusToken =
                    mControlsVisibilityDelegate.showControlsPersistentAndClearOldToken(
                            mFullscreenFocusToken);
        } else {
            mControlsVisibilityDelegate.releasePersistentShowingToken(mFullscreenFocusToken);
        }

        mUrlFocusChangedCallback.onResult(hasFocus);
    }

    @Override
    public void onUrlAnimationFinished(boolean hasFocus) {
        mToolbar.onUrlAnimationFinished(hasFocus);
    }

    /** See {@link mBottomToolbarControlsOffsetSupplier} */
    public NonNullObservableSupplier<Integer> getBottomToolbarOffsetSupplier() {
        return mBottomToolbarControlsOffsetSupplier;
    }

    /** Get the supplier for the current height of the tab strip. Always returns a valid integer. */
    public NonNullObservableSupplier<Integer> getTabStripHeightSupplier() {
        return mTabStripTopControlLayer.getSupplier();
    }

    /** Return the TabStripTransitionCoordinator. */
    public @Nullable TabStripTransitionCoordinator getTabStripTransitionCoordinator() {
        return mToolbar.getTabStripTransitionCoordinator();
    }

    /**
     * @return The {@link StatusBarColorController} instance maintained by this class.
     */
    public StatusBarColorController getStatusBarColorController() {
        return mStatusBarColorController;
    }

    /**
     * Updates the primary color used by the model to the given color.
     *
     * @param color The primary color for the current tab.
     * @param shouldAnimate Whether the change of color should be animated.
     */
    @Override
    public void onThemeColorChanged(int color, boolean shouldAnimate) {
        if (!mShouldUpdateToolbarPrimaryColor) return;

        boolean colorChanged = mCurrentThemeColor != color;
        if (!colorChanged) return;

        mCurrentThemeColor = color;
        mLocationBarModel.setPrimaryColor(color);
        mToolbar.onPrimaryColorChanged(shouldAnimate);
        // TODO(https://crbug.com/40585866, pnoland): Rationalize theme color logic
        // into a set of documented, self-contained providers that we can inject to the appropriate
        // sub-components. That will let us have every component handle its own coloring, and remove
        // onThemeColorChanged from ToolbarManager.
        mCustomTabThemeColorProvider.setPrimaryColor(color, shouldAnimate);
    }

    @Override
    public void onTintChanged(
            @Nullable ColorStateList tint,
            @Nullable ColorStateList activityFocusTint,
            @BrandedColorScheme int brandedColorScheme) {
        updateBookmarkButtonStatus();

        if (mShouldUpdateToolbarPrimaryColor) {
            mCustomTabThemeColorProvider.setTint(tint, brandedColorScheme);
        }
    }

    /**
     * @param shouldUpdate Whether we should be updating the toolbar primary color based on updates
     *     from the Tab.
     */
    public void setShouldUpdateToolbarPrimaryColor(boolean shouldUpdate) {
        mShouldUpdateToolbarPrimaryColor = shouldUpdate;
    }

    /**
     * @return The primary toolbar color.
     */
    public int getPrimaryColor() {
        return mLocationBarModel.getPrimaryColor();
    }

    /**
     * @return The {@link EdgeToEdgeController} supplier.
     */
    public Supplier<EdgeToEdgeController> getEdgeToEdgeControllerSupplier() {
        return mEdgeToEdgeControllerSupplier;
    }

    /** Sets the visibility of the Toolbar shadow. */
    public void setToolbarShadowVisibility(int visibility) {
        if (mToolbarHairline != null) mToolbarHairline.setVisibility(visibility);
    }

    /**
     * We use getTopControlOffset to position the top controls. However, the toolbar's height may be
     * less than the total top controls height. If that's the case, this method will return the
     * extra offset needed to align the toolbar at the bottom of the top controls.
     *
     * @return The extra Y offset for the toolbar in pixels.
     */
    private int getToolbarExtraYOffset() {
        final int toolbarHairlineHeight = mToolbarHairline.getHeight();
        final int controlContainerHeight = mControlContainer.getHeight();
        final int bookmarkBarHeight =
                mBookmarkBarHeightSupplier != null ? mBookmarkBarHeightSupplier.get() : 0;

        // Offset can't be calculated if control container height isn't known yet.
        if (controlContainerHeight == 0) {
            return 0;
        }

        final int extraYOffset =
                mBrowserControlsSizer.getTopControlsHeight()
                        - (controlContainerHeight - toolbarHairlineHeight)
                        - bookmarkBarHeight;

        // There are cases where extraYOffset can be negative e.g. during tab strip transitioning
        // from invisible -> visible.
        return Math.max(0, extraYOffset);
    }

    /**
     * Sets the drawable that the close button shows, or hides it if {@code drawable} is {@code
     * null}.
     */
    public void setCloseButtonDrawable(@Nullable Drawable drawable) {
        mToolbar.setCloseButtonImageResource(drawable);
    }

    /**
     * Sets custom actions visibility of the custom tab toolbar.
     *
     * @param isVisible true if should be visible, false if should be hidden.
     */
    public void setCustomActionsVisibility(boolean isVisible) {
        mToolbar.setCustomActionsVisibility(isVisible);
    }

    /**
     * Hides menu button persistently until all tokens are released.
     *
     * @param oldToken previously acquired token.
     * @return a new token that keeps menu button hidden.
     */
    public int hideMenuButtonPersistently(int oldToken) {
        return mMenuButtonCoordinator.hideWithOldTokenRelease(oldToken);
    }

    /**
     * Releases menu button hide token that might cause menu button to become visible if no more
     * tokens are held.
     *
     * @param token previously acquired token.
     */
    public void releaseHideMenuButtonToken(int token) {
        mMenuButtonCoordinator.releaseHideToken(token);
    }

    /**
     * Sets whether a title should be shown within the Toolbar.
     *
     * @param showTitle Whether a title should be shown.
     */
    public void setShowTitle(boolean showTitle) {
        mToolbar.setShowTitle(showTitle);
    }

    /**
     * @see TopToolbarCoordinator#setUrlBarHidden(boolean)
     */
    public void setUrlBarHidden(boolean hidden) {
        mToolbar.setUrlBarHidden(hidden);
    }

    /**
     * Focuses or unfocuses the URL bar.
     *
     * <p>If you request focus and the UrlBar was already focused, this will select all of the text.
     *
     * @param focused Whether URL bar should be focused.
     * @param reason The given reason.
     */
    public void setUrlBarFocus(boolean focused, @OmniboxFocusReason int reason) {
        setUrlBarFocusAndText(focused, reason, null);
    }

    /**
     * Sets a new anchor view for the progress bar, which is anchored to the bottom of a given view.
     * By default the progress bar is anchored to the control_container, but when the Bookmark Bar
     * is visible, it should be anchored below that.
     *
     * @param anchorId The ID of the new anchor view
     */
    public void setProgressBarAnchorView(int anchorId) {
        // TODO(crbug.com/417238089): Position should be controlled by the TopControlsStacker.
        CoordinatorLayout.LayoutParams params =
                (CoordinatorLayout.LayoutParams) mProgressBarContainer.getLayoutParams();
        params.setAnchorId(anchorId);
        mProgressBarContainer.setLayoutParams(params);
    }

    /**
     * Same as {@code #setUrlBarFocus(boolean, @OmniboxFocusReason int)}, with the additional option
     * to set URL bar text.
     *
     * @param focused Whether URL bar should be focused.
     * @param reason The given reason.
     * @param text The URL bar text. {@code null} if no text is to be set.
     */
    public void setUrlBarFocusAndText(
            boolean focused, @OmniboxFocusReason int reason, @Nullable String text) {
        if (!mInitializedWithNative || mIsDestroyed) return;
        OmniboxStub omniboxStub = mLocationBar.getOmniboxStub();
        if (omniboxStub == null) return;
        if (focused) {
            omniboxStub.beginInput(
                    new AutocompleteInput()
                            .setUserText(text)
                            .setSelection(0, Integer.MAX_VALUE)
                            .setFocusReason(reason));
        } else {
            omniboxStub.endInput();
        }
    }

    /**
     * See {@link #setUrlBarFocus}, but if native is not loaded it will queue the request instead of
     * dropping it.
     */
    public void setUrlBarFocusOnceNativeInitialized(
            boolean focused, @OmniboxFocusReason int reason) {
        if (mInitializedWithNative) {
            setUrlBarFocus(focused, reason);
            return;
        }

        if (focused) {
            // Remember requests to focus the Url bar and replay them once native has been
            // initialized. This is important for the Launch to Incognito Tab flow (see
            // IncognitoTabLauncher.
            mOnInitializedRunnable =
                    () -> {
                        setUrlBarFocus(focused, reason);
                    };
        } else {
            mOnInitializedRunnable = null;
        }
    }

    /**
     * Reverts any pending edits of the location bar and reset to the page state. This does not
     * change the focus state of the location bar.
     */
    public void revertLocationBarChanges() {
        mLocationBar.revertChanges();
    }

    /**
     * Handle all necessary tasks that can be delayed until initialization completes.
     *
     * @param activityCreationTimeMs The time of creation for the activity this toolbar belongs to.
     * @param activityName Simple class name for the activity this toolbar belongs to.
     */
    public void onDeferredStartup(final long activityCreationTimeMs, final String activityName) {
        mLocationBar.onDeferredStartup();
    }

    /** Finish any toolbar animations. */
    public void finishAnimations() {
        if (mInitializedWithNative) mToolbar.finishAnimations();
    }

    /**
     * @return The current {@link LoadProgressCoordinator}.
     */
    public LoadProgressCoordinator getProgressBarCoordinator() {
        return mProgressBarCoordinator;
    }

    /**
     * Updates the current button states and calls appropriate abstract visibility methods, giving
     * inheriting classes the chance to update the button visuals as well.
     */
    private void updateButtonStatus() {
        if (mIsDestroyed) {
            assert false;
            return;
        }

        Tab currentTab = mLocationBarModel.getTab();
        boolean tabCrashed = currentTab != null && SadTab.isShowing(currentTab);
        boolean shouldShowOnToolbar =
                HomepageManager.getInstance().shouldShowHomeButtonOnToolbar(isNewTabPage());
        mHomepageEnabledSupplier.set(shouldShowOnToolbar);

        mToolbar.updateButtonVisibility();
        onBackPressStateChanged();
        updateReloadState(tabCrashed);
        updateBookmarkButtonStatus();
        mToolbarLayout.updateMenuButtonVisibility();
    }

    private void updateBookmarkButtonStatus() {
        if (mBookmarkModelSupplier == null) return;
        Tab currentTab = mLocationBarModel.getTab();
        BookmarkModel bridge = mBookmarkModelSupplier.get();
        boolean isBookmarked =
                currentTab != null && bridge != null && bridge.hasBookmarkIdForTab(currentTab);
        boolean editingAllowed =
                currentTab == null || bridge == null || bridge.isEditBookmarksEnabled();
        mToolbar.updateBookmarkButton(isBookmarked, editingAllowed);
    }

    private void updateReloadState(boolean tabCrashed) {
        Tab currentTab = mLocationBarModel.getTab();
        boolean isLoading = false;
        if (!tabCrashed) {
            isLoading = (currentTab != null && currentTab.isLoading()) || !mInitializedWithNative;
        }
        mMenuButtonCoordinator.updateReloadingState(isLoading);
    }

    /** Triggered when the selected tab has changed. */
    private void refreshSelectedTab(@Nullable Tab tab) {
        boolean wasIncognitoBranded = mLocationBarModel.isIncognitoBranded();
        Tab previousTab = mLocationBarModel.getTab();

        assumeNonNull(mTabModelSelector);
        Profile profile =
                tab != null ? tab.getProfile() : mTabModelSelector.getCurrentModel().getProfile();
        assert profile != null
                : "Failed to get Profile when offTheRecord = "
                        + mTabModelSelector.isOffTheRecordModelSelected();

        if (mBackPressHandler != null) mBackPressHandler.handleOnBackCancelled();

        mLocationBarModel.setTab(tab, profile);
        updateTabLoadingState(true);

        boolean isIncognitoBranded = profile.isIncognitoBranded();
        // This method is called prior to action mode destroy callback for incognito <-> normal
        // tab switch. Makes sure the action mode toolbar is hidden before selecting the new tab.
        if (previousTab != null && wasIncognitoBranded != isIncognitoBranded && mIsTablet) {
            mActionModeController.startHideAnimation();
        }
        // NOTE: Here we're not checking if isIncognitoBranded has changed because it's redundant
        // when we're already checking if tab has changed.
        if (previousTab != tab) {
            boolean isBottomBarEnabled = BottomBarConfigUtils.isBottomBarEnabled(mActivity);
            boolean isBottomPosition = mToolbarPositionSupplier.get() == ControlsPosition.BOTTOM;
            int defaultPrimaryColor =
                    (isBottomBarEnabled && isBottomPosition)
                            ? IncognitoColors.getColorSurfaceContainerHigh(
                                    mActivity, isIncognitoBranded)
                            : ChromeColors.getDefaultThemeColor(mActivity, isIncognitoBranded);

            int primaryColor =
                    tab != null
                            ? mToolbarThemeColorProvider.getToolbarBackgroundColor(tab)
                            : defaultPrimaryColor;
            // TODO(jinsukkim): Let TopUiThemeColorProvider handle this by updating the theme color.
            onThemeColorChanged(primaryColor, false);

            onTabOrModelChanged();

            if (tab != null) {
                mToolbar.onNavigatedToDifferentPage();
            }
        }

        updateButtonStatus();
        mCurrentTabSupplier.set(tab);
        mFormFieldFocusedSupplier.onWebContentsChanged(tab == null ? null : tab.getWebContents());
    }

    private void onTabOrModelChanged() {
        mToolbar.onTabOrModelChanged();
        checkIfNtpLoaded();
    }

    private void maybeShowBottomToolbarIph() {
        if (!ToolbarPositionController.isToolbarPositionCustomizationEnabled(
                        mActivity, mIsCustomTab)
                || mLocationBarModel.getCurrentGurl().isEmpty()
                || UrlUtilities.isNtpUrl(mLocationBarModel.getCurrentGurl())) {
            return;
        }

        mIphController.showBottomToolbarIph(mControlContainer.findViewById(R.id.location_bar));
    }

    /**
     * This method and checkIfNtpLoaded encode different concepts of the current tab's NTP-ness"
     * which are suitable for different use cases. checkIfNtpShowingWithNoPendingLoad checks that
     * the NTP is loaded and that there is no pending load away from the NTP; this is suitable for
     * cases where we need to update the UI as soon an extra-NTP load begins.In contrast,
     * checkIfNtpLoaded checks only that the NTP is showing, which will remain true until
     * didFinishNavigationInPrimaryMainFrame causes the tab to hide the NTP NativePage and render
     * the new page.
     */
    private void checkIfNtpShowingWithNoPendingLoad() {
        boolean isNtpUrl = UrlUtilities.isNtpUrl(mLocationBarModel.getCurrentGurl());
        if (isNtpUrl && getNewTabPageForCurrentTab() != null) {
            mIsNtpWithFakeboxShowingSupplier.set(NewTabPage.isInSingleUrlBarMode(mIsTablet));
        } else {
            mIsNtpWithFakeboxShowingSupplier.set(false);
            maybeShowBottomToolbarIph();
        }
    }

    private void checkIfNtpLoaded() {
        NewTabPage ntp = getNewTabPageForCurrentTab();

        if (ntp != null) {
            OmniboxStub omniboxStub = mLocationBar.getOmniboxStub();
            assert omniboxStub != null;
            ntp.setOmniboxStub(omniboxStub);
            mLocationBarModel.notifyNtpStartedLoading();
        }

        checkIfNtpShowingWithNoPendingLoad();

        if (mToolbarPositionController != null) {
            mIsIncognitoNtpShowingSupplier.set(isIncognitoNewTabPageCurrentlyVisible());
        }
    }

    private void setBookmarkModel(
            @Nullable BookmarkModel newBookmarkModel, @Nullable BookmarkModel oldBookmarkModel) {
        if (oldBookmarkModel != null) {
            oldBookmarkModel.removeObserver(mBookmarksObserver);
        }
        if (newBookmarkModel != null) {
            newBookmarkModel.addObserver(mBookmarksObserver);
        }
    }

    private void setLayoutStateProvider(LayoutStateProvider layoutStateProvider) {
        assert mLayoutStateProvider == null : "the mLayoutStateProvider should set at most once.";

        mLayoutStateProvider = layoutStateProvider;
        mLayoutStateProvider.addObserver(mLayoutStateObserver);

        // TODO(crbug.com/40187309): We shouldn't need to post this. Instead we should wait until
        // the dependencies are ready. This logic was introduced to move asynchronous observer
        // events from the infra (LayoutManager) into the feature using it.
        if (mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)) {
            mControlContainer.post(
                    mCallbackController.makeCancelable(
                            () -> updateForLayout(LayoutType.TAB_SWITCHER)));
        }

        mAppThemeColorProvider.setLayoutStateProvider(mLayoutStateProvider);
        BottomControlsCoordinator tabGroupUiBottomControlsCoordinator =
                mTabGroupUiBottomControlsCoordinatorSupplier.get();
        if (tabGroupUiBottomControlsCoordinator != null) {
            tabGroupUiBottomControlsCoordinator.setLayoutStateProvider(mLayoutStateProvider);
        }
        BottomControlsCoordinator bottomAppBarCoordinator = mBottomAppBarCoordinatorSupplier.get();
        if (bottomAppBarCoordinator != null) {
            bottomAppBarCoordinator.setLayoutStateProvider(mLayoutStateProvider);
        }
    }

    private void updateTabLoadingState(boolean updateUrl) {
        if (mIsDestroyed) return;

        mLocationBarModel.notifySecurityStateChanged();
        if (updateUrl) {
            mLocationBarModel.notifyUrlChanged(false);
            updateButtonStatus();
            checkIfNtpShowingWithNoPendingLoad();
        }
    }

    /**
     * @return The {@link OmniboxStub}.
     */
    public @Nullable OmniboxStub getOmniboxStub() {
        // TODO(crbug.com/40097170): Split fakebox component out of ntp package.
        return mLocationBar.getOmniboxStub();
    }

    public @Nullable VoiceRecognitionHandler getVoiceRecognitionHandler() {
        return mLocationBar.getVoiceRecognitionHandler();
    }

    /** Returns the app menu coordinator. */
    public @Nullable MenuButtonCoordinator getOverviewModeMenuButtonCoordinator() {
        return mOverviewModeMenuButtonCoordinator;
    }

    private void maybeShowUrlBarCursorIfHardwareKeyboardAvailable() {
        if (!mIsTablet) return;
        if (!UrlUtilities.isNtpUrl(mLocationBarModel.getCurrentGurl())) return;

        if (mActivity.getResources().getConfiguration().keyboard == Configuration.KEYBOARD_QWERTY) {
            mLocationBar.showUrlBarCursorWithoutFocusAnimations();
        }
    }

    /**
     * Sets the top margin for the control container.
     *
     * @param margin The margin in pixels.
     */
    private void setControlContainerTopMargin(int margin) {
        final ViewGroup.MarginLayoutParams layoutParams =
                ((ViewGroup.MarginLayoutParams) mControlContainer.getLayoutParams());
        if (layoutParams.topMargin == margin) {
            return;
        }

        layoutParams.topMargin = margin;
        mControlContainer.setLayoutParams(layoutParams);
    }

    private void onSendTabToSelfClicked() {
        GURL url = mLocationBarModel.getUrlBarData().url;
        if (url == null || url.isEmpty()) return;
        Tab tab = mActivityTabProvider.get();
        String title = tab != null ? tab.getTitle() : "";
        SendTabToSelfCoordinator sttsCoordinator =
                new SendTabToSelfCoordinator(
                        mActivity,
                        mWindowAndroid,
                        url.getSpec(),
                        title,
                        BottomSheetControllerProvider.from(mWindowAndroid),
                        mProfileSupplier.get(),
                        DeviceLockActivityLauncherSupplier.get(mWindowAndroid),
                        mActivityTabProvider,
                        mActivity,
                        SigninAndHistorySyncActivityLauncherImpl.get(),
                        mActivityResultTracker,
                        mModalDialogManagerSupplier,
                        mSnackbarManager);
        sttsCoordinator.show();
    }

    private void onBackPressStateChanged() {
        Tab tab = mActivityTabProvider.get();
        mBackPressStateSupplier.set(tab != null && mToolbarTabController.canGoBack());
    }

    private void onBackForwardTransitionAnimationChange() {
        Tab tab = mActivityTabProvider.get();
        final boolean nativeDrawsProgressBar =
                tab != null
                        && tab.getWebContents() != null
                        && tab.getWebContents().getCurrentBackForwardTransitionStage()
                                == AnimationStage.INVOKE_ANIMATION_WITH_PROGRESS_BAR;
        mToolbar.setShowingProgressBarForBackForwardTransition(nativeDrawsProgressBar);
    }

    public @BackPressResult int handleBackPress() {
        boolean ret = back();
        onBackPressStateChanged();
        return ret ? BackPressResult.SUCCESS : BackPressResult.FAILURE;
    }

    public NonNullObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressStateSupplier;
    }

    /** Returns {@link LocationBar}. */
    public LocationBar getLocationBar() {
        return mLocationBar;
    }

    /** Returns {@link WindowAndroid} */
    public WindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

    /** Returns {@link LocationBarModel} for access in tests. */
    public LocationBarModel getLocationBarModelForTesting() {
        return mLocationBarModel;
    }

    /**
     * @return The {@link ToolbarLayout} that constitutes the toolbar.
     */
    public ToolbarLayout getToolbarLayoutForTesting() {
        return mToolbarLayout;
    }

    public boolean areAnyToolbarComponentsMissingForWidth(
            @ToolbarComponentId int[] toolbarComponents) {
        if (mToolbarLayout instanceof ToolbarTablet) {
            return ((ToolbarTablet) mToolbarLayout)
                    .areAnyToolbarComponentsMissingForWidth(toolbarComponents);
        }
        return false;
    }

    public @Nullable HomeButtonCoordinator getHomeButtonCoordinatorForTesting() {
        return mHomeButtonCoordinator;
    }

    /**
     * @return View for toolbar container.
     */
    public View getContainerViewForTesting() {
        return mControlContainer.getView();
    }

    public ToolbarTabController getToolbarTabControllerForTesting() {
        return mToolbarTabController;
    }

    public BottomControlsCoordinator getTabGroupUiBottomControlsCoordinatorForTesting() {
        return mTabGroupUiBottomControlsCoordinatorSupplier.get();
    }

    public @Nullable ToggleTabStackButtonCoordinator getTabSwitcherButtonCoordinatorForTesting() {
        return mTabSwitcherButtonCoordinator;
    }

    private boolean isForward() {
        // Gestural navigation navigates backwards from both edges since this is an OS-level
        // gesture; users expect both edges to take them back.
        return false;
    }

    /**
     * Sets a Supplier which provides the current height of the bookmark bar when read.
     *
     * @param bookmarkBarHeightSupplier the Supplier to fetch the current height.
     */
    public void setBookmarkBarHeightSupplier(
            @Nullable Supplier<Integer> bookmarkBarHeightSupplier) {
        mBookmarkBarHeightSupplier = bookmarkBarHeightSupplier;
        mToolbar.setBookmarkBarHeightSupplier(mBookmarkBarHeightSupplier);
    }

    /** Requests focus onto the toolbar. */
    public void requestFocus() {
        mToolbar.requestFocus();
    }

    /**
     * @return The {@link StripLayoutHelperManager} used by this {@link ToolbarManager}, or null
     */
    public @Nullable StripLayoutHelperManager getStripLayoutHelperManager() {
        return mStripLayoutHelperManager;
    }

    /**
     * @return Whether the toolbar contains keyboard focus.
     */
    public boolean containsKeyboardFocus() {
        return mToolbar.containsKeyboardFocus();
    }

    /**
     * Reorders the focused item, if there is one.
     *
     * @param toLeft Whether the focused item should be reordered to the left (note: this is still
     *     left in RTL).
     * @return Whether the item was reordered.
     */
    public boolean reorderKeyboardFocusedItem(boolean toLeft) {
        if (mStripLayoutHelperManager == null) return false;
        return mStripLayoutHelperManager.reorderKeyboardFocusedItem(toLeft);
    }

    /**
     * Toggles multiselection on the keyboard focused tab.
     *
     * @return Whether a multiselect action was performed.
     */
    public boolean multiselectKeyboardFocusedItem() {
        if (mStripLayoutHelperManager == null) return false;
        return mStripLayoutHelperManager.multiselectKeyboardFocusedItem();
    }

    public void onXrSpaceModeChanged(Boolean fullSpaceMode) {
        boolean isFsm = Boolean.TRUE.equals(fullSpaceMode);
        mSuppressToolbarSceneLayerSupplier.set(isFsm);
        setToolbarShadowVisibility(isFsm ? View.INVISIBLE : View.VISIBLE);
        getToolbar().getProgressBar().setVisibility(isFsm ? View.INVISIBLE : View.VISIBLE);
    }

    /**
     * @return The {@link ExtensionsToolbarCoordinator} that manages the extension toolbar UI. null
     *     if extensions are not supported on this build.
     */
    public @Nullable ExtensionsToolbarCoordinator getExtensionsToolbarCoordinator() {
        return mExtensionsToolbarCoordinator;
    }

    /**
     * Returns the mAdjustedToolbarThemeColorProvider if non-null, otherwise returns the
     * mToolbarThemeColorProvider.
     */
    private ToolbarThemeColorProvider getAdjustedToolbarThemeColorProvider() {
        return mAdjustedToolbarThemeColorProvider != null
                ? mAdjustedToolbarThemeColorProvider
                : mToolbarThemeColorProvider;
    }

    /** Returns mAppThemeColorProvider for tablets or mToolbarThemeColorProvider for non-tablets. */
    private ThemeColorProvider getBrowsingModeThemeColorProvider() {
        return mIsTablet ? mAppThemeColorProvider : mToolbarThemeColorProvider;
    }

    /**
     * Returns mAppThemeColorProvider for tablets or getAdjustedToolbarThemeColorProvider() for
     * non-tablets.
     */
    private ThemeColorProvider getBrowsingModeThemeColorProviderWithAdjustableTint() {
        return mIsTablet ? mAppThemeColorProvider : getAdjustedToolbarThemeColorProvider();
    }

    private OnLongClickListener createTabSwitcherLongClickListener(
            Profile profile, Runnable openGridTabSwitcherHandler) {
        assert openGridTabSwitcherHandler != null;
        return TabSwitcherActionMenuCoordinator.createOnLongClickListener(
                menuItemId -> mAppMenuDelegate.onOptionsItemSelected(menuItemId, null),
                profile,
                mTabModelSelectorSupplier);
    }
}
