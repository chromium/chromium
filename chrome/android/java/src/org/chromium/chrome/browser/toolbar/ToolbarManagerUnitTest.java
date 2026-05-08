// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.when;

import android.view.ViewStub;

import androidx.appcompat.app.AppCompatActivity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.Callback;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.overlay_panel.OverlayPanelManager;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.findinpage.FindToolbarManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.glic.GlicKeyedService;
import org.chromium.chrome.browser.glic.GlicKeyedServiceFactory;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponent;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponentSupplier;
import org.chromium.chrome.browser.layouts.CompositorModelChangeProcessor;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayerJni;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustSignalsCoordinator;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestrator;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.omnibox.ChromeAutocompleteSchemeClassifier;
import org.chromium.chrome.browser.omnibox.ChromeAutocompleteSchemeClassifierJni;
import org.chromium.chrome.browser.omnibox.OmniboxChipManager;
import org.chromium.chrome.browser.omnibox.fusebox.ComposeboxQueryControllerBridge;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxActionDelegateImpl;
import org.chromium.chrome.browser.paint_preview.services.PaintPreviewTabServiceFactory;
import org.chromium.chrome.browser.paint_preview.services.PaintPreviewTabServiceFactoryJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tab.TabViewManager;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabModelDotInfo;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.theme.AdjustedTopUiThemeColorProvider;
import org.chromium.chrome.browser.theme.BottomUiThemeColorProvider;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator.VisibilityDelegate;
import org.chromium.chrome.browser.toolbar.top.ToolbarActionModeCallback;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer;
import org.chromium.chrome.browser.toolbar.top.TopToolbarSceneLayer;
import org.chromium.chrome.browser.toolbar.top.TopToolbarSceneLayerJni;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuDelegate;
import org.chromium.chrome.browser.ui.bottombar.BottomBarHostManager;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.TopInsetProvider;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;
import org.chromium.components.browser_ui.accessibility.PageZoomManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.omnibox.OmniboxFocusReason;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.TokenHolder;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.function.Supplier;

/** Unit tests for {@link ToolbarManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    ChromeFeatureList.HTTPS_FIRST_DIALOG_UI,
    SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT,
    SigninFeatures.ENABLE_SEAMLESS_SIGNIN
})
@DisableFeatures({SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS})
public class ToolbarManagerUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private MultiInstanceOrchestrator mMultiInstanceOrchestrator;
    @Mock private BottomControlsStacker mBottomControlsStacker;
    @Mock private LayoutManagerImpl mLayoutManager;
    @Mock private CompositorModelChangeProcessor mCompositorModelChangeProcessor;
    @Mock private OverlayPanelManager mOverlayPanelManager;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModelDotInfo mTabModelDotInfo;
    @Mock private LocationBarModel.Natives mLocationBarModelNatives;
    @Mock private FaviconHelper.Natives mFaviconHelperNatives;
    @Mock private LargeIconBridge.Natives mLargeIconBridgeNatives;
    @Mock private ChromeAutocompleteSchemeClassifier.Natives mChromeAutocompleteSchemeClassifierJni;
    @Mock private PaintPreviewTabServiceFactory.Natives mPaintPreviewTabServiceFactoryNatives;
    @Mock private Runnable mOpenGridTabSwitcherHandler;
    @Mock private Tracker mTracker;
    @Mock private TopToolbarSceneLayer.Natives mTopToolbarSceneLayerNatives;
    @Mock private SceneLayer.Natives mSceneLayerNatives;
    @Mock private TabModel mTabModel;
    @Mock private Tab mTab;
    @Mock private TabViewManager mTabViewManager;
    @Mock private Profile mProfile;
    @Mock private BrowserControlsSizer mControlsSizer;
    @Mock private FullscreenManager mFullscreenManager;
    @Mock private CompositorViewHolder mCompositorViewHolder;
    @Mock private Callback<Boolean> mUrlFocusChangedCallback;
    @Mock private TopUiThemeColorProvider mTopUiThemeColorProvider;
    @Mock private AdjustedTopUiThemeColorProvider mAdjustedTopUiThemeColorProvider;
    @Mock private TabObscuringHandler mTabObscuringHandler;
    @Mock private ScrimManager mScrimManager;
    @Mock private ToolbarActionModeCallback mToolbarActionModeCallback;
    @Mock private FindToolbarManager mFindToolbarManager;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ActivityResultTracker mActivityResultTracker;
    @Mock private DeviceLockActivityLauncher mDeviceLockActivityLauncher;
    @Mock private StatusBarColorController mStatusBarColorController;
    @Mock private AppMenuDelegate mAppMenuDelegate;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private DataSharingTabManager mDataSharingTabManager;
    @Mock private TabContentManager mTabContentManager;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private OmniboxActionDelegateImpl mOmniboxActionDelegate;
    @Mock private BackPressManager mBackPressManager;
    @Mock private DesktopWindowStateManager mDesktopWindowStateManager;
    @Mock private TokenHolder mLockTopControlsTokenJar;
    @Mock private VisibilityDelegate mMenuButtonVisibilityDelegate;
    @Mock private TopControlsStacker mTopControlsStacker;
    @Mock private TopInsetProvider mTopInsetProvider;
    @Mock private PageZoomManager mPageZoomManager;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private OmniboxChipManager mOmniboxChipManager;
    @Mock private BottomBarHostManager mBottomBarHostManager;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private BottomUiThemeColorProvider mBottomUiThemeColorProvider;
    @Mock private IncognitoStateProvider mIncognitoStateProvider;
    @Mock private DisplayAndroid mDisplayAndroid;
    @Mock private KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    @Mock private InsetObserver mInsetObserver;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private AutocompleteController mAutocompleteController;
    @Mock private IdentityManager mIdentityManager;
    @Mock private SigninManager mSigninManager;
    @Mock private SyncService mSyncService;
    @Mock private ActionRegistry mActionRegistry;
    @Mock private PropertyModel mActionPropertyModel;
    @Mock private ActorKeyedService mActorKeyedService;
    @Mock private GlicKeyedService mGlicKeyedService;

    private ActivityController<TestActivity> mActivityController;
    private ToolbarManager mToolbarManager;
    private TopToolbarSceneLayer mTopToolbarSceneLayerInstance;

    @Before
    @SuppressWarnings("unchecked") // Raw CompositorModelChangeProcessor mock.
    public void setUp() {
        ActorKeyedServiceFactory.setForTesting(mActorKeyedService);
        GlicKeyedServiceFactory.setForTesting(mGlicKeyedService);
        ComposeboxQueryControllerBridge.setInstanceForTesting(null);
        ChromeAutocompleteSchemeClassifierJni.setInstanceForTesting(
                mChromeAutocompleteSchemeClassifierJni);
        when(mChromeAutocompleteSchemeClassifierJni.createAutocompleteClassifier(any()))
                .thenReturn(1L);
        FaviconHelperJni.setInstanceForTesting(mFaviconHelperNatives);
        when(mFaviconHelperNatives.init()).thenReturn(1L);
        LargeIconBridgeJni.setInstanceForTesting(mLargeIconBridgeNatives);
        when(mLargeIconBridgeNatives.init()).thenReturn(1L);
        PaintPreviewTabServiceFactoryJni.setInstanceForTesting(
                mPaintPreviewTabServiceFactoryNatives);
        when(mPaintPreviewTabServiceFactoryNatives.getServiceInstanceForCurrentProfile())
                .thenReturn(null);
        LocationBarModelJni.setInstanceForTesting(mLocationBarModelNatives);
        when(mLocationBarModelNatives.init(any())).thenReturn(1L);
        SceneLayerJni.setInstanceForTesting(mSceneLayerNatives);
        when(mLocationBarModelNatives.getUrlOfVisibleNavigationEntry(anyLong()))
                .thenReturn(GURL.emptyGURL());
        when(mLocationBarModelNatives.getFormattedFullURL(anyLong())).thenReturn("");
        when(mLocationBarModelNatives.getURLForDisplay(anyLong())).thenReturn("");
        TopToolbarSceneLayerJni.setInstanceForTesting(mTopToolbarSceneLayerNatives);
        when(mTopToolbarSceneLayerNatives.init(any()))
                .thenAnswer(
                        invocation -> {
                            mTopToolbarSceneLayerInstance = invocation.getArgument(0);
                            mTopToolbarSceneLayerInstance.setNativePtr(1L);
                            return 1L;
                        });
        doAnswer(
                        invocation -> {
                            if (mTopToolbarSceneLayerInstance != null) {
                                mTopToolbarSceneLayerInstance.setNativePtr(0L);
                            }
                            return null;
                        })
                .when(mSceneLayerNatives)
                .destroy(anyLong());
        TrackerFactory.setTrackerForTests(mTracker);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        AutocompleteController.setInstanceForTesting(mAutocompleteController);
        IdentityServicesProvider.setIdentityManagerForTesting(mIdentityManager);
        IdentityServicesProvider.setSigninManagerForTesting(mSigninManager);
        SyncServiceFactory.setInstanceForTesting(mSyncService);

        mActivityController = Robolectric.buildActivity(TestActivity.class).setup();
        AppCompatActivity activity = mActivityController.get();
        activity.setContentView(R.layout.main);
        ViewStub controlStub = activity.findViewById(R.id.control_container_stub);
        controlStub.setLayoutResource(R.layout.control_container);
        ToolbarControlContainer controlContainer = (ToolbarControlContainer) controlStub.inflate();
        controlContainer.initWithToolbar(R.layout.toolbar_phone, R.dimen.toolbar_height_no_shadow);

        MultiInstanceOrchestratorFactory.setInstanceForTesting(mMultiInstanceOrchestrator);
        BrowserStateBrowserControlsVisibilityDelegate browserVisibilityDelegate =
                new BrowserStateBrowserControlsVisibilityDelegate(
                        ObservableSuppliers.createNonNull(false));
        when(mControlsSizer.getBrowserVisibilityDelegate()).thenReturn(browserVisibilityDelegate);
        NonNullObservableSupplier<Boolean> compositorInMotionSupplier =
                ObservableSuppliers.createNonNull(false);
        when(mCompositorViewHolder.getInMotionSupplier()).thenReturn(compositorInMotionSupplier);
        when(mDisplayAndroid.getDisplayHeight()).thenReturn(1000);
        when(mWindowAndroid.getDisplay()).thenReturn(mDisplayAndroid);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(activity));
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(activity));

        when(mTabModelSelector.getCurrentTab()).thenReturn(mTab);
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mTab.getTabViewManager()).thenReturn(mTabViewManager);
        UserDataHost userDataHost = new UserDataHost();
        when(mTab.getUserDataHost()).thenReturn(userDataHost);
        when(mTab.isInitialized()).thenReturn(true);
        when(mTab.getUrl()).thenReturn(GURL.emptyGURL());
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mTabModelSelector.getCurrentModelTabCountSupplier())
                .thenReturn(ObservableSuppliers.createNonNull(0));
        when(mTabModelSelector.getCurrentTabSupplier())
                .thenReturn(ObservableSuppliers.createNullable(mTab));
        SettableMonotonicObservableSupplier<TabModel> currentTabModelSupplier =
                ObservableSuppliers.createMonotonic();
        currentTabModelSupplier.set(mTabModel);
        when(mTabModelSelector.getCurrentTabModelSupplier()).thenReturn(currentTabModelSupplier);
        when(mLayoutManager.getOverlayPanelManager()).thenReturn(mOverlayPanelManager);
        when(mLayoutManager.createCompositorMCP(any(), any(), any()))
                .thenReturn(mCompositorModelChangeProcessor);

        UnownedUserDataHost unownedUserDataHost = new UnownedUserDataHost();
        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(unownedUserDataHost);
        SettableMonotonicObservableSupplier<ManualFillingComponent> manualFillingComponentSupplier =
                ObservableSuppliers.createMonotonic();
        ManualFillingComponentSupplier.attach(unownedUserDataHost, manualFillingComponentSupplier);
        when(mWindowAndroid.getKeyboardDelegate()).thenReturn(mKeyboardVisibilityDelegate);
        KeyboardVisibilityDelegate.setInstanceForTesting(mKeyboardVisibilityDelegate);
        when(mWindowAndroid.getInsetObserver()).thenReturn(mInsetObserver);
        when(mInsetObserver.getSupplierForKeyboardInset())
                .thenReturn(ObservableSuppliers.createNonNull(0));

        when(mActionRegistry.get(ActionId.NEW_TAB))
                .thenReturn(ObservableSuppliers.createNullable(mActionPropertyModel));

        SettableMonotonicObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier =
                ObservableSuppliers.createMonotonic();
        SettableMonotonicObservableSupplier<ShareDelegate> shareDelegateSupplier =
                ObservableSuppliers.createMonotonic();
        SettableNonNullObservableSupplier<Profile> profileSupplier =
                ObservableSuppliers.createNonNull(mProfile);
        SettableMonotonicObservableSupplier<BookmarkModel> bookmarkModelSupplier =
                ObservableSuppliers.createMonotonic();
        OneshotSupplierImpl<LayoutStateProvider> layoutStateProviderSupplier =
                new OneshotSupplierImpl<>();
        OneshotSupplierImpl<AppMenuCoordinator> appMenuCoordinatorSupplier =
                new OneshotSupplierImpl<>();
        SettableMonotonicObservableSupplier<TabModelSelector> tabModelSelectorSupplier =
                ObservableSuppliers.createMonotonic();
        tabModelSelectorSupplier.set(mTabModelSelector);
        SettableNonNullObservableSupplier<Boolean> omniboxFocusStateSupplier =
                ObservableSuppliers.createNonNull(false);
        OneshotSupplierImpl<Boolean> promoShownOneshotSupplier = new OneshotSupplierImpl<>();
        OneshotSupplierImpl<ChromeAndroidTask> chromeAndroidTaskSupplier =
                new OneshotSupplierImpl<>();
        Supplier<Boolean> isInOverviewModeSupplier = () -> false;
        SettableNonNullObservableSupplier<ModalDialogManager> modalDialogManagerSupplier =
                ObservableSuppliers.createNonNull(mModalDialogManager);
        SettableMonotonicObservableSupplier<MerchantTrustSignalsCoordinator>
                merchantTrustSignalsCoordinatorSupplier = ObservableSuppliers.createMonotonic();
        SettableMonotonicObservableSupplier<EphemeralTabCoordinator>
                ephemeralTabCoordinatorSupplier = ObservableSuppliers.createMonotonic();
        SettableMonotonicObservableSupplier<ReadAloudController> readAloudControllerSupplier =
                ObservableSuppliers.createMonotonic();
        SettableMonotonicObservableSupplier<TabBookmarker> tabBookmarkerSupplier =
                ObservableSuppliers.createMonotonic();
        SettableNonNullObservableSupplier<Boolean> xrSpaceModeObservableSupplier =
                ObservableSuppliers.createNonNull(false);

        ActivityTabProvider activityTabProvider = new ActivityTabProvider();
        activityTabProvider.setForTesting(mTab);

        mToolbarManager =
                new ToolbarManager(
                        activity,
                        mBottomControlsStacker,
                        mControlsSizer,
                        mFullscreenManager,
                        edgeToEdgeControllerSupplier,
                        controlContainer,
                        mCompositorViewHolder,
                        mUrlFocusChangedCallback,
                        mTopUiThemeColorProvider,
                        mAdjustedTopUiThemeColorProvider,
                        mBottomUiThemeColorProvider,
                        mIncognitoStateProvider,
                        mTabObscuringHandler,
                        shareDelegateSupplier,
                        /* buttonDataProviders= */ new ArrayList<>(),
                        activityTabProvider,
                        mScrimManager,
                        mToolbarActionModeCallback,
                        mFindToolbarManager,
                        profileSupplier,
                        bookmarkModelSupplier,
                        layoutStateProviderSupplier,
                        appMenuCoordinatorSupplier,
                        /* canShowUpdateBadge= */ false,
                        tabModelSelectorSupplier,
                        omniboxFocusStateSupplier,
                        promoShownOneshotSupplier,
                        mWindowAndroid,
                        mActivityResultTracker,
                        mDeviceLockActivityLauncher,
                        chromeAndroidTaskSupplier,
                        isInOverviewModeSupplier,
                        modalDialogManagerSupplier,
                        mStatusBarColorController,
                        mAppMenuDelegate,
                        mActivityLifecycleDispatcher,
                        mBottomSheetController,
                        mDataSharingTabManager,
                        mTabContentManager,
                        mTabCreatorManager,
                        merchantTrustSignalsCoordinatorSupplier,
                        mOmniboxActionDelegate,
                        ephemeralTabCoordinatorSupplier,
                        /* initializeWithIncognitoColors= */ false,
                        mBackPressManager,
                        readAloudControllerSupplier,
                        mDesktopWindowStateManager,
                        mLockTopControlsTokenJar,
                        tabBookmarkerSupplier,
                        mMenuButtonVisibilityDelegate,
                        mTopControlsStacker,
                        mTopInsetProvider,
                        xrSpaceModeObservableSupplier,
                        mPageZoomManager,
                        mSnackbarManager,
                        mOmniboxChipManager,
                        mBottomBarHostManager,
                        mActionRegistry,
                        /* toggleGlicCallback= */ (preventClose) -> {});

        NonNullObservableSupplier<TabModelDotInfo> dotSupplier =
                ObservableSuppliers.createNonNull(mTabModelDotInfo);
        mToolbarManager.initializeWithNative(
                mLayoutManager,
                /* stripLayoutHelperManager= */ null,
                mOpenGridTabSwitcherHandler,
                /* bookmarkClickHandler= */ null,
                /* customTabsBackClickHandler= */ null,
                /* archivedTabCountSupplier= */ null,
                dotSupplier,
                /* undoBarThrottle= */ null,
                /* contextMenuPopulatorFactory= */ null,
                /* selectionDropdownMenuDelegate= */ null);

        RobolectricUtil.runAllBackgroundAndUi();
    }

    @After
    public void tearDown() {
        mActivityController.close();
    }

    @Test
    public void testSetUrlBarFocusAfterDestroy() {
        mToolbarManager.setUrlBarFocus(true, OmniboxFocusReason.OMNIBOX_TAP);
        assertTrue(mToolbarManager.isUrlBarFocused());

        mToolbarManager.setUrlBarFocus(false, OmniboxFocusReason.OMNIBOX_TAP);
        assertFalse(mToolbarManager.isUrlBarFocused());

        mToolbarManager.destroy();

        mToolbarManager.setUrlBarFocus(true, OmniboxFocusReason.OMNIBOX_TAP);
        assertFalse(mToolbarManager.isUrlBarFocused());
    }
}
