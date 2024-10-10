// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.view.View.OnClickListener;
import android.view.View.OnLayoutChangeListener;
import android.view.View.OnLongClickListener;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.FrameLayout;

import androidx.activity.BackEventCompat;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AppCompatActivity;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.JavaExceptionReporter;
import org.chromium.base.TraceEvent;
import org.chromium.base.ValueChangedCallback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.back_press.BackPressMetrics;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkModelObserver;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager.OverlayPanelManagerObserver;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.dom_distiller.DomDistillerTabUtils;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.dragdrop.toolbar.ToolbarDragDropCoordinator;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.findinpage.FindToolbarManager;
import org.chromium.chrome.browser.findinpage.FindToolbarObserver;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.gesturenav.GestureNavigationUtils;
import org.chromium.chrome.browser.gesturenav.TabOnBackGestureHandler;
import org.chromium.chrome.browser.history.HistoryManagerUtils;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.homepage.HomepagePolicyManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustSignalsCoordinator;
import org.chromium.chrome.browser.metrics.UmaActivityObserver;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.offlinepages.OfflinePageTabData;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.omnibox.BackKeyBehaviorDelegate;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.LocationBarEmbedderUiOverrides;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.OmniboxFocusReason;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.OverrideUrlLoadingDelegateImpl;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownScrollListener;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.page_info.ChromePageInfo;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.LoadUrlResult;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupUi;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupUiOneshotSupplier;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider.ThemeColorObserver;
import org.chromium.chrome.browser.theme.ThemeColorProvider.TintObserver;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsContentDelegate;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator;
import org.chromium.chrome.browser.toolbar.bottom.ScrollingBottomViewResourceFrameLayout;
import org.chromium.chrome.browser.toolbar.home_button.HomeButtonCoordinator;
import org.chromium.chrome.browser.toolbar.load_progress.LoadProgressCoordinator;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonState;
import org.chromium.chrome.browser.toolbar.top.ActionModeController;
import org.chromium.chrome.browser.toolbar.top.ActionModeController.ActionBarDelegate;
import org.chromium.chrome.browser.toolbar.top.TabSwitcherActionMenuCoordinator;
import org.chromium.chrome.browser.toolbar.top.ToggleTabStackButton;
import org.chromium.chrome.browser.toolbar.top.ToggleTabStackButtonCoordinator;
import org.chromium.chrome.browser.toolbar.top.Toolbar;
import org.chromium.chrome.browser.toolbar.top.ToolbarActionModeCallback;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer;
import org.chromium.chrome.browser.toolbar.top.ToolbarLayout;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone;
import org.chromium.chrome.browser.toolbar.top.ToolbarTablet;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator.ToolbarAlphaInOverviewObserver;
import org.chromium.chrome.browser.toolbar.top.ViewShiftingActionBarDelegate;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator.TabStripHeightObserver;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator.TabStripTransitionDelegate;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuDelegate;
import org.chromium.chrome.browser.ui.appmenu.MenuButtonDelegate;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
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
import org.chromium.net.NetError;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.BackGestureEventSwipeEdge;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowDelegate;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.util.TokenHolder;
import org.chromium.url.GURL;

import java.util.List;

/**
 * Contains logic for managing the toolbar visual component. This class manages the interactions
 * with the rest of the application to ensure the toolbar is always visually up to date.
 */
public class ToolbarManager
        implements UrlFocusChangeListener,
                ThemeColorObserver,
                TintObserver,
                MenuButtonDelegate,
                TabObscuringHandler.Observer {
    private final IncognitoStateProvider mIncognitoStateProvider;
    private final TopUiThemeColorProvider mTopUiThemeColorProvider;
    private final Supplier<EphemeralTabCoordinator> mEphemeralTabCoordinatorSupplier;
    private AppThemeColorProvider mAppThemeColorProvider;
    private SettableThemeColorProvider mCustomTabThemeColorProvider;
    private final TopToolbarCoordinator mToolbar;
    private final ToolbarControlContainer mControlContainer;
    private final View mToolbarHairline;
    private final BrowserControlsStateProvider.Observer mBrowserControlsObserver;
    private final FullscreenManager.Observer mFullscreenObserver;
    private final ObservableSupplierImpl<Boolean> mHomepageEnabledSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplier<Boolean> mOmniboxFocusStateSupplier;
    private final ConstraintsProxy mConstraintsProxy = new ConstraintsProxy();
    private ObservableSupplierImpl<BottomControlsCoordinator> mBottomControlsCoordinatorSupplier =
            new ObservableSupplierImpl<>();
    private TabModelSelector mTabModelSelector;
    private final Callback<TabModel> mCurrentTabModelObserver;
    private ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private ActivityTabProvider.ActivityTabTabObserver mActivityTabTabObserver;
    private final ActivityTabProvider mActivityTabProvider;
    private final LocationBarModel mLocationBarModel;
    private ObservableSupplier<BookmarkModel> mBookmarkModelSupplier;
    private final ValueChangedCallback<BookmarkModel> mBookmarkModelSupplierObserver =
            new ValueChangedCallback<>(this::setBookmarkModel);
    private TemplateUrlService mTemplateUrlService;
    private TemplateUrlServiceObserver mTemplateUrlObserver;
    private LocationBar mLocationBar;
    private FindToolbarManager mFindToolbarManager;

    private LayoutManagerImpl mLayoutManager;

    private BookmarkModelObserver mBookmarksObserver;
    private FindToolbarObserver mFindToolbarObserver;

    private LayoutStateProvider mLayoutStateProvider;
    private LayoutStateProvider.LayoutStateObserver mLayoutStateObserver;
    private OneshotSupplier<LayoutStateProvider> mLayoutStateProviderSupplier;
    private CallbackController mCallbackController = new CallbackController();

    private final ActionBarDelegate mActionBarDelegate;
    private ActionModeController mActionModeController;
    private final Callback<Boolean> mUrlFocusChangedCallback;
    private final Handler mHandler = new Handler();
    private final AppCompatActivity mActivity;
    private final WindowAndroid mWindowAndroid;
    private final AppMenuDelegate mAppMenuDelegate;
    private final CompositorViewHolder mCompositorViewHolder;
    private final BottomControlsStacker mBottomControlsStacker;
    private final BrowserControlsSizer mBrowserControlsSizer;
    private final FullscreenManager mFullscreenManager;
    private final ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeControllerSupplier;
    private LocationBarFocusScrimHandler mLocationBarFocusHandler;
    private ComponentCallbacks mComponentCallbacks;
    private final LoadProgressCoordinator mProgressBarCoordinator;
    private final ToolbarTabControllerImpl mToolbarTabController;
    private MenuButtonCoordinator mMenuButtonCoordinator;
    private MenuButtonCoordinator mOverviewModeMenuButtonCoordinator;
    private HomepageManager.HomepageStateListener mHomepageStateListener;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private StatusBarColorController mStatusBarColorController;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final BottomSheetController mBottomSheetController;
    private final DataSharingTabManager mDataSharingTabManager;
    private final TabContentManager mTabContentManager;
    private final TabCreatorManager mTabCreatorManager;
    private final TabObscuringHandler mTabObscuringHandler;
    private ToolbarDragDropCoordinator mToolbarDragDropCoordinator;
    private OnAttachStateChangeListener mAttachStateChangeListener;
    private final BackPressManager mBackPressManager;
    private final UserEducationHelper mUserEducationHelper;
    private final ToolbarLongPressMenuHandler mToolbarLongPressMenuHandler;
    private final OverrideUrlLoadingDelegateImpl mOverrideUrlLoadingDelegate;

    private HomeButtonCoordinator mHomeButtonCoordinator;
    private ToggleTabStackButtonCoordinator mTabSwitcherButtonCoordinator;

    private BrowserStateBrowserControlsVisibilityDelegate mControlsVisibilityDelegate;
    private int mFullscreenFocusToken = TokenHolder.INVALID_TOKEN;
    private int mFullscreenFindInPageToken = TokenHolder.INVALID_TOKEN;

    private boolean mTabRestoreCompleted;

    private boolean mInitializedWithNative;
    private Runnable mOnInitializedRunnable;
    private Runnable mMenuStateObserver;
    private UpdateMenuItemHelper mUpdateMenuItemHelper;

    private boolean mShouldUpdateToolbarPrimaryColor = true;
    private int mCurrentThemeColor;

    private int mCurrentOrientation;

    private final Supplier<Boolean> mCanAnimateNativeBrowserControls;

    private final ScrimCoordinator mScrimCoordinator;

    private OneshotSupplier<Boolean> mPromoShownOneshotSupplier;
    private OverlayPanelManagerObserver mOverlayPanelManagerObserver;
    private ObservableSupplierImpl<Boolean> mOverlayPanelVisibilitySupplier =
            new ObservableSupplierImpl<>();
    private ObservableSupplierImpl<Integer> mTabStripHeightSupplier;
    private TabStripHeightObserver mTabStripHeightObserver;
    private @Nullable DesktopWindowStateProvider mDesktopWindowStateProvider;
    private OneshotSupplierImpl<TabStripTransitionDelegate> mTabStripTransitionDelegateSupplier =
            new OneshotSupplierImpl<>();

    private @Nullable TabGroupUiOneshotSupplier mTabGroupUiOneshotSupplier;

    private final ObservableSupplierImpl<Boolean> mBackPressStateSupplier =
            new ObservableSupplierImpl<>();

    private boolean mIsDestroyed;

    private final boolean mIsCustomTab;

    private final ObservableSupplier<ReadAloudController> mReadAloudControllerSupplier;
    private final Runnable mReadAloudReadabilityCallback = this::onReadAloudReadabilityUpdated;

    private boolean mBackGestureInProgress;
    private boolean mStartNavDuringOngoingGesture;
    private WindowAndroid.ProgressBarConfig.Provider mProgressBarConfigProvider;

    private static class TabObscuringCallback implements Callback<Boolean> {
        private final TabObscuringHandler mTabObscuringHandler;

        /** A token held while the toolbar/omnibox is obscuring all visible tabs. */
        private TabObscuringHandler.Token mTabObscuringToken;

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

    /** An {@link ObservableSupplier<Integer>} for the browser constraints of the current tab. */
    private static class ConstraintsProxy extends ObservableSupplierImpl<Integer>
            implements Callback<Integer> {
        private ObservableSupplier<Integer> mCurrentConstraintDelegate;

        void onTabSwitched(Tab newTab) {
            if (!ToolbarFeatures.shouldSuppressCaptures()) {
                return;
            }

            if (mCurrentConstraintDelegate != null) {
                mCurrentConstraintDelegate.removeObserver(this);
                mCurrentConstraintDelegate = null;
            }
            if (newTab != null) {
                ObservableSupplier<Integer> newDelegate =
                        TabBrowserControlsConstraintsHelper.getObservableConstraints(newTab);
                if (newDelegate != null) {
                    Integer currentValue = newDelegate.addObserver(this);
                    mCurrentConstraintDelegate = newDelegate;

                    // While addObserver will call onResult for us, it posts a task for that. We
                    // want to be up to date right now. So manually call set.
                    set(currentValue);
                }
            }
        }

        public void destroy() {
            if (mCurrentConstraintDelegate != null) {
                mCurrentConstraintDelegate.removeObserver(this);
                mCurrentConstraintDelegate = null;
            }
        }

        @Override
        public void onResult(Integer result) {
            set(result);
        }
    }

    private class OnBackPressHandler implements BackPressHandler {
        private TabOnBackGestureHandler mHandler;
        private boolean mIsGestureMode;

        @Override
        public int handleBackPress() {
            if (mIsGestureMode) {
                BackPressMetrics.recordNavStatusDuringGesture(
                        mStartNavDuringOngoingGesture, mActivity.getWindow());
            }
            int res = BackPressResult.SUCCESS;

            if (mHandler != null) {
                mHandler.onBackInvoked();
            } else {
                assert !mBackGestureInProgress
                                || // called handleBackPress without handleBackStarted
                                !GestureNavigationUtils.allowTransition(
                                        mActivityTabProvider.get(), /* forward= */ false)
                        : "No gesture handler when transition is disallowed.";
                res = ToolbarManager.this.handleBackPress();
            }
            mBackGestureInProgress = false;
            mHandler = null;
            return res;
        }

        @Override
        public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
            return ToolbarManager.this.mBackPressStateSupplier;
        }

        @Override
        public void handleOnBackCancelled() {
            if (mIsGestureMode) {
                BackPressMetrics.recordNavStatusDuringGesture(
                        mStartNavDuringOngoingGesture, mActivity.getWindow());
            }
            mBackGestureInProgress = false;
            if (mHandler == null) return;
            mHandler.onBackCancelled();
        }

        @Override
        public void handleOnBackProgressed(@NonNull BackEventCompat backEvent) {
            if (mHandler == null) return;
            mHandler.onBackProgressed(
                    backEvent.getProgress(),
                    backEvent.getSwipeEdge() == BackEventCompat.EDGE_LEFT
                            ? BackGestureEventSwipeEdge.LEFT
                            : BackGestureEventSwipeEdge.RIGHT);
        }

        @Override
        public void handleOnBackStarted(@NonNull BackEventCompat backEvent) {
            mIsGestureMode = UiUtils.isGestureNavigationMode(mActivity.getWindow());
            // For 3-button mode, record metrics only when back is triggered by swiping.
            // See NavigationHandler.java.
            if (mIsGestureMode) {
                BackPressMetrics.recordNavStatusOnGestureStart(
                        mActivityTabProvider
                                .get()
                                .getWebContents()
                                .hasUncommittedNavigationInPrimaryMainFrame(),
                        mActivity.getWindow());
            }

            mStartNavDuringOngoingGesture = false;
            mBackGestureInProgress = true;
            if (!GestureNavigationUtils.allowTransition(mActivityTabProvider.get(), false)) return;

            mHandler = TabOnBackGestureHandler.from(mActivityTabProvider.get());

            // Gestural navigation navigates backwards from both edges since this is an OS-level
            // gesture; users expect both edges to take them back.
            boolean navigatesForward = false;
            if (TabOnBackGestureHandler.shouldAnimateNavigationTransition(
                    navigatesForward, backEvent.getSwipeEdge())) {
                // Always force to show the top control at the start of the gesture.
                TabBrowserControlsConstraintsHelper.update(
                        mLocationBarModel.getTab(),
                        BrowserControlsState.SHOWN,
                        /* animate= */ true);
            }
            mHandler.onBackStarted(
                    backEvent.getProgress(),
                    backEvent.getSwipeEdge() == BackEventCompat.EDGE_LEFT
                            ? BackGestureEventSwipeEdge.LEFT
                            : BackGestureEventSwipeEdge.RIGHT,
                    navigatesForward);
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
     * @param topUiThemeColorProvider The ThemeColorProvider object for top UI.
     * @param tabObscuringHandler Delegate object handling obscuring views.
     * @param shareDelegateSupplier Supplier for ShareDelegate.
     * @param buttonDataProviders The list of button data providers for the optional toolbar button
     *     in the browsing mode toolbar, given in precedence order.
     * @param tabProvider The {@link ActivityTabProvider} for accessing current activity tab.
     * @param scrimCoordinator A means of showing the scrim.
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
     * @param overviewColorSupplier Notifies when the overview color changes.
     * @param baseChromeLayout The base view hosting Chrome that certain views (e.g. the omnibox
     *     suggestion list) will position themselves relative to. If null, the content view will be
     *     used.
     * @param desktopWindowStateProvider The {@link DesktopWindowStateProvider} instance.
     */
    public ToolbarManager(
            AppCompatActivity activity,
            BottomControlsStacker bottomControlsStacker,
            BrowserControlsSizer controlsSizer,
            FullscreenManager fullscreenManager,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier,
            ToolbarControlContainer controlContainer,
            CompositorViewHolder compositorViewHolder,
            Callback<Boolean> urlFocusChangedCallback,
            TopUiThemeColorProvider topUiThemeColorProvider,
            TabObscuringHandler tabObscuringHandler,
            ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            List<ButtonDataProvider> buttonDataProviders,
            ActivityTabProvider tabProvider,
            ScrimCoordinator scrimCoordinator,
            ToolbarActionModeCallback toolbarActionModeCallback,
            FindToolbarManager findToolbarManager,
            ObservableSupplier<Profile> profileSupplier,
            ObservableSupplier<BookmarkModel> bookmarkModelSupplier,
            @Nullable Supplier<Boolean> canAnimateNativeBrowserControls,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            OneshotSupplier<AppMenuCoordinator> appMenuCoordinatorSupplier,
            boolean canShowUpdateBadge,
            @NonNull ObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            ObservableSupplier<Boolean> omniboxFocusStateSupplier,
            OneshotSupplier<Boolean> promoShownOneshotSupplier,
            WindowAndroid windowAndroid,
            Supplier<Boolean> isInOverviewModeSupplier,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            StatusBarColorController statusBarColorController,
            AppMenuDelegate appMenuDelegate,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull DataSharingTabManager dataSharingTabManager,
            @NonNull TabContentManager tabContentManager,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull
                    Supplier<MerchantTrustSignalsCoordinator>
                            merchantTrustSignalsCoordinatorSupplier,
            @NonNull OmniboxActionDelegate omniboxActionDelegate,
            Supplier<EphemeralTabCoordinator> ephemeralTabCoordinatorSupplier,
            boolean initializeWithIncognitoColors,
            @Nullable BackPressManager backPressManager,
            @Nullable ObservableSupplier<Integer> overviewColorSupplier,
            @Nullable View baseChromeLayout,
            ObservableSupplier<ReadAloudController> readAloudControllerSupplier,
            @Nullable DesktopWindowStateProvider desktopWindowStateProvider) {
        TraceEvent.begin("ToolbarManager.ToolbarManager");
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mCompositorViewHolder = compositorViewHolder;
        mBottomControlsStacker = bottomControlsStacker;
        mBrowserControlsSizer = controlsSizer;
        mFullscreenManager = fullscreenManager;
        mEdgeToEdgeControllerSupplier = edgeToEdgeControllerSupplier;
        mActionBarDelegate =
                new ViewShiftingActionBarDelegate(
                        activity.getSupportActionBar(),
                        controlContainer,
                        activity.findViewById(R.id.action_bar_black_background));
        mCanAnimateNativeBrowserControls = canAnimateNativeBrowserControls;
        mScrimCoordinator = scrimCoordinator;
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
        mEphemeralTabCoordinatorSupplier = ephemeralTabCoordinatorSupplier;
        mUserEducationHelper = new UserEducationHelper(mActivity, profileSupplier, mHandler);
        mDesktopWindowStateProvider = desktopWindowStateProvider;
        mOverrideUrlLoadingDelegate = new OverrideUrlLoadingDelegateImpl();

        ToolbarLayout toolbarLayout = mActivity.findViewById(R.id.toolbar);
        NewTabPageDelegate ntpDelegate = createNewTabPageDelegate(toolbarLayout);
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
                        });
        mControlContainer = controlContainer;
        mToolbarHairline = mControlContainer.findViewById(R.id.toolbar_hairline);

        mBookmarkModelSupplier = bookmarkModelSupplier;
        mBookmarkModelSupplier.addObserver(mBookmarkModelSupplierObserver);

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
                        onOrientationChange(newOrientation);
                    }

                    @Override
                    public void onLowMemory() {}
                };
        mActivity.registerComponentCallbacks(mComponentCallbacks);

        mIncognitoStateProvider = new IncognitoStateProvider();
        mTopUiThemeColorProvider = topUiThemeColorProvider;
        mTopUiThemeColorProvider.addThemeColorObserver(this);

        final boolean isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity);
        mAppThemeColorProvider =
                new AppThemeColorProvider(
                        /* context= */ mActivity,
                        ToolbarFeatures.isTabStripWindowLayoutOptimizationEnabled(isTablet)
                                ? mActivityLifecycleDispatcher
                                : null,
                        mDesktopWindowStateProvider);
        // Observe tint changes to update sub-components that rely on the tint (crbug.com/1077684).
        mAppThemeColorProvider.addTintObserver(this);
        mCustomTabThemeColorProvider = new SettableThemeColorProvider(/* context= */ mActivity);

        mActivityTabProvider = tabProvider;

        mToolbarTabController =
                new ToolbarTabControllerImpl(
                        mLocationBarModel::getTab,
                        () -> {
                            Profile profile = profileSupplier.get();
                            return profile != null
                                    ? TrackerFactory.getTrackerForProfile(profile)
                                    : null;
                        },
                        mBottomControlsCoordinatorSupplier,
                        ToolbarManager::homepageUrl,
                        this::updateButtonStatus,
                        mActivityTabProvider);
        if (backPressManager != null && BackPressManager.isEnabled()) {
            OnBackPressHandler handler = new OnBackPressHandler();
            backPressManager.addHandler(handler, BackPressHandler.Type.TAB_HISTORY);
            mBackPressManager = backPressManager;
        } else {
            mBackPressManager = null;
        }

        BrowserStateBrowserControlsVisibilityDelegate controlsVisibilityDelegate =
                mBrowserControlsSizer.getBrowserVisibilityDelegate();
        assert controlsVisibilityDelegate != null;
        mControlsVisibilityDelegate = controlsVisibilityDelegate;
        ThemeColorProvider browsingModeThemeColorProvider =
                isTablet ? mAppThemeColorProvider : mTopUiThemeColorProvider;
        ThemeColorProvider overviewModeThemeColorProvider = mAppThemeColorProvider;

        Runnable requestFocusRunnable = compositorViewHolder::requestFocus;
        mIsCustomTab = toolbarLayout instanceof CustomTabToolbar;
        ThemeColorProvider menuButtonThemeColorProvider =
                mIsCustomTab ? mCustomTabThemeColorProvider : browsingModeThemeColorProvider;

        Supplier<MenuButtonState> menuButtonStateSupplier =
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
                        appMenuCoordinatorSupplier,
                        mControlsVisibilityDelegate,
                        mWindowAndroid,
                        this::setUrlBarFocus,
                        requestFocusRunnable,
                        canShowUpdateBadge,
                        isInOverviewModeSupplier,
                        menuButtonThemeColorProvider,
                        menuButtonStateSupplier,
                        onMenuButtonClicked,
                        R.id.menu_button_wrapper);
        if (canShowUpdateBadge) mMenuStateObserver = mMenuButtonCoordinator.getStateObserver();

        // TODO(b/351005760): Investigate the feasibility of replacing
        // mOverviewModeMenuButtonCoordinator with mMenuButtonCoordinator when Hub is enabled.
        mOverviewModeMenuButtonCoordinator =
                new MenuButtonCoordinator(
                        appMenuCoordinatorSupplier,
                        mControlsVisibilityDelegate,
                        mWindowAndroid,
                        this::setUrlBarFocus,
                        requestFocusRunnable,
                        canShowUpdateBadge,
                        isInOverviewModeSupplier,
                        overviewModeThemeColorProvider,
                        menuButtonStateSupplier,
                        onMenuButtonClicked,
                        R.id.none);

        ToggleTabStackButton tabSwitcherButton =
                mControlContainer.findViewById(R.id.tab_switcher_button);
        if (tabSwitcherButton != null) {
            mTabSwitcherButtonCoordinator =
                    new ToggleTabStackButtonCoordinator(
                            mActivity,
                            tabSwitcherButton,
                            mUserEducationHelper,
                            mIncognitoStateProvider::isIncognitoSelected,
                            mPromoShownOneshotSupplier,
                            mLayoutStateProviderSupplier,
                            mActivityTabProvider,
                            mTabModelSelectorSupplier);
        }

        mToolbarLongPressMenuHandler =
                new ToolbarLongPressMenuHandler(/* context= */ mActivity, mIsCustomTab);
        OnLongClickListener onLongClickListener =
                mToolbarLongPressMenuHandler.getOnLongClickListener();

        mToolbar =
                createTopToolbarCoordinator(
                        controlContainer,
                        toolbarLayout,
                        buttonDataProviders,
                        browsingModeThemeColorProvider,
                        initializeWithIncognitoColors,
                        mConstraintsProxy,
                        onLongClickListener);
        mTabStripHeightSupplier = new ObservableSupplierImpl<>(mToolbar.getTabStripHeight());
        mActionModeController =
                new ActionModeController(
                        mActivity,
                        mActionBarDelegate,
                        toolbarActionModeCallback,
                        mTabStripHeightSupplier);

        tabObscuringHandler.addObserver(this);

        if (mIsCustomTab) {
            CustomTabToolbar customTabToolbar = ((CustomTabToolbar) toolbarLayout);
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
                            merchantTrustSignalsCoordinatorSupplier::get,
                            mEphemeralTabCoordinatorSupplier,
                            mTabCreatorManager.getTabCreator(
                                    mIncognitoStateProvider.isIncognitoSelected()));
            OmniboxSuggestionsDropdownScrollListener scrollListener =
                    toolbarLayout instanceof OmniboxSuggestionsDropdownScrollListener
                            ? (OmniboxSuggestionsDropdownScrollListener) toolbarLayout
                            : null;

            Supplier<Integer> bottomWindowPaddingSupplier =
                    () ->
                            mEdgeToEdgeControllerSupplier.get() != null
                                    ? mEdgeToEdgeControllerSupplier.get().getBottomInsetPx()
                                    : 0;

            LocationBarCoordinator locationBarCoordinator =
                    new LocationBarCoordinator(
                            mActivity.findViewById(R.id.location_bar),
                            toolbarLayout,
                            profileSupplier,
                            mLocationBarModel,
                            mActionModeController.getActionModeCallback(),
                            new WindowDelegate(mActivity.getWindow()),
                            windowAndroid,
                            mActivityTabProvider,
                            modalDialogManagerSupplier,
                            shareDelegateSupplier,
                            mIncognitoStateProvider,
                            activityLifecycleDispatcher,
                            mOverrideUrlLoadingDelegate,
                            new BackKeyBehaviorDelegate() {},
                            toolbarPageInfo::show,
                            IntentHandler::bringTabToFront,
                            DownloadUtils::isAllowedToDownloadPage,
                            NewTabPageUma::recordOmniboxNavigation,
                            TabWindowManagerSingleton::getInstance,
                            (url) ->
                                    mBookmarkModelSupplier.hasValue()
                                            && mBookmarkModelSupplier.get().isBookmarked(url),
                            () -> {
                                return mToolbar.getCurrentOptionalButtonVariant()
                                        == AdaptiveToolbarButtonVariant.VOICE;
                            },
                            merchantTrustSignalsCoordinatorSupplier,
                            omniboxActionDelegate,
                            mControlsVisibilityDelegate,
                            BackPressManager.isEnabled() ? backPressManager : null,
                            scrollListener,
                            tabModelSelectorSupplier,
                            new LocationBarEmbedderUiOverrides(),
                            baseChromeLayout,
                            bottomWindowPaddingSupplier,
                            onLongClickListener);
            toolbarLayout.setLocationBarCoordinator(locationBarCoordinator);
            toolbarLayout.setBrowserControlsVisibilityDelegate(mControlsVisibilityDelegate);
            mLocationBar = locationBarCoordinator;
            if (isTablet && ChromeFeatureList.sDragDropIntoOmnibox.isEnabled()) {
                ViewStub targetViewStub = mActivity.findViewById(R.id.target_view_stub);
                assert targetViewStub != null;
                mToolbarDragDropCoordinator =
                        new ToolbarDragDropCoordinator(
                                (FrameLayout) targetViewStub.inflate(),
                                locationBarCoordinator,
                                locationBarCoordinator.getOmniboxStub(),
                                () -> mTemplateUrlService);
                mControlContainer.setOnDragListener(mToolbarDragDropCoordinator);
            }
        }

        Runnable clickDelegate = () -> setUrlBarFocus(false, OmniboxFocusReason.UNFOCUS);
        View scrimTarget = mCompositorViewHolder;
        mLocationBarFocusHandler =
                new LocationBarFocusScrimHandler(
                        scrimCoordinator,
                        new TabObscuringCallback(tabObscuringHandler),
                        /* context= */ activity,
                        mLocationBarModel,
                        clickDelegate,
                        scrimTarget,
                        mTabStripHeightSupplier);

        var omnibox = mLocationBar.getOmniboxStub();
        if (omnibox != null) {
            omnibox.addUrlFocusChangeListener(this);
            omnibox.addUrlFocusChangeListener(mStatusBarColorController);
            omnibox.addUrlFocusChangeListener(mLocationBarFocusHandler);
        }
        mLocationBar.addOmniboxSuggestionsDropdownScrollListener(mStatusBarColorController);

        mProgressBarCoordinator =
                new LoadProgressCoordinator(mActivityTabProvider, mToolbar.getProgressBar());
        mToolbar.addUrlExpansionObserver(statusBarColorController);
        mToolbar.setToolbarColorObserver(statusBarColorController);

        mActivityTabTabObserver =
                new ActivityTabProvider.ActivityTabTabObserver(mActivityTabProvider) {
                    @Override
                    public void onObservingDifferentTab(Tab tab, boolean hint) {
                        // ActivityTabProvider will null out the tab passed to
                        // onObservingDifferentTab when the tab is non-interactive (e.g. when
                        // entering the TabSwitcher).
                        // In those cases we actually still want to use the most recently selected
                        // tab, but will update the URL.
                        onBackPressStateChanged();
                        onBackForwardTransitionAnimationChange();
                        mBackGestureInProgress = false;
                        if (tab == null) {
                            mLocationBarModel.notifyUrlChanged();
                            return;
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
                        mLocationBarModel.notifyUrlChanged();
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
                        mLocationBarModel.notifyContentChanged();
                        checkIfNtpLoaded();
                        mToolbar.onTabContentViewChanged();
                        maybeShowOrClearCursorInLocationBar();
                        // Paint preview status might have been changed. Update the omnibox chip.
                        mLocationBarModel.notifySecurityStateChanged();
                        onBackPressStateChanged();
                    }

                    @Override
                    public void onWebContentsSwapped(
                            Tab tab, boolean didStartLoad, boolean didFinishLoad) {
                        onBackPressStateChanged();
                        if (!didStartLoad) return;
                        mLocationBarModel.notifyWebContentsSwapped();
                        mLocationBarModel.notifyUrlChanged();
                        mLocationBarModel.notifySecurityStateChanged();
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
                            mToolbar.onNavigatedToDifferentPage();
                            maybeTriggerCacheRefreshForZeroSuggest(navigation.getUrl());
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
                        onBackPressStateChanged();
                        mLocationBarModel.notifyDidStartNavigation(
                                navigationHandle.isSameDocument());
                    }

                    @Override
                    public void onNavigationEntriesDeleted(Tab tab) {
                        if (tab == mLocationBarModel.getTab()) {
                            updateButtonStatus();
                        }
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
                    public void didBackForwardTransitionAnimationChange() {
                        onBackForwardTransitionAnimationChange();
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
                    private OnLayoutChangeListener mLayoutChangeListener;

                    @Override
                    public void onControlsOffsetChanged(
                            int topOffset,
                            int topControlsMinHeightOffset,
                            int bottomOffset,
                            int bottomControlsMinHeightOffset,
                            boolean needsAnimate,
                            boolean isVisibilityForced) {
                        // Controls need to be offset to match the composited layer, which is
                        // anchored below the minimum height. In other words, the top of the toolbar
                        // composited layer is anchored at the bottom of the minimum height.
                        // https://crbug.com/1157859 wait until the background is cleared so that
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
                        updateForLayout(layoutType);
                    }

                    @Override
                    public void onFinishedShowing(int layoutType) {
                        if (layoutType == LayoutType.TAB_SWITCHER) {
                            mToolbar.onTabSwitcherTransitionFinished();
                        }
                        mToolbar.onTransitionEnd();
                        if (layoutType == LayoutType.BROWSING) {
                            maybeShowUrlBarCursorIfHardwareKeyboardAvailable();
                        }
                    }

                    @Override
                    public void onStartedHiding(@LayoutType int layoutType) {
                        if (layoutType == LayoutType.TAB_SWITCHER) {
                            mLocationBarModel.updateForNonStaticLayout();
                            mToolbar.setTabSwitcherMode(false);
                            updateButtonStatus();
                            if (mToolbar.setForceTextureCapture(true)) {
                                mControlContainer.invalidateBitmap();
                            }
                        }
                        mToolbar.onTransitionStart();
                    }

                    @Override
                    public void onFinishedHiding(@LayoutType int layoutType) {
                        if (layoutType == LayoutType.TAB_SWITCHER) {
                            mToolbar.onTabSwitcherTransitionFinished();
                            updateButtonStatus();

                            if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
                                checkIfNtpLoaded();
                                maybeShowOrClearCursorInLocationBar();
                            }
                        }
                    }
                };

        mOverlayPanelManagerObserver =
                new OverlayPanelManagerObserver() {
                    @Override
                    public void onOverlayPanelShown() {
                        mOverlayPanelVisibilitySupplier.set(true);
                    }

                    @Override
                    public void onOverlayPanelHidden() {
                        mOverlayPanelVisibilitySupplier.set(false);
                    }
                };

        mToolbar.setIncognitoStateProvider(mIncognitoStateProvider, overviewColorSupplier);

        mFindToolbarManager = findToolbarManager;
        mFindToolbarManager.addObserver(mFindToolbarObserver);

        Callback<Profile> profileObserver =
                new Callback<Profile>() {
                    @Override
                    public void onResult(Profile profile) {
                        mTemplateUrlService = TemplateUrlServiceFactory.getForProfile(profile);
                        mTemplateUrlService.runWhenLoaded(
                                ToolbarManager.this::registerTemplateUrlObserver);
                        profileSupplier.removeObserver(this);
                    }
                };
        profileSupplier.addObserver(profileObserver);
        mReadAloudControllerSupplier = readAloudControllerSupplier;
        mReadAloudControllerSupplier.addObserver(
                readAloudController -> {
                    if (readAloudController != null) {
                        readAloudController.addReadabilityUpdateListener(
                                mReadAloudReadabilityCallback);
                    }
                });

        if (mDesktopWindowStateProvider != null) {
            mDesktopWindowStateProvider.addObserver(mControlContainer);
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

        TraceEvent.end("ToolbarManager.ToolbarManager");
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
            mToolbar.setTabSwitcherMode(layoutType == LayoutType.TAB_SWITCHER);
            updateButtonStatus();
        }
        mToolbar.setContentAttached(layoutType == LayoutType.BROWSING);
    }

    private TopToolbarCoordinator createTopToolbarCoordinator(
            ToolbarControlContainer controlContainer,
            ToolbarLayout toolbarLayout,
            List<ButtonDataProvider> buttonDataProviders,
            ThemeColorProvider browsingModeThemeColorProvider,
            boolean initializeWithIncognitoColors,
            ObservableSupplier<Integer> constraintsSupplier,
            OnLongClickListener onLongClickListener) {
        TopToolbarCoordinator toolbar =
                new TopToolbarCoordinator(
                        controlContainer,
                        toolbarLayout,
                        mLocationBarModel,
                        mToolbarTabController,
                        mUserEducationHelper,
                        buttonDataProviders,
                        mLayoutStateProviderSupplier,
                        browsingModeThemeColorProvider,
                        mMenuButtonCoordinator,
                        mMenuButtonCoordinator.getMenuButtonHelperSupplier(),
                        mTabSwitcherButtonCoordinator,
                        mTabModelSelectorSupplier,
                        mHomepageEnabledSupplier,
                        mCompositorViewHolder::getResourceManager,
                        HistoryManagerUtils::showHistoryManager,
                        PartnerBrowserCustomizations.getInstance()
                                ::isHomepageProviderAvailableAndEnabled,
                        DownloadUtils::downloadOfflinePage,
                        initializeWithIncognitoColors,
                        constraintsSupplier,
                        mCompositorViewHolder.getInMotionSupplier(),
                        mControlsVisibilityDelegate,
                        mFullscreenManager,
                        mTabObscuringHandler,
                        mDesktopWindowStateProvider,
                        mTabStripTransitionDelegateSupplier,
                        onLongClickListener);

        mHomepageStateListener =
                () -> {
                    mHomepageEnabledSupplier.set(HomepageManager.getInstance().isHomepageEnabled());
                };

        HomepageManager.getInstance().addListener(mHomepageStateListener);
        mHomepageStateListener.onHomepageStateUpdated();

        View homeButton = controlContainer.findViewById(R.id.home_button);
        if (homeButton != null) {
            mHomeButtonCoordinator =
                    new HomeButtonCoordinator(
                            mActivity,
                            homeButton,
                            this::onHomeButtonMenuClick,
                            HomepagePolicyManager::isHomepageManagedByPolicy);
        }

        return toolbar;
    }

    /**
     * Menu click handler on home button and records if user long presses on home button to
     * edit homepage on the new tab page.
     * @param context {@link Context} used for launching a settings activity.
     */
    private void onHomeButtonMenuClick(Context context) {
        boolean isNtp = getNewTabPageForCurrentTab() != null;
        HomepageManager.getInstance().onMenuClick(context);
        if (isNtp) {
            BrowserUiUtils.recordModuleLongClickHistogram(ModuleTypeOnStartAndNtp.HOME_BUTTON);
        }
    }

    // Base abstract implementation of NewTabPageDelegate for phone/table toolbar layout.
    private abstract class ToolbarNtpDelegate implements NewTabPageDelegate {
        protected NewTabPage mVisibleNtp;

        @Override
        public boolean wasShowingNtp() {
            return mVisibleNtp != null;
        }

        @Override
        public boolean isCurrentlyVisible() {
            return getNewTabPageForCurrentTab() != null;
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
            if (mVisibleNtp != null) mVisibleNtp.setSearchBoxScrollListener(null);
            mVisibleNtp = newVisibleNtp;
            if (mVisibleNtp != null && shouldUpdateListener()) {
                mVisibleNtp.setSearchBoxScrollListener(
                        (fraction) -> scrollCallback.onResult(fraction));
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
        public void setSearchBoxBackground(Drawable drawable) {
            assert getNewTabPageForCurrentTab() != null;
            getNewTabPageForCurrentTab().setSearchBoxBackground(drawable);
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

    private NewTabPageDelegate createNewTabPageDelegate(ToolbarLayout toolbarLayout) {
        if (toolbarLayout instanceof ToolbarPhone) {
            return new ToolbarNtpDelegate() {
                @Override
                protected boolean shouldUpdateListener() {
                    return mVisibleNtp.isLocationBarShownInNtp();
                }
            };
        } else if (toolbarLayout instanceof ToolbarTablet) {
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

    private NewTabPage getNewTabPageForCurrentTab() {
        if (mLocationBarModel.hasTab()) {
            NativePage nativePage = mLocationBarModel.getTab().getNativePage();
            if (nativePage instanceof NewTabPage) return (NewTabPage) nativePage;
        }
        return null;
    }

    /**
     * @return Whether the UrlBar currently has focus.
     */
    public boolean isUrlBarFocused() {
        if (mLocationBar.getOmniboxStub() == null) {
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

    /** Enable the bottom controls. */
    public void enableBottomControls() {
        View root = ((ViewStub) mActivity.findViewById(R.id.bottom_controls_stub)).inflate();
        assert mTabGroupUiOneshotSupplier == null;
        mTabGroupUiOneshotSupplier =
                new TabGroupUiOneshotSupplier(
                        mActivityTabProvider,
                        mTabModelSelector,
                        mActivity,
                        root.findViewById(R.id.bottom_container_slot),
                        mBrowserControlsSizer,
                        mIncognitoStateProvider,
                        mScrimCoordinator,
                        mOmniboxFocusStateSupplier,
                        mBottomSheetController,
                        mDataSharingTabManager,
                        mTabContentManager,
                        mTabCreatorManager,
                        mLayoutStateProviderSupplier,
                        mModalDialogManagerSupplier.get());
        var bottomControlsContentDelegateSupplier =
                (OneshotSupplier<BottomControlsContentDelegate>)
                        ((OneshotSupplier<? extends BottomControlsContentDelegate>)
                                mTabGroupUiOneshotSupplier);
        var bottomControlsCoordinator =
                new BottomControlsCoordinator(
                        mActivity,
                        mWindowAndroid,
                        mLayoutManager,
                        mCompositorViewHolder.getResourceManager(),
                        mBottomControlsStacker,
                        mControlsVisibilityDelegate,
                        mFullscreenManager,
                        mEdgeToEdgeControllerSupplier,
                        (ScrollingBottomViewResourceFrameLayout) root,
                        bottomControlsContentDelegateSupplier,
                        mTabObscuringHandler,
                        mOverlayPanelVisibilitySupplier,
                        mConstraintsProxy,
                        /* readAloudRestoringSupplier= */ () -> {
                            final var readAloud = mReadAloudControllerSupplier.get();
                            return readAloud != null && readAloud.isRestoringPlayer();
                        });
        mBottomControlsCoordinatorSupplier.set(bottomControlsCoordinator);
        if (mBackPressManager != null) {
            mBackPressManager.addHandler(
                    bottomControlsCoordinator, BackPressHandler.Type.BOTTOM_CONTROLS);
        }
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
     */
    public void initializeWithNative(
            @NonNull LayoutManagerImpl layoutManager,
            @Nullable StripLayoutHelperManager stripLayoutHelperManager,
            Runnable openGridTabSwitcherHandler,
            OnClickListener bookmarkClickHandler,
            OnClickListener customTabsBackClickHandler,
            @Nullable ObservableSupplier<Integer> archivedTabCountSupplier) {
        TraceEvent.begin("ToolbarManager.initializeWithNative");
        assert !mInitializedWithNative;
        assert mTabModelSelectorSupplier.get() != null;

        mTabModelSelector = mTabModelSelectorSupplier.get();
        Profile profile = mTabModelSelector.getModel(false).getProfile();
        assert profile != null;

        mOverrideUrlLoadingDelegate.setOpenGridTabSwitcherCallback(openGridTabSwitcherHandler);

        // Must be initialized before Toolbar attempts to use it.
        mLocationBarModel.initializeWithNative();
        if (mTabSwitcherButtonCoordinator != null) {
            OnLongClickListener tabSwitcherLongClickListener =
                    TabSwitcherActionMenuCoordinator.createOnLongClickListener(
                            menuItemId -> mAppMenuDelegate.onOptionsItemSelected(menuItemId, null),
                            profile,
                            mTabModelSelectorSupplier);
            mTabSwitcherButtonCoordinator.initializeWithNative(
                    v -> openGridTabSwitcherHandler.run(),
                    tabSwitcherLongClickListener,
                    mTabModelSelectorSupplier.get().getCurrentModelTabCountSupplier(),
                    archivedTabCountSupplier,
                    () -> TabArchiveSettings.setIphShownThisSession(true),
                    () -> TabArchiveSettings.setIphShownThisSession(false));
        }

        mToolbar.initializeWithNative(
                profile,
                layoutManager::requestUpdate,
                bookmarkClickHandler,
                customTabsBackClickHandler,
                layoutManager,
                mActivityTabProvider,
                mBrowserControlsSizer,
                mTopUiThemeColorProvider);
        mTabStripHeightSupplier.set(mToolbar.getTabStripHeight());

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
        mLayoutManager.getOverlayPanelManager().addObserver(mOverlayPanelManagerObserver);

        if (stripLayoutHelperManager != null) {
            mControlContainer.setToolbarContainerDragListener(
                    stripLayoutHelperManager.getDragListener());

            mTabStripTransitionDelegateSupplier.set(stripLayoutHelperManager);
            stripLayoutHelperManager.setIsTabStripHidden(mToolbar.getTabStripHeight() == 0);
        }

        mTabStripHeightObserver =
                new TabStripHeightObserver() {
                    @Override
                    public void onTransitionRequested(int newHeight) {
                        // TODO(crbug.com/41481630): Supplier can have an inconsistent value
                        //  with mToolbar.getTabStripHeight().
                        mTabStripHeightSupplier.set(newHeight);
                    }
                };
        mToolbar.addTabStripHeightObserver(mTabStripHeightObserver);

        mUpdateMenuItemHelper = UpdateMenuItemHelper.getInstance(profile);
        if (mMenuStateObserver != null) {
            mUpdateMenuItemHelper.registerObserver(mMenuStateObserver);
        }

        mInitializedWithNative = true;
        TabModelUtils.runOnTabStateInitialized(
                mTabModelSelector,
                mCallbackController.makeCancelable(
                        (unusedTabModelSelector) -> {
                            mTabRestoreCompleted = true;
                            handleTabRestoreCompleted();
                        }));
        mTabModelSelector.getCurrentTabModelSupplier().addObserver(mCurrentTabModelObserver);
        refreshSelectedTab(mActivityTabProvider.get());
        maybeShowUrlBarCursorIfHardwareKeyboardAvailable();
        if (mTabModelSelector.isTabStateInitialized()) mTabRestoreCompleted = true;
        handleTabRestoreCompleted();
        mIncognitoStateProvider.setTabModelSelector(mTabModelSelector);
        mAppThemeColorProvider.setIncognitoStateProvider(mIncognitoStateProvider);

        if (mOnInitializedRunnable != null) {
            mOnInitializedRunnable.run();
            mOnInitializedRunnable = null;
        }

        // Allow bitmap capturing once everything has been initialized.
        Tab currentTab = mTabModelSelector.getCurrentTab();
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
        return mMenuButtonCoordinator.getMenuButton().getImageButton();
    }

    /**
     * TODO(twellington): Try to remove this method. It's only used to return an in-product help
     *                    bubble anchor view... which should be moved out of tab and perhaps into
     *                    the status bar icon component.
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
     * @see #updateCustomActionButton
     */
    public void addCustomActionButton(
            Drawable drawable, String description, OnClickListener listener) {
        mToolbar.addCustomActionButton(drawable, description, listener);
    }

    /**
     * Updates the visual appearance of a custom action button in the {@link Toolbar},
     * if it is supported.
     * @param index The index of the button to update.
     * @param drawable The {@link Drawable} to use as the background for the button.
     * @param description The content description for the custom action button.
     * @see #addCustomActionButton
     */
    public void updateCustomActionButton(int index, Drawable drawable, String description) {
        mToolbar.updateCustomActionButton(index, drawable, description);
    }

    /** Call to tear down all of the toolbar dependencies. */
    public void destroy() {
        mIsDestroyed = true;

        var omnibox = mLocationBar.getOmniboxStub();
        if (omnibox != null) {
            omnibox.removeUrlFocusChangeListener(this);
            omnibox.removeUrlFocusChangeListener(mStatusBarColorController);
            omnibox.removeUrlFocusChangeListener(mLocationBarFocusHandler);
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
            mLayoutManager.getOverlayPanelManager().removeObserver(mOverlayPanelManagerObserver);
            mLayoutManager = null;
        }

        HomepageManager.getInstance().removeListener(mHomepageStateListener);

        if (mBottomControlsCoordinatorSupplier.get() != null) {
            mBottomControlsCoordinatorSupplier.get().destroy();
            mBottomControlsCoordinatorSupplier = null;
        }

        if (mLocationBar != null) {
            mLocationBar.destroy();
            mLocationBar = null;
        }

        if (mAttachStateChangeListener != null) {
            mToolbar.removeOnAttachStateChangeListener(mAttachStateChangeListener);
            mAttachStateChangeListener = null;
        }
        if (mTabStripHeightObserver != null) {
            mToolbar.removeTabStripHeightObserver(mTabStripHeightObserver);
            mTabStripHeightObserver = null;
        }
        mTabStripHeightSupplier = null;
        mToolbar.removeUrlExpansionObserver(mStatusBarColorController);
        mToolbar.destroy();

        mIncognitoStateProvider.destroy();

        mLocationBarModel.destroy();
        mHandler.removeCallbacksAndMessages(null); // Cancel delayed tasks.
        mBrowserControlsSizer.removeObserver(mBrowserControlsObserver);
        mFullscreenManager.removeObserver(mFullscreenObserver);

        if (mTopUiThemeColorProvider != null) {
            mTopUiThemeColorProvider.removeThemeColorObserver(this);
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

        if (mOverviewModeMenuButtonCoordinator != null) {
            mOverviewModeMenuButtonCoordinator.destroy();
            mOverviewModeMenuButtonCoordinator = null;
        }

        mUpdateMenuItemHelper = null;

        if (mTabSwitcherButtonCoordinator != null) {
            mTabSwitcherButtonCoordinator.destroy();
            mTabSwitcherButtonCoordinator = null;
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

        if (mDesktopWindowStateProvider != null) {
            mDesktopWindowStateProvider.removeObserver(mControlContainer);
        }

        mTabObscuringHandler.removeObserver(this);

        mActivity.unregisterComponentCallbacks(mComponentCallbacks);
        mComponentCallbacks = null;

        mControlContainer.destroy();
        mConstraintsProxy.destroy();
        mLocationBarFocusHandler.destroy();

        mWindowAndroid.setProgressBarConfigProvider(null);
    }

    /** Called when the orientation of the activity has changed. */
    private void onOrientationChange(int newOrientation) {
        if (mActionModeController != null) mActionModeController.showControlsOnOrientationChange();
    }

    @VisibleForTesting
    static String homepageUrl() {
        GURL homepageGurl = HomepageManager.getInstance().getHomepageGurl();
        if (homepageGurl.isEmpty()) {
            return UrlConstants.NTP_URL;
        } else {
            return homepageGurl.getSpec();
        }
    }

    private void registerTemplateUrlObserver() {
        assert mTemplateUrlObserver == null;
        mTemplateUrlObserver =
                new TemplateUrlServiceObserver() {
                    private TemplateUrl mSearchEngine =
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
                    }
                };
        mTemplateUrlService.addObserver(mTemplateUrlObserver);
    }

    private void handleTabRestoreCompleted() {
        if (!mTabRestoreCompleted || !mInitializedWithNative) return;
        // Enable tab switcher button.
        if (mTabSwitcherButtonCoordinator != null) {
            mTabSwitcherButtonCoordinator.getContainerView().setClickable(true);
        }
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

    /** Get the supplier for the current height of the tab strip. Always returns a valid integer. */
    public ObservableSupplier<Integer> getTabStripHeightSupplier() {
        return mTabStripHeightSupplier;
    }

    /** Return the TabStripTransitionCoordinator. */
    public TabStripTransitionCoordinator getTabStripTransitionCoordinator() {
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
        // TODO(https://crbug.com/865801, pnoland): Rationalize theme color logic
        // into a set of documented, self-contained providers that we can inject to the appropriate
        // sub-components. That will let us have every component handle its own coloring, and remove
        // onThemeColorChanged from ToolbarManager.
        mCustomTabThemeColorProvider.setPrimaryColor(color, shouldAnimate);
    }

    @Override
    public void onTintChanged(
            ColorStateList tint,
            ColorStateList activityFocusTint,
            @BrandedColorScheme int brandedColorScheme) {
        updateBookmarkButtonStatus();

        if (mShouldUpdateToolbarPrimaryColor) {
            mCustomTabThemeColorProvider.setTint(tint, brandedColorScheme);
        }
    }

    /**
     * @param shouldUpdate Whether we should be updating the toolbar primary color based on updates
     *                     from the Tab.
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
        int toolbarHairlineHeight = mToolbarHairline.getHeight();
        final int controlContainerHeight = mControlContainer.getHeight();

        // Offset can't be calculated if control container height isn't known yet.
        if (controlContainerHeight == 0) {
            return 0;
        }

        int extraYOffset =
                mBrowserControlsSizer.getTopControlsHeight()
                        - (controlContainerHeight - toolbarHairlineHeight);
        // There are cases where extraYOffset can be negative e.g. during tab strip transitioning
        // from invisible -> visible.
        return Math.max(0, extraYOffset);
    }

    /**
     * Sets the drawable that the close button shows, or hides it if {@code drawable} is
     * {@code null}.
     */
    public void setCloseButtonDrawable(Drawable drawable) {
        mToolbar.setCloseButtonImageResource(drawable);
    }

    /**
     * Sets whether a title should be shown within the Toolbar.
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
     * If you request focus and the UrlBar was already focused, this will select all of the text.
     *
     * @param focused Whether URL bar should be focused.
     * @param reason The given reason.
     */
    public void setUrlBarFocus(boolean focused, @OmniboxFocusReason int reason) {
        setUrlBarFocusAndText(focused, reason, null);
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
            boolean focused, @OmniboxFocusReason int reason, String text) {
        if (!mInitializedWithNative) return;
        if (mLocationBar.getOmniboxStub() == null) return;
        boolean wasFocused = mLocationBar.getOmniboxStub().isUrlBarFocused();
        mLocationBar.getOmniboxStub().setUrlBarFocus(focused, text, reason);
        if (wasFocused && focused) {
            mLocationBar.selectAll();
        }
    }

    /**
     * Unfocus the url bar when back press is performed. Do nothing if it is unfocused.
     * @return Whether url bar is focused when this method is called.
     */
    public boolean unfocusUrlBarOnBackPress() {
        return mLocationBar.unfocusUrlBarOnBackPressed();
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
     * Reverts any pending edits of the location bar and reset to the page state.  This does not
     * change the focus state of the location bar.
     */
    public void revertLocationBarChanges() {
        mLocationBar.revertChanges();
    }

    /**
     * Handle all necessary tasks that can be delayed until initialization completes.
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

        mToolbar.updateButtonVisibility();
        mToolbar.updateBackButtonVisibility(currentTab != null && currentTab.canGoBack());
        onBackPressStateChanged();
        mToolbar.updateForwardButtonVisibility(currentTab != null && currentTab.canGoForward());
        updateReloadState(tabCrashed);
        updateBookmarkButtonStatus();
        if (mToolbar.getMenuButtonWrapper() != null) {
            mToolbar.getMenuButtonWrapper().setVisibility(View.VISIBLE);
        }
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
        mToolbar.updateReloadButtonVisibility(isLoading);
        mMenuButtonCoordinator.updateReloadingState(isLoading);
    }

    /** Triggered when the selected tab has changed. */
    private void refreshSelectedTab(Tab tab) {
        boolean wasIncognitoBranded = mLocationBarModel.isIncognitoBranded();
        Tab previousTab = mLocationBarModel.getTab();

        Profile profile =
                tab != null ? tab.getProfile() : mTabModelSelector.getCurrentModel().getProfile();
        assert profile != null
                : "Failed to get Profile when offTheRecord = "
                        + mTabModelSelector.isOffTheRecordModelSelected();

        mLocationBarModel.setTab(tab, profile);
        updateTabLoadingState(true);

        boolean isIncognitoBranded = profile.isIncognitoBranded();
        // This method is called prior to action mode destroy callback for incognito <-> normal
        // tab switch. Makes sure the action mode toolbar is hidden before selecting the new tab.
        if (previousTab != null
                && wasIncognitoBranded != isIncognitoBranded
                && DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            mActionModeController.startHideAnimation();
        }
        if (previousTab != tab || wasIncognitoBranded != isIncognitoBranded) {
            int defaultPrimaryColor =
                    ChromeColors.getDefaultThemeColor(mActivity, isIncognitoBranded);
            int primaryColor =
                    tab != null
                            ? mTopUiThemeColorProvider.calculateColor(tab, tab.getThemeColor())
                            : defaultPrimaryColor;
            // TODO(jinsukkim): Let TopUiThemeColorProvider handle this by updating the theme color.
            onThemeColorChanged(primaryColor, false);

            onTabOrModelChanged();

            if (tab != null) {
                mToolbar.onNavigatedToDifferentPage();
            }

            // Ensure the URL bar loses focus if the tab it was interacting with is changed from
            // underneath it.
            setUrlBarFocus(false, OmniboxFocusReason.UNFOCUS);

            // Place the cursor in the Omnibox if applicable.  We always clear the focus above to
            // ensure the shield placed over the content is dismissed when switching tabs.  But if
            // needed, we will refocus the omnibox and make the cursor visible here.
            maybeShowOrClearCursorInLocationBar();
        }

        updateButtonStatus();
        mConstraintsProxy.onTabSwitched(tab);
    }

    private void onTabOrModelChanged() {
        mToolbar.onTabOrModelChanged();
        checkIfNtpLoaded();
    }

    @VisibleForTesting
    public void showPriceDropIPH() {
        ToggleTabStackButton toggleTabStackButton =
                mControlContainer.findViewById(R.id.tab_switcher_button);
        HighlightParams params = new HighlightParams(HighlightShape.CIRCLE);
        params.setBoundsRespectPadding(true);
        int yInset =
                mControlContainer
                        .getResources()
                        .getDimensionPixelOffset(
                                R.dimen.price_drop_spotted_iph_ntp_tabswitcher_y_inset);
        mUserEducationHelper.requestShowIPH(
                new IPHCommandBuilder(
                                mControlContainer.getResources(),
                                FeatureConstants.PRICE_DROP_NTP_FEATURE,
                                R.string.price_drop_spotted_iph,
                                R.string.price_drop_spotted_iph)
                        .setInsetRect(new Rect(0, 0, 0, -yInset))
                        .setAnchorView(toggleTabStackButton)
                        .setHighlightParams(params)
                        .setDismissOnTouch(true)
                        .build());
    }

    /**
     * Checks to to see if there are any unseen price drops, and if so attempts to show the price
     * drop IPH. An unseen price drop occurs when there is a tab with a price drop that has not been
     * viewed in the tab switcher grid.
     */
    private void maybeShowPriceDropIPH() {
        if (mTabModelSelector == null) return;
        Profile profile = mTabModelSelector.getCurrentModel().getProfile();
        if (profile.isOffTheRecord()) return;

        if (!PriceTrackingUtilities.isTrackPricesOnTabsEnabled(profile)
                || !PriceTrackingFeatures.isPriceDropIphEnabled(profile)) {
            return;
        }
        TabModel tabModel = mTabModelSelector.getCurrentModel();
        for (int i = 0; i < tabModel.getCount(); i++) {
            ShoppingPersistedTabData.from(
                    tabModel.getTabAt(i),
                    (shoppingPersistedTabData) -> {
                        if (shoppingPersistedTabData != null
                                && shoppingPersistedTabData.getPriceDrop() != null
                                && !shoppingPersistedTabData.getIsCurrentPriceDropSeen()) {
                            showPriceDropIPH();
                        }
                    });
        }
    }

    private void checkIfNtpLoaded() {
        NewTabPage ntp = getNewTabPageForCurrentTab();
        if (ntp != null) {
            ntp.setOmniboxStub(mLocationBar.getOmniboxStub());
            mLocationBarModel.notifyNtpStartedLoading();
            maybeShowPriceDropIPH();
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
        // the
        //                dependencies are ready. This logic was introduced to move asynchronous
        //                observer events from the infra (LayoutManager) into the feature using
        //                it.
        if (mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)) {
            mControlContainer.post(
                    mCallbackController.makeCancelable(
                            () -> updateForLayout(LayoutType.TAB_SWITCHER)));
        }

        mAppThemeColorProvider.setLayoutStateProvider(mLayoutStateProvider);
        if (mBottomControlsCoordinatorSupplier.get() != null) {
            mBottomControlsCoordinatorSupplier.get().setLayoutStateProvider(mLayoutStateProvider);
        }
    }

    private void updateTabLoadingState(boolean updateUrl) {
        if (mIsDestroyed) return;

        mLocationBarModel.notifySecurityStateChanged();
        if (updateUrl) {
            mLocationBarModel.notifyUrlChanged();
            updateButtonStatus();
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

    /**
     * Called whenever the NTP could have been entered or exited (e.g. tab content changed, tab
     * navigated to from the tab strip/tab switcher, etc.). If the user is on a tablet and indeed
     * entered or exited from the NTP, we will check the following cases:
     *   1. If a11y is enabled, we will request a11y focus on the omnibox (e.g. for TalkBack) on the
     * NTP.
     *   2. If a keyboard is plugged in, we will show the URL bar cursor (without focus animations)
     * on entering the NTP.
     *   3. If a keyboard is plugged in, we will clear focus established in #2 above on exiting
     * from the NTP.
     */
    private void maybeShowOrClearCursorInLocationBar() {
        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) return;
        Tab tab = mLocationBarModel.getTab();
        if (tab == null) return;
        NativePage nativePage = tab.getNativePage();
        boolean onNtp = UrlUtilities.isNtpUrl(tab.getUrl());

        if (ChromeAccessibilityUtil.get().isAccessibilityEnabled()
                && nativePage instanceof NewTabPage) {
            mLocationBar.requestUrlBarAccessibilityFocus();
        }

        // While a hardware keyboard is connected, loading the NTP should cause the URL bar to gain
        // focus with a blinking cursor and without focus animations. Loading a non-NTP URL should
        // clear such focus if it exists.
        if (mActivity.getResources().getConfiguration().keyboard == Configuration.KEYBOARD_QWERTY) {
            if (onNtp) {
                mLocationBar.showUrlBarCursorWithoutFocusAnimations();
            } else {
                mLocationBar.clearUrlBarCursorWithoutFocusAnimations();
            }
        }
    }

    private void maybeShowUrlBarCursorIfHardwareKeyboardAvailable() {
        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) return;
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

    private void onBackPressStateChanged() {
        Tab tab = mActivityTabProvider.get();
        mBackPressStateSupplier.set(tab != null && mToolbarTabController.canGoBack());
    }

    private void onBackForwardTransitionAnimationChange() {
        Tab tab = mActivityTabProvider.get();
        final boolean backGestureInvokeAnimationInProgress =
                tab != null
                        && tab.getWebContents().getCurrentBackForwardTransitionStage()
                                == AnimationStage.INVOKE_ANIMATION;
        mToolbar.setShowingProgressBarForBackForwardTransition(
                backGestureInvokeAnimationInProgress);
    }

    public @BackPressResult int handleBackPress() {
        boolean ret = back();
        onBackPressStateChanged();
        return ret ? BackPressResult.SUCCESS : BackPressResult.FAILURE;
    }

    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressStateSupplier;
    }

    /**
     * Overviews that are not transitively owned by this class need to update this observer when
     * they update their alpha during animations.
     */
    public ToolbarAlphaInOverviewObserver getToolbarAlphaInOverviewObserver() {
        return mToolbar.getToolbarAlphaInOverviewObserver();
    }

    /** Returns {@link LocationBar}. */
    public LocationBar getLocationBar() {
        return mLocationBar;
    }

    /** Returns {@link LocationBarModel} for access in tests. */
    public LocationBarModel getLocationBarModelForTesting() {
        return mLocationBarModel;
    }

    /**
     * @return The {@link ToolbarLayout} that constitutes the toolbar.
     */
    public ToolbarLayout getToolbarLayoutForTesting() {
        return mToolbar.getToolbarLayoutForTesting();
    }

    public HomeButtonCoordinator getHomeButtonCoordinatorForTesting() {
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

    public BottomControlsCoordinator getBottomControlsCoordinatorForTesting() {
        return mBottomControlsCoordinatorSupplier.get();
    }

    public ToggleTabStackButtonCoordinator getTabSwitcherButtonCoordinatorForTesting() {
        return mTabSwitcherButtonCoordinator;
    }
}
