// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.os.Build;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.DeviceInfo;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.TriState;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.composeplate.ComposeplateCoordinator;
import org.chromium.chrome.browser.composeplate.ComposeplateUtils;
import org.chromium.chrome.browser.composeplate.ComposeplateUtilsJni;
import org.chromium.chrome.browser.feed.FeedStreamViewResizerUtils;
import org.chromium.chrome.browser.feed.FeedSurfaceScrollDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.logo.LogoCoordinator;
import org.chromium.chrome.browser.logo.LogoUtils;
import org.chromium.chrome.browser.magic_stack.ModuleRegistry;
import org.chromium.chrome.browser.ntp.NewTabPage.NtpScrollListener;
import org.chromium.chrome.browser.ntp.search.SearchBoxCoordinator;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinatorFactory;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationTestHelper;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.theme.NtpCustomizationPromoManager;
import org.chromium.chrome.browser.omnibox.SearchEngineService;
import org.chromium.chrome.browser.omnibox.fusebox.ComposeboxQueryControllerBridge;
import org.chromium.chrome.browser.omnibox.fusebox.ComposeboxQueryControllerBridgeJni;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.segmentation_platform.client_util.HomeModulesRankingHelper;
import org.chromium.chrome.browser.segmentation_platform.client_util.HomeModulesRankingHelperJni;
import org.chromium.chrome.browser.signin.services.AccountPreviewDataService;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesCoordinator;
import org.chromium.chrome.browser.suggestions.tile.TileGroup;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.tasks.HomeSurfaceTracker;
import org.chromium.chrome.browser.ui.bottombar.BottomBarEducationCoordinator;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.TouchEnabledDelegate;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.UiUpdateRequest;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.UiUpdateRequest.UpdateReason;
import org.chromium.chrome.browser.ui.side_ui.SideUiObserver;
import org.chromium.chrome.browser.ui.side_ui.SideUiStateProvider;
import org.chromium.chrome.test.util.browser.offlinepages.FakeOfflinePageBridge;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.displaystyle.DisplayStyleObserver;
import org.chromium.components.browser_ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.VerticalDisplayStyle;
import org.chromium.components.contextual_search.InputState;
import org.chromium.components.contextual_search.InputStateBuilder;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.IconProto.Icon;
import org.chromium.components.omnibox.IconResourceIdsProto.IconResourceIds;
import org.chromium.components.omnibox.InputSourceProto.InputSource;
import org.chromium.components.omnibox.InputTypeConfigProto.InputTypeConfig;
import org.chromium.components.omnibox.InputTypeProto.InputType;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.omnibox.OmniboxFocusReason;
import org.chromium.components.omnibox.ToolConfigProto.ToolConfig;
import org.chromium.components.omnibox.ToolModeProtoIntDef.ToolMode;
import org.chromium.components.search_engines.AiModeButtonUiConfig;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;
import java.util.function.Supplier;

/** Unit tests for {@link NewTabPageCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER_V2,
    ChromeFeatureList.FEED_CONTAINMENT,
    SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS,
    SigninFeatures.ENABLE_ACCOUNT_PREVIEW_PREFERRED_ACCOUNT
})
public class NewTabPageCoordinatorUnitTest {
    private static final String THIRD_PARTY_AI_MODE_NAVIGATION_URL =
            JUnitTestGURLs.EXAMPLE_URL.getSpec();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();
    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    @Mock private ComposeplateUtils.Natives mMockComposeplateUtilsJni;
    @Mock private ComposeboxQueryControllerBridge mComposeboxQueryControllerBridge;
    @Mock private ComposeboxQueryControllerBridge.Natives mComposeboxBridgeJni;
    @Mock private HomeModulesRankingHelper.Natives mHomeModulesRankingHelperJniMock;
    @Mock private NewTabPageManager mManager;
    @Mock private Tab mTab;
    @Mock private Tab mMostRecentTab;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private HomeSurfaceTracker mHomeSurfaceTracker;
    @Mock private ModuleRegistry mModuleRegistry;
    @Mock private Profile mProfile;
    @Mock private TabModel mTabModel;
    @Mock private TileGroup.Delegate mTileGroupDelegate;
    @Mock private FeedSurfaceScrollDelegate mScrollDelegate;
    @Mock private TouchEnabledDelegate mTouchEnabledDelegate;
    @Mock private UiConfig mUiConfig;
    @Mock private ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ActivityResultTracker mActivityResultTracker;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private Supplier<Integer> mTabStripHeightSupplier;
    @Mock private SearchEngineService mSearchEngineService;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private IdentityManager mIdentityManager;
    @Mock private SigninManager mSigninManager;
    @Mock private AccountPreviewDataService mAccountPreviewDataService;
    @Mock private SyncService mSyncService;
    @Mock private BackPressManager mBackPressManager;
    @Mock private SearchBoxCoordinator mMockSearchBox;
    @Mock private LogoCoordinator mMockLogo;
    @Mock private MostVisitedTilesCoordinator mMockTiles;
    @Mock private ComposeplateCoordinator mMockComposeplate;
    @Mock private BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    @Mock private SideUiStateProvider mSideUiStateProvider;
    @Captor private ArgumentCaptor<DisplayStyleObserver> mDisplayStyleObserverCaptor;
    @Captor private ArgumentCaptor<SideUiObserver> mSideUiObserverCaptor;

    private Activity mActivity;
    private View mSearchBoxView;
    private RecyclerView mRecyclerView;
    private NewTabPageLayout mNewTabPageLayout;
    private NewTabPageCoordinator mCoordinator;
    private BrowserStateBrowserControlsVisibilityDelegate mVisibilityDelegate;
    private final OneshotSupplierImpl<ModuleRegistry> mModuleRegistrySupplier =
            new OneshotSupplierImpl<>();
    private final OneshotSupplierImpl<SideUiStateProvider> mSideUiStateProviderSupplier =
            new OneshotSupplierImpl<>();
    private final SettableNullableObservableSupplier<AiModeButtonUiConfig>
            mAiModeButtonUiConfigSupplier = ObservableSuppliers.createNullable();
    private final SettableNonNullObservableSupplier<StatusIconResource> mAiModeButtonIconSupplier =
            ObservableSuppliers.createNonNull(
                    new StatusIconResource(R.drawable.ic_search_spark_24dp, Resources.ID_NULL));
    private final SettableMonotonicObservableSupplier<InputState> mInputStateSupplier =
            ObservableSuppliers.createMonotonic();

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mSearchBoxView = new View(mActivity);
        mRecyclerView = new RecyclerView(mActivity);

        // Setup for MV tiles.
        mSuggestionsDeps.getFactory().mostVisitedSites = new FakeMostVisitedSites();
        mSuggestionsDeps.getFactory().offlinePageBridge = new FakeOfflinePageBridge();

        // Setup for signin and sync.
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        IdentityServicesProvider.setIdentityManagerForTesting(mIdentityManager);
        IdentityServicesProvider.setSigninManagerForTesting(mSigninManager);
        IdentityServicesProvider.setAccountPreviewDataServiceForTesting(mAccountPreviewDataService);

        // Setup for the composeplate buttons.
        ComposeplateUtilsJni.setInstanceForTesting(mMockComposeplateUtilsJni);
        when(mMockComposeplateUtilsJni.isAimEntrypointEligible(mProfile)).thenReturn(true);
        when(mMockComposeplateUtilsJni.isEnabledByPolicy(mProfile)).thenReturn(true);
        ComposeboxQueryControllerBridge.setInstanceForTesting(mComposeboxQueryControllerBridge);
        ComposeboxQueryControllerBridgeJni.setInstanceForTesting(mComposeboxBridgeJni);
        when(mComposeboxBridgeJni.isFuseboxEligibleForProfile(any())).thenReturn(true);
        mInputStateSupplier.set(
                new InputStateBuilder().withAllowedTools(ToolMode.TOOL_MODE_IMAGE_GEN).build());
        when(mComposeboxQueryControllerBridge.getInputStateSupplier())
                .thenReturn(mInputStateSupplier);
        IncognitoUtils.setEnabledForTesting(true);

        // Setup for home modules.
        HomeModulesRankingHelperJni.setInstanceForTesting(mHomeModulesRankingHelperJniMock);
        mModuleRegistrySupplier.set(mModuleRegistry);

        // Setup for lens.
        WeakReference<Context> contextWeakReference = new WeakReference<>(mActivity);
        when(mWindowAndroid.getContext()).thenReturn(contextWeakReference);

        // Setup for search.
        SearchEngineService.setInstanceForTesting(mSearchEngineService);
        // By default the search engine offers an AI Mode entry point.
        mAiModeButtonUiConfigSupplier.set(createAiModeButtonUiConfig(/* isGoogle= */ true));
        when(mSearchEngineService.getAiModeButtonUiConfigSupplier())
                .thenReturn(mAiModeButtonUiConfigSupplier);
        when(mSearchEngineService.getAiModeButtonIconSupplier())
                .thenReturn(mAiModeButtonIconSupplier);

        when(mMostRecentTab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mProfile.isOffTheRecord()).thenReturn(false);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        when(mUiConfig.getCurrentDisplayStyle())
                .thenReturn(
                        new UiConfig.DisplayStyle(
                                HorizontalDisplayStyle.REGULAR, VerticalDisplayStyle.REGULAR));

        mVisibilityDelegate =
                new BrowserStateBrowserControlsVisibilityDelegate(
                        ObservableSuppliers.alwaysFalse());
        when(mBrowserControlsVisibilityManager.getBrowserVisibilityDelegate())
                .thenReturn(mVisibilityDelegate);
        when(mBrowserControlsVisibilityManager.getBottomControlsHeight()).thenReturn(100);

        createCoordinator();
    }

    @After
    public void tearDown() {
        mCoordinator.destroy();
    }

    @Test
    public void testShowHomeSurfaceUiOnNtp() {
        testShowHomeSurfaceUiOnNtpImpl(
                /* mostRecentTab= */ mMostRecentTab, /* isHomeSurface= */ true);
    }

    @Test
    public void testShowHomeSurfaceUiOnNtp_noMostVisitedTab() {
        testShowHomeSurfaceUiOnNtpImpl(/* mostRecentTab= */ null, /* isHomeSurface= */ false);
    }

    private void testShowHomeSurfaceUiOnNtpImpl(Tab mostRecentTab, boolean isHomeSurface) {
        assertFalse(mCoordinator.isHomeSurface());

        mCoordinator.showHomeSurfaceUiOnNtp(mostRecentTab);

        verifyIsHomeSurface(isHomeSurface);
    }

    @Test
    public void testOnHomeModulesShown() {
        boolean isVisible = true;
        ViewGroup homeModulesContainer = mCoordinator.getHomeModulesContainerForTesting();
        assertNotNull(homeModulesContainer);

        mCoordinator.onHomeModulesShown(isVisible);
        assertEquals(View.VISIBLE, homeModulesContainer.getVisibility());

        isVisible = false;
        mCoordinator.onHomeModulesShown(isVisible);
        assertEquals(View.GONE, homeModulesContainer.getVisibility());
    }

    @Test
    public void testOnTabSelected() {
        int tabId = 123;
        TabRemover tabRemover = mock(TabRemover.class);
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModel.getTabRemover()).thenReturn(tabRemover);

        mCoordinator.onTabSelected(tabId);

        verify(tabRemover).closeTabs(any(), /* allowDialog= */ eq(false));
        verify(mHomeSurfaceTracker).updateHomeSurfaceAndTrackingTabs(eq(null), eq(null));
    }

    @Test
    public void testInitializeHomeModules_TrackingTabReady() {
        mCoordinator.destroy();

        when(mHomeSurfaceTracker.isHomeSurfaceTab(mTab)).thenReturn(true);
        when(mHomeSurfaceTracker.getLastActiveTabToTrack()).thenReturn(mMostRecentTab);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("NewTabPage.AsHomeSurface", true)
                        .build();
        createCoordinator();

        verifyIsHomeSurface(/* isHomeSurface= */ true);
        assertNotNull(mCoordinator.getHomeModulesCoordinatorForTesting());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testInitializeHomeModules_NormalNtp() {
        mCoordinator.destroy();
        when(mHomeSurfaceTracker.isHomeSurfaceTab(mTab)).thenReturn(false);
        when(mTab.getLaunchType()).thenReturn(TabLaunchType.FROM_CHROME_UI);

        createCoordinator();

        verifyIsHomeSurface(/* isHomeSurface= */ false);
        assertNotNull(mCoordinator.getHomeModulesCoordinatorForTesting());
    }

    @Test
    public void testInitializeHomeModules_StartupNtp() {
        mCoordinator.destroy();
        when(mHomeSurfaceTracker.isHomeSurfaceTab(mTab)).thenReturn(false);
        when(mTab.getLaunchType()).thenReturn(TabLaunchType.FROM_STARTUP);

        createCoordinator();
        // Verifies that the HomeModulesCoordinator isn't created if the Ntp's tracking Tab isn't
        // ready.
        assertNull(mCoordinator.getHomeModulesCoordinatorForTesting());

        mCoordinator.showHomeSurfaceUiOnNtp(mMostRecentTab);
        verifyIsHomeSurface(/* isHomeSurface= */ true);
        assertNotNull(mCoordinator.getHomeModulesCoordinatorForTesting());
    }

    @Test
    public void testInitializeHomeModules_OnDesktop() {
        mCoordinator.destroy();
        DeviceInfo.setIsDesktopForTesting(true);

        createCoordinator();

        assertNull(mCoordinator.getHomeModulesCoordinatorForTesting());
    }

    @Test
    public void testDestroy() {
        mCoordinator.initializeLayoutChangeListener();
        PropertyModel model = mCoordinator.getModelForTesting();

        assertNotNull(model.get(NewTabPageLayoutProperties.DELEGATE));
        assertNotNull(model.get(NewTabPageLayoutProperties.ON_LAYOUT_CHANGE_LISTENER));
        assertNotNull(model.get(NewTabPageLayoutProperties.SEARCH_BOX_VIEW));

        mCoordinator.destroy();

        assertNull(model.get(NewTabPageLayoutProperties.DELEGATE));
        assertNull(model.get(NewTabPageLayoutProperties.ON_LAYOUT_CHANGE_LISTENER));
        assertNull(model.get(NewTabPageLayoutProperties.SEARCH_BOX_VIEW));
    }

    @Test
    public void testTriggerCustomizationBottomSheet() {
        NtpCustomizationCoordinatorFactory factory = mock(NtpCustomizationCoordinatorFactory.class);
        NtpCustomizationCoordinatorFactory.setInstanceForTesting(factory);
        NtpCustomizationCoordinator customizationCoordinator =
                mock(NtpCustomizationCoordinator.class);
        when(factory.create(any(), any(), any(), anyInt(), any(), any(), any()))
                .thenReturn(customizationCoordinator);
        assertFalse(NtpCustomizationUtils.isThemeTipBottomSheetShownFromSharedPreference());

        mCoordinator.triggerCustomizationBottomSheet();

        verify(customizationCoordinator).showBottomSheet();
        assertTrue(NtpCustomizationUtils.isThemeTipBottomSheetShownFromSharedPreference());

        NtpCustomizationUtils.resetSharedPreferenceForTesting();
    }

    @Test
    public void testSetUrlFocusAnimationsDisabled_ResetsTranslationAndAlphas() {
        PropertyModel model = mCoordinator.getModelForTesting();

        // 1. Set some non-zero transition Y translation and non-1.f alpha states.
        model.set(NewTabPageLayoutProperties.TRANSITION_Y, -150f);
        mCoordinator.setSearchBoxAlpha(0.2f);
        mCoordinator.setSearchProviderLogoAlpha(0.3f);

        assertEquals(-150f, model.get(NewTabPageLayoutProperties.TRANSITION_Y), 0.01f);
        assertEquals(0.2f, mCoordinator.getSearchBoxView().getAlpha(), 0.01f);

        View logoView = mNewTabPageLayout.findViewById(R.id.logo_container_view);
        if (logoView == null) {
            logoView = mNewTabPageLayout.findViewById(R.id.search_provider_logo);
        }
        assertNotNull(logoView);
        assertEquals(0.3f, logoView.getAlpha(), 0.01f);

        // 2. Disabling focus animations should reset everything to their resting states:
        // - TRANSITION_Y to 0f
        // - Search Box alpha to 1.f
        // - Search Provider Logo alpha to 1.f
        mCoordinator.setUrlFocusAnimationsDisabled(true);

        assertEquals(0f, model.get(NewTabPageLayoutProperties.TRANSITION_Y), 0.01f);
        assertEquals(1.f, mCoordinator.getSearchBoxView().getAlpha(), 0.01f);
        assertEquals(1.f, logoView.getAlpha(), 0.01f);

        // 3. Verify subsequent alpha updates are blocked while animations are disabled.
        mCoordinator.setSearchBoxAlpha(0.2f);
        mCoordinator.setSearchProviderLogoAlpha(0.3f);
        assertEquals(1.f, mCoordinator.getSearchBoxView().getAlpha(), 0.01f);
        assertEquals(1.f, logoView.getAlpha(), 0.01f);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    @Features.EnableFeatures({ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2})
    public void testMaybeTriggerCustomizationBottomSheet_triggersWhenEducationNotShown() {
        var customizationCoordinator = setUpThemeTipTrigger();

        mCoordinator.maybeTriggerCustomizationBottomSheet();

        verify(customizationCoordinator).showBottomSheet();
        assertTrue(NtpCustomizationUtils.isThemeTipBottomSheetShownFromSharedPreference());

        NtpCustomizationUtils.resetSharedPreferenceForTesting();
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    @Features.EnableFeatures({ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2})
    public void testMaybeTriggerCustomizationBottomSheet_deferredWhenEducationShown() {
        var customizationCoordinator = setUpThemeTipTrigger();
        BottomBarEducationCoordinator.setEducationShownThisSessionForTesting();

        mCoordinator.maybeTriggerCustomizationBottomSheet();

        verify(customizationCoordinator, never()).showBottomSheet();
        assertFalse(NtpCustomizationUtils.isThemeTipBottomSheetShownFromSharedPreference());
    }

    @Test
    public void testSetUrlFocusAnimationsDisabled_FalseRecalculatesTransitionY() {
        PropertyModel model = mCoordinator.getModelForTesting();

        // 1. Disable animations first (TRANSITION_Y is reset to 0f).
        mCoordinator.setUrlFocusAnimationsDisabled(true);
        assertEquals(0f, model.get(NewTabPageLayoutProperties.TRANSITION_Y), 0.01f);

        // 2. Set focus animation percentage to 1.0f. Since animations are disabled,
        // TRANSITION_Y should remain unchanged at 0f.
        mCoordinator.setUrlFocusChangeAnimationPercent(1.0f);
        assertEquals(0f, model.get(NewTabPageLayoutProperties.TRANSITION_Y), 0.01f);

        // 3. Force the search box view to have a simulated mock layout height in Robolectric.
        View searchBoxView = mCoordinator.getSearchBoxView();
        assertNotNull(searchBoxView);
        searchBoxView.layout(0, 100, 1080, 200); // Height is 100, bottom is 200.

        // 4. Re-enable focus animations. This should trigger onUrlFocusAnimationChanged()
        // and recalculate/apply a non-zero (negative) vertical translation.
        mCoordinator.setUrlFocusAnimationsDisabled(false);
        assertTrue(model.get(NewTabPageLayoutProperties.TRANSITION_Y) < 0f);
    }

    private void createCoordinator() {
        createCoordinator(/* isLff= */ false);
    }

    private void createCoordinator(boolean isLff) {
        mNewTabPageLayout =
                (NewTabPageLayout)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.new_tab_page_layout, null, false);

        mCoordinator =
                new NewTabPageCoordinator(
                        mManager,
                        mActivity,
                        mNewTabPageLayout,
                        mTab,
                        mTabModelSelector,
                        mModuleRegistrySupplier,
                        mProfile,
                        mWindowAndroid,
                        mActivityResultTracker,
                        mBottomSheetController,
                        mModalDialogManager,
                        mSnackbarManager,
                        isLff,
                        mTabStripHeightSupplier,
                        mSideUiStateProviderSupplier,
                        mHomeSurfaceTracker,
                        mBackPressManager,
                        mTemplateUrlService);

        mCoordinator.initialize(
                mTileGroupDelegate,
                /* searchProviderHasLogo= */ true,
                /* searchProviderIsGoogle= */ true,
                mScrollDelegate,
                mTouchEnabledDelegate,
                mUiConfig,
                mLifecycleDispatcher);
    }

    private void verifyIsHomeSurface(boolean isHomeSurface) {
        assertEquals(isHomeSurface, mCoordinator.isHomeSurface());
        assertNotNull(mCoordinator.getHomeModulesCoordinatorForTesting());
    }

    @Test
    public void testElementsMaxWidthLimit() {
        NewTabPageLayout layout = mCoordinator.getNewTabPageLayout();
        int maxSearchBoxWidthPx =
                mActivity.getResources().getDimensionPixelSize(R.dimen.ntp_search_box_max_width);

        // Set the parent measure width to be significantly larger than the max allowed width
        // to ensure the unconstrained width (width - margins) is guaranteed to exceed the cap.
        int measureWidth = maxSearchBoxWidthPx * 10;
        mCoordinator.onMeasure(measureWidth);

        // The max width cap is applied to Search Box, Composeplate and MVT on all platforms
        // to ensure visual alignment. On tablets, smaller tiles are used internally to fit.
        int expectedBoundedWidth = maxSearchBoxWidthPx;

        View searchBoxView = layout.findViewById(R.id.search_box);
        assertNotNull(searchBoxView);
        assertEquals(expectedBoundedWidth, searchBoxView.getLayoutParams().width);

        View composeplateView = layout.findViewById(R.id.composeplate_view);
        assertNotNull(composeplateView);
        int expectedComposeplateWidth = expectedBoundedWidth;
        if (NewTabPageUtils.isNtpAuroraButtonColorEnabled()) {
            int margin =
                    mActivity
                            .getResources()
                            .getDimensionPixelSize(R.dimen.composeplate_view_lateral_margin);
            expectedComposeplateWidth -= margin * 2;
        } else if (NewTabPageUtils.isNtpAuroraEnabled()) {
            int paddingForShadow =
                    mActivity
                            .getResources()
                            .getDimensionPixelSize(R.dimen.search_box_padding_for_shadow_lateral);
            expectedComposeplateWidth -= paddingForShadow * 2;
        }
        assertEquals(expectedComposeplateWidth, composeplateView.getLayoutParams().width);

        View logoView = layout.findViewById(R.id.logo_container_view);
        assertNotNull(logoView);
        assertEquals(measureWidth, logoView.getLayoutParams().width);

        View mvtView = layout.findViewById(R.id.mv_tiles_container);
        assertNotNull(mvtView);
        // On phones, MVT width matches the search box width to align visually in landscape.
        assertEquals(expectedBoundedWidth, mvtView.getLayoutParams().width);

        // Verify that the applied MVT margins are non-negative on Phone
        ViewGroup.MarginLayoutParams mvtMarginParams =
                (ViewGroup.MarginLayoutParams) mvtView.getLayoutParams();
        assertTrue(mvtMarginParams.leftMargin >= 0);
        assertTrue(mvtMarginParams.rightMargin >= 0);

        // On tablet / LFF, MVT width expands to match the Feed bounds.
        createCoordinator(/* isLff= */ true);
        layout = mCoordinator.getNewTabPageLayout();
        mvtView = layout.findViewById(R.id.mv_tiles_container);
        assertNotNull(mvtView);
        mvtView.setVisibility(View.VISIBLE);
        mCoordinator.onMeasure(measureWidth);
        int feedPadding =
                -FeedStreamViewResizerUtils.getFeedNtpCompensationMargin(
                        mActivity.getResources(), mUiConfig);
        assertEquals(measureWidth - (feedPadding * 2), mvtView.getLayoutParams().width);

        // Verify that the applied MVT margins are non-negative on Tablet
        ViewGroup.MarginLayoutParams tabletMarginParams =
                (ViewGroup.MarginLayoutParams) mvtView.getLayoutParams();
        assertTrue(tabletMarginParams.leftMargin >= 0);
        assertTrue(tabletMarginParams.rightMargin >= 0);
    }

    @Test
    @Config(qualifiers = "w839dp")
    public void testSearchBoxWidth_MediumWindowOnTablet() {
        testSearchBoxWidthInWideWindowOnTabletImpl(/* isExpandedWindow= */ false);
    }

    @Test
    @Config(qualifiers = "w840dp")
    public void testSearchBoxWidth_ExpandedWindowOnTablet() {
        testSearchBoxWidthInWideWindowOnTabletImpl(/* isExpandedWindow= */ true);
    }

    @Test
    @Config(qualifiers = "w839dp")
    public void testSearchBoxWidth_ResizeBetweenMediumAndExpandedWindowOnTablet() {
        setHorizontalDisplayStyle(HorizontalDisplayStyle.WIDE);
        createCoordinator(/* isLff= */ true);
        assertEquals(0, mCoordinator.getSearchBoxTwoSideMarginForTesting());

        // Crossing the expanded window width doesn't change the display style, but UiConfig still
        // notifies its observers on the configuration change.
        RuntimeEnvironment.setQualifiers("w840dp");
        notifyDisplayStyleObserver();
        assertEquals(
                getExpandedWindowSearchBoxTwoSideMargin(),
                mCoordinator.getSearchBoxTwoSideMarginForTesting());

        RuntimeEnvironment.setQualifiers("w839dp");
        notifyDisplayStyleObserver();
        assertEquals(0, mCoordinator.getSearchBoxTwoSideMarginForTesting());
    }

    @Test
    @Config(qualifiers = "w900dp")
    public void testSearchBoxWidth_ExpandedScreenWithHorizontalInsetOnTablet() {
        setHorizontalDisplayStyle(HorizontalDisplayStyle.WIDE);
        createCoordinator(/* isLff= */ true);
        assertEquals(
                getExpandedWindowSearchBoxTwoSideMargin(),
                mCoordinator.getSearchBoxTwoSideMarginForTesting());

        // The horizontal inset, e.g. of a side UI, is excluded from the window width, so the
        // window becomes a medium-width window.
        when(mUiConfig.getHorizontalInset()).thenReturn(100);
        notifyDisplayStyleObserver();

        assertEquals(0, mCoordinator.getSearchBoxTwoSideMarginForTesting());
    }

    /**
     * Verifies the search box width in a wide window on tablets. In medium-width windows, the
     * search box matches the MVT width, since the NTP is already inset by the Feed's lateral
     * paddings. In expanded windows, the search box has extra lateral margins.
     *
     * @param isExpandedWindow Whether the window is an expanded window.
     */
    private void testSearchBoxWidthInWideWindowOnTabletImpl(boolean isExpandedWindow) {
        setHorizontalDisplayStyle(HorizontalDisplayStyle.WIDE);
        createCoordinator(/* isLff= */ true);
        NewTabPageLayout layout = mCoordinator.getNewTabPageLayout();
        View mvtView = layout.findViewById(R.id.mv_tiles_container);
        mvtView.setVisibility(View.VISIBLE);
        Resources res = mActivity.getResources();
        // Below the max search box width, so that the search box width isn't capped.
        int measureWidth = res.getDimensionPixelSize(R.dimen.ntp_search_box_max_width) - 100;

        mCoordinator.onMeasure(measureWidth);

        int expectedTwoSideMargin =
                isExpandedWindow ? getExpandedWindowSearchBoxTwoSideMargin() : 0;
        assertEquals(
                measureWidth - expectedTwoSideMargin,
                layout.findViewById(R.id.search_box).getLayoutParams().width);
        assertEquals(measureWidth, mvtView.getLayoutParams().width);
    }

    @Test
    public void testOnDisplayStyleChanged_Tablet_UpdatesComposeplateNarrowWindowState() {
        int horizontalInsetDp = 20;
        when(mUiConfig.getHorizontalInset()).thenReturn(horizontalInsetDp);
        createCoordinator(/* isLff= */ true);
        setupMockSubCoordinators();

        notifyDisplayStyleObserver();

        verify(mMockComposeplate).updateIsInNarrowWindow(eq(horizontalInsetDp));
    }

    @Test
    public void testSigninPromoLateralMargins_NarrowWindowOnTablet() {
        when(mUiConfig.getCurrentDisplayStyle())
                .thenReturn(
                        new UiConfig.DisplayStyle(
                                HorizontalDisplayStyle.REGULAR, VerticalDisplayStyle.REGULAR));
        createCoordinator(/* isLff= */ true);

        mCoordinator.onMeasure(1000);

        int expectedMargin =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(
                                R.dimen.ntp_search_box_lateral_margin_narrow_window_tablet);
        assertEquals(expectedMargin, mCoordinator.getStartMargin());
    }

    @Test
    public void testSigninPromoLateralMargins_Phone() {
        when(mUiConfig.getCurrentDisplayStyle())
                .thenReturn(
                        new UiConfig.DisplayStyle(
                                HorizontalDisplayStyle.REGULAR, VerticalDisplayStyle.REGULAR));
        createCoordinator(/* isLff= */ false);

        mCoordinator.onMeasure(1000);

        int expectedMargin =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.mvt_container_lateral_margin);
        assertEquals(expectedMargin, mCoordinator.getStartMargin());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.NTP_AURORA + ":padding_style/0"})
    public void testSetSearchProviderTopMargin_WithLogo() {
        setupMockSubCoordinators();
        mCoordinator.setSearchProviderInfo(/* hasLogo= */ true, /* isGoogle= */ true);
        clearInvocations(mMockSearchBox, mMockLogo);

        mCoordinator.setSearchProviderTopMargin();

        Resources resources = mActivity.getResources();
        int logoTopMargin = resources.getDimensionPixelSize(R.dimen.ntp_logo_margin_top);
        verify(mMockSearchBox).setTopMargin(eq(0));
        verify(mMockLogo).setTopMargin(eq(logoTopMargin));
    }

    @Test
    public void testSetSearchProviderTopMargin_NoLogo() {
        setupMockSubCoordinators();
        mCoordinator.setSearchProviderInfo(/* hasLogo= */ false, /* isGoogle= */ false);
        clearInvocations(mMockSearchBox, mMockLogo);

        mCoordinator.setSearchProviderTopMargin();

        int searchBoxTopMarginNoLogo =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.ntp_search_box_top_margin_if_no_logo);
        verify(mMockSearchBox).setTopMargin(eq(searchBoxTopMarginNoLogo));
        verify(mMockLogo).setTopMargin(anyInt());
    }

    @Test
    public void testSetLogoViewBottomMargin() {
        setupMockSubCoordinators();
        clearInvocations(mMockLogo);

        mCoordinator.setLogoViewBottomMargin();

        int expectedBottomMargin =
                NtpCustomizationUtils.getLogoViewBottomMarginPx(mActivity.getResources());
        verify(mMockLogo).setBottomMargin(eq(expectedBottomMargin));
    }

    @Test
    public void testUpdateTilesLayoutMargins() {
        setupMockSubCoordinators();
        mCoordinator.setSearchProviderInfo(/* hasLogo= */ true, /* isGoogle= */ true);
        clearInvocations(mMockTiles);

        mCoordinator.updateTilesLayoutMargins();

        verify(mMockTiles).updateTilesLayoutMargins(eq(true), eq(false));
    }

    @Test
    public void testSetSearchBoxTextAppearance() {
        setupMockSubCoordinators();
        clearInvocations(mMockSearchBox);

        mCoordinator.setSearchBoxTextAppearance();

        int expectedStyle =
                mCoordinator.shouldApplyWhiteBackgroundOnSearchBox()
                        ? R.style.TextAppearance_FakeSearchBoxTextMediumDark
                        : R.style.TextAppearance_FakeSearchBoxTextMedium;
        verify(mMockSearchBox).setSearchBoxTextAppearance(eq(expectedStyle));
    }

    @Test
    public void testUpdateActionButtonVisibility() {
        setupMockSubCoordinators();
        when(mManager.isVoiceSearchEnabled()).thenReturn(true);
        when(mMockSearchBox.isLensEnabled(anyInt())).thenReturn(false);
        mCoordinator.setIsComposeplateEnabledForTesting(TriState.TRUE);
        mCoordinator.setSearchProviderInfo(/* hasLogo= */ true, /* isGoogle= */ true);
        clearInvocations(mMockSearchBox, mMockComposeplate);

        mCoordinator.updateActionButtonVisibility();

        verify(mMockSearchBox).setVoiceSearchButtonVisibility(eq(true));
        verify(mMockSearchBox).setLensButtonVisibility(eq(false));
        verify(mMockComposeplate).setVisibility(eq(true), anyBoolean());
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.AIM3P_ENTRYPOINT)
    public void testInitializeComposeplate() {
        recreateCoordinatorAndInitializeComposeplate(/* isLff= */ false);

        assertNotNull(mCoordinator.getComposeplateCoordinatorForTesting());
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.AIM3P_ENTRYPOINT)
    public void testInitializeComposeplate_Tablet_NarrowWindow() {
        testInitializeComposeplateOnTabletImpl(/* isInNarrowWindow= */ true);
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.AIM3P_ENTRYPOINT)
    public void testInitializeComposeplate_Tablet_RegularWindow() {
        testInitializeComposeplateOnTabletImpl(/* isInNarrowWindow= */ false);
    }

    /**
     * Verifies that the composeplate created on tablets uses the phone layout only in windows
     * narrower than {@code R.dimen.composeplate_view_lff_layout_min_window_width}, where the
     * optional button is icon-only.
     *
     * @param isInNarrowWindow Whether the window is narrower than the min window width of the large
     *     form factor layout.
     */
    private void testInitializeComposeplateOnTabletImpl(boolean isInNarrowWindow) {
        Resources res = mActivity.getResources();
        int minWindowWidthDp =
                Math.round(
                        res.getDimension(R.dimen.composeplate_view_lff_layout_min_window_width)
                                / res.getDisplayMetrics().density);
        int windowWidthDp = isInNarrowWindow ? minWindowWidthDp - 1 : minWindowWidthDp;
        RuntimeEnvironment.setQualifiers("+w" + windowWidthDp + "dp");
        recreateCoordinatorAndInitializeComposeplate(/* isLff= */ true);
        boolean expectedUseLffLayout = !isInNarrowWindow;

        View optionalButtonText =
                mCoordinator.getNewTabPageLayout().findViewById(R.id.optional_button_text);
        assertEquals(
                expectedUseLffLayout ? View.VISIBLE : View.GONE,
                optionalButtonText.getVisibility());
    }

    /**
     * Recreates the coordinator without a composeplate, then initializes the composeplate.
     *
     * @param isLff Whether the NTP is on a large form factor device.
     */
    private void recreateCoordinatorAndInitializeComposeplate(boolean isLff) {
        mCoordinator.destroy();
        when(mMockComposeplateUtilsJni.isAimEntrypointEligible(mProfile)).thenReturn(false);
        createCoordinator(isLff);
        assertNull(mCoordinator.getComposeplateCoordinatorForTesting());
        SearchBoxCoordinator mockSearchBox = mock(SearchBoxCoordinator.class);
        when(mockSearchBox.getView()).thenReturn(new View(mActivity));
        mCoordinator.setSearchBoxCoordinatorForTesting(mockSearchBox);

        mCoordinator.initializeComposeplate();
    }

    /**
     * Verifies when switching to a 3rd party search engine with no logo, expects fallback top
     * margin and hidden composeplate.
     */
    @Test
    @EnableFeatures({ChromeFeatureList.NTP_AURORA + ":padding_style/0"})
    @DisableFeatures(OmniboxFeatureList.AIM3P_ENTRYPOINT)
    public void testSetSearchProviderInfo_ThirdPartyNoLogo() {
        int searchBoxTopMarginNoLogo =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.ntp_search_box_top_margin_if_no_logo);
        verifySetSearchProviderInfo(
                /* targetHasLogo= */ false,
                /* targetIsGoogle= */ false,
                /* expectedSearchBoxTopMargin= */ searchBoxTopMarginNoLogo,
                /* expectedComposeplateVisible= */ false);
    }

    /**
     * Verifies when switching to a 3rd party search engine with a logo, expects standard 0px top
     * margin and hidden composeplate.
     */
    @Test
    @EnableFeatures({ChromeFeatureList.NTP_AURORA + ":padding_style/0"})
    @DisableFeatures(OmniboxFeatureList.AIM3P_ENTRYPOINT)
    public void testSetSearchProviderInfo_ThirdPartyWithLogo() {
        verifySetSearchProviderInfo(
                /* targetHasLogo= */ true,
                /* targetIsGoogle= */ false,
                /* expectedSearchBoxTopMargin= */ 0,
                /* expectedComposeplateVisible= */ false);
    }

    /**
     * Verifies when switching to Google default search engine, expects standard 0px top margin and
     * visible composeplate button.
     */
    @Test
    @EnableFeatures({ChromeFeatureList.NTP_AURORA + ":padding_style/0"})
    @DisableFeatures(OmniboxFeatureList.AIM3P_ENTRYPOINT)
    public void testSetSearchProviderInfo_GoogleWithLogo() {
        verifySetSearchProviderInfo(
                /* targetHasLogo= */ true,
                /* targetIsGoogle= */ true,
                /* expectedSearchBoxTopMargin= */ 0,
                /* expectedComposeplateVisible= */ true);
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.AIM3P_ENTRYPOINT)
    public void testSetSearchProviderInfo_InitializesComposeplate() {
        // Destroys the default coordinator created in setUp() since we need composeplate to start
        // disabled.
        mCoordinator.destroy();

        when(mMockComposeplateUtilsJni.isAimEntrypointEligible(mProfile)).thenReturn(false);
        createCoordinator();

        // Verifies composeplate was not initialized at startup.
        assertNull(mCoordinator.getComposeplateCoordinatorForTesting());

        // Setup mock SearchBoxCoordinator to verify side effect of
        // setSearchBoxHeightBoundsVerticalInset().
        SearchBoxCoordinator mockSearchBox = mock(SearchBoxCoordinator.class);
        View view = new View(mActivity);
        when(mockSearchBox.getView()).thenReturn(view);
        mCoordinator.setSearchBoxCoordinatorForTesting(mockSearchBox);

        // Enables composeplate eligibility so it is ready to be initialized.
        when(mMockComposeplateUtilsJni.isAimEntrypointEligible(mProfile)).thenReturn(true);

        // Moves state to non-Google provider first, then transition to Google.
        mCoordinator.setSearchProviderInfo(/* hasLogo= */ true, /* isGoogle= */ false);
        mCoordinator.setSearchProviderInfo(/* hasLogo= */ true, /* isGoogle= */ true);

        // Verifies composeplate has been initialized.
        assertNotNull(mCoordinator.getComposeplateCoordinatorForTesting());

        // Verifies setSearchBoxHeightBoundsVerticalInset() side effect on SearchBoxCoordinator.
        verify(mockSearchBox, atLeastOnce()).setHeight(anyInt());

        // Verifies that calling setSearchProviderInfo() a second time with the same Google
        // search provider does not crash or re-inflate the ViewStub.
        mCoordinator.setSearchProviderInfo(/* hasLogo= */ true, /* isGoogle= */ true);
        assertNotNull(mCoordinator.getComposeplateCoordinatorForTesting());
    }

    /**
     * Verifies that the AI Mode button is shown on the first NTP when the default search engine's
     * {@link AiModeButtonUiConfig} is already available before the coordinator is initialized.
     */
    @Test
    @EnableFeatures(OmniboxFeatureList.AIM3P_ENTRYPOINT)
    public void testAiModeButtonUiConfig_AvailableBeforeInitialize() {
        testAiModeButtonUiConfigImpl(
                /* isGoogle= */ true, /* isConfigAvailableBeforeInitialize= */ true);
    }

    /**
     * Verifies that a new NTP renders the AI Mode button from the config of a third party default
     * search engine which is already available before the coordinator is initialized.
     */
    @Test
    @EnableFeatures(OmniboxFeatureList.AIM3P_ENTRYPOINT)
    public void testAiModeButtonUiConfig_ThirdPartyAvailableBeforeInitialize() {
        testAiModeButtonUiConfigImpl(
                /* isGoogle= */ false, /* isConfigAvailableBeforeInitialize= */ true);
    }

    /**
     * Verifies that the AI Mode button is shown once the default search engine's {@link
     * AiModeButtonUiConfig} becomes available after the coordinator has been initialized.
     */
    @Test
    @EnableFeatures(OmniboxFeatureList.AIM3P_ENTRYPOINT)
    public void testAiModeButtonUiConfig_AvailableAfterInitialize() {
        testAiModeButtonUiConfigImpl(
                /* isGoogle= */ true, /* isConfigAvailableBeforeInitialize= */ false);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.AIM3P_ENTRYPOINT)
    public void testDestroy_RemovesAiModeButtonUiConfigObserver() {
        assertTrue(mAiModeButtonUiConfigSupplier.hasObservers());

        mCoordinator.destroy();

        assertFalse(mAiModeButtonUiConfigSupplier.hasObservers());

        // Recreates the coordinator since #tearDown() destroys it.
        createCoordinator();
    }

    /**
     * Verifies that the AI Mode button stays visible when switching from Google to a third party
     * search engine which offers an AI Mode entry point.
     */
    @Test
    @EnableFeatures(OmniboxFeatureList.AIM3P_ENTRYPOINT)
    public void testSwitchSearchEngine_GoogleToThirdPartyWithConfig() {
        testSwitchSearchEngineImpl(
                /* initialIsGoogle= */ true,
                /* hasInitialConfig= */ true,
                /* targetIsGoogle= */ false,
                /* hasTargetConfig= */ true);
    }

    /**
     * Verifies that the AI Mode button is hidden when switching from Google to a third party search
     * engine which doesn't offer an AI Mode entry point.
     */
    @Test
    @EnableFeatures(OmniboxFeatureList.AIM3P_ENTRYPOINT)
    public void testSwitchSearchEngine_GoogleToThirdPartyWithoutConfig() {
        testSwitchSearchEngineImpl(
                /* initialIsGoogle= */ true,
                /* hasInitialConfig= */ true,
                /* targetIsGoogle= */ false,
                /* hasTargetConfig= */ false);
    }

    /**
     * Verifies that the AI Mode button becomes visible when switching from a third party search
     * engine without an AI Mode entry point back to Google.
     */
    @Test
    @EnableFeatures(OmniboxFeatureList.AIM3P_ENTRYPOINT)
    public void testSwitchSearchEngine_ThirdPartyWithoutConfigToGoogle() {
        testSwitchSearchEngineImpl(
                /* initialIsGoogle= */ false,
                /* hasInitialConfig= */ false,
                /* targetIsGoogle= */ true,
                /* hasTargetConfig= */ true);
    }

    /**
     * Verifies that the AI Mode button stays visible when switching from a third party search
     * engine which offers an AI Mode entry point back to Google.
     */
    @Test
    @EnableFeatures(OmniboxFeatureList.AIM3P_ENTRYPOINT)
    public void testSwitchSearchEngine_ThirdPartyWithConfigToGoogle() {
        testSwitchSearchEngineImpl(
                /* initialIsGoogle= */ false,
                /* hasInitialConfig= */ true,
                /* targetIsGoogle= */ true,
                /* hasTargetConfig= */ true);
    }

    /**
     * Verifies that the optional button is shown on an existing NTP when switching back to Google
     * from a third party search engine without an AI Mode entry point, even though Google's
     * AiModeButtonUiConfig arrives after the search provider info is updated.
     */
    @Test
    @EnableFeatures({
        OmniboxFeatureList.AIM3P_ENTRYPOINT,
        ChromeFeatureList.NTP_AURORA,
        ChromeFeatureList.NTP_AURORA_V2 + ":action_chips/2"
    })
    public void testOptionalButtonVisibility_SwitchBackToGoogle_CreateImage() {
        testOptionalButtonVisibilitySwitchBackToGoogleImpl();
    }

    @Test
    @EnableFeatures({
        OmniboxFeatureList.AIM3P_ENTRYPOINT,
        ChromeFeatureList.NTP_AURORA,
        ChromeFeatureList.NTP_AURORA_V2 + ":action_chips/3"
    })
    public void testOptionalButtonVisibility_SwitchBackToGoogle_Canvas() {
        testOptionalButtonVisibilitySwitchBackToGoogleImpl();
    }

    /**
     * Verifies that the Create image chip falls back to the canvas chip when image generation isn't
     * allowed.
     */
    @Test
    @EnableFeatures({
        ChromeFeatureList.NTP_AURORA,
        ChromeFeatureList.NTP_AURORA_V2 + ":action_chips/2"
    })
    public void testOptionalButton_CreateImage_ImageGenNotAllowed_FallsBackToCanvas() {
        mInputStateSupplier.set(new InputStateBuilder().withAllowedTools().build());
        // Tool availability is checked when the composeplate is created, so recreate the
        // coordinator.
        mCoordinator.destroy();
        clearInvocations(mComposeboxQueryControllerBridge);
        createCoordinator();

        // The session used to determine the allowed tools is closed right away, and only one is
        // created even though the chip type is queried multiple times during initialization.
        verify(mComposeboxQueryControllerBridge).destroy();
        verifyOptionalButtonFellBackToCanvas();
    }

    /**
     * Verifies that the Create image chip falls back to the canvas chip when no Composebox session
     * can be created.
     */
    @Test
    @EnableFeatures({
        ChromeFeatureList.NTP_AURORA,
        ChromeFeatureList.NTP_AURORA_V2 + ":action_chips/2"
    })
    public void testOptionalButton_CreateImage_NoComposeboxSession_FallsBackToCanvas() {
        ComposeboxQueryControllerBridge.setInstanceForTesting(null);
        // Tool availability is checked when the composeplate is created, so recreate the
        // coordinator.
        mCoordinator.destroy();
        createCoordinator();

        verifyOptionalButtonFellBackToCanvas();
    }

    private void verifyOptionalButtonFellBackToCanvas() {
        assertNotNull(mCoordinator.getComposeplateCoordinatorForTesting());
        View optionalButton = mNewTabPageLayout.findViewById(R.id.optional_button);
        assertEquals(View.VISIBLE, optionalButton.getVisibility());

        // The Create image strings aren't applied, so the layout's default strings are kept.
        assertEquals(
                mActivity.getString(R.string.ntp_action_chip_add_image_accessibility),
                optionalButton.getContentDescription());
        TextView optionalButtonText = mNewTabPageLayout.findViewById(R.id.optional_button_text);
        assertEquals(mActivity.getString(R.string.add), optionalButtonText.getText().toString());

        // Clicking doesn't enter image generation mode.
        optionalButton.performClick();
        verify(mManager, never())
                .focusSearchBox(
                        anyBoolean(),
                        eq(AutocompleteRequestType.IMAGE_GENERATION),
                        eq(OmniboxFocusReason.NTP_AI_MODE),
                        any());
    }

    /**
     * Verifies that clicking the AI Mode button focuses the search box in AI Mode when an action
     * chip is enabled.
     */
    @Test
    @EnableFeatures({
        ChromeFeatureList.NTP_AURORA,
        ChromeFeatureList.NTP_AURORA_V2 + ":action_chips/1"
    })
    public void testComposeplateButtonClicked_ActionChipsEnabled() {
        testComposeplateButtonClickedImpl(/* expectedFocusSearchBox= */ true);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.NTP_AURORA_V2})
    public void testComposeplateButtonClicked_ActionChipsDisabled() {
        testComposeplateButtonClickedImpl(/* expectedFocusSearchBox= */ false);
    }

    /**
     * Verifies that on devices that don't support the fusebox, the AI Mode button remains visible
     * and clicking it navigates to the AI Mode SRP rather than focusing the AI Mode omnibox.
     */
    @Test
    @EnableFeatures({
        ChromeFeatureList.NTP_AURORA,
        ChromeFeatureList.NTP_AURORA_V2 + ":action_chips/1"
    })
    public void testComposeplateButtonClicked_NonFuseboxDevice_NavigatesToSrp() {
        DeviceInfo.setIsAutomotiveForTesting(true);
        when(mTemplateUrlService.getComposeplateUrl()).thenReturn(JUnitTestGURLs.URL_1);

        View composeplateButton = mNewTabPageLayout.findViewById(R.id.composeplate_button);
        assertEquals(View.VISIBLE, composeplateButton.getVisibility());

        composeplateButton.performClick();

        verify(mManager, never())
                .focusSearchBox(anyBoolean(), eq(AutocompleteRequestType.AI_MODE), anyInt(), any());
        verify(mManager).loadUrl(any(LoadUrlParams.class), eq(false));
    }

    /**
     * Verifies that the Create image chip falls back to the default chip on devices that don't
     * support the fusebox.
     */
    @Test
    @EnableFeatures({
        ChromeFeatureList.NTP_AURORA,
        ChromeFeatureList.NTP_AURORA_V2 + ":action_chips/2"
    })
    public void testOptionalButton_CreateImage_NonFuseboxDevice_FallsBackToDefault() {
        testOptionalButtonNonFuseboxDeviceImpl();
    }

    /**
     * Verifies that the gallery chip falls back to the default chip on devices that don't support
     * the fusebox.
     */
    @Test
    @EnableFeatures({
        ChromeFeatureList.NTP_AURORA,
        ChromeFeatureList.NTP_AURORA_V2 + ":action_chips/3"
    })
    public void testOptionalButton_Gallery_NonFuseboxDevice_FallsBackToDefault() {
        testOptionalButtonNonFuseboxDeviceImpl();
    }

    private void testOptionalButtonNonFuseboxDeviceImpl() {
        // The chip type is resolved when the composeplate is created, so recreate the coordinator.
        mCoordinator.destroy();
        DeviceInfo.setIsAutomotiveForTesting(true);
        createCoordinator();

        assertNotNull(mCoordinator.getComposeplateCoordinatorForTesting());
        View optionalButton = mNewTabPageLayout.findViewById(R.id.optional_button);
        assertEquals(View.GONE, optionalButton.getVisibility());
    }

    /**
     * Verifies that clicking the Create image chip focuses the search box in image generation mode.
     */
    @Test
    @EnableFeatures({
        ChromeFeatureList.NTP_AURORA,
        ChromeFeatureList.NTP_AURORA_V2 + ":action_chips/2"
    })
    public void testCreateImageButtonClicked() {
        // The composeplate is created in setUp() since the default search engine is Google.
        assertNotNull(mCoordinator.getComposeplateCoordinatorForTesting());
        View optionalButton = mNewTabPageLayout.findViewById(R.id.optional_button);

        optionalButton.performClick();

        verify(mManager)
                .focusSearchBox(
                        /* beginVoiceSearch= */ false,
                        AutocompleteRequestType.IMAGE_GENERATION,
                        OmniboxFocusReason.NTP_AI_MODE,
                        /* pastedText= */ null);
    }

    /** Verifies that clicking the gallery chip focuses the search box to open the gallery. */
    @Test
    @EnableFeatures({
        ChromeFeatureList.NTP_AURORA,
        ChromeFeatureList.NTP_AURORA_V2 + ":action_chips/3"
    })
    public void testGalleryButtonClicked_focusesSearchBoxForGallery() {
        // The composeplate is created in setUp() since the default search engine is Google.
        assertNotNull(mCoordinator.getComposeplateCoordinatorForTesting());
        View optionalButton = mNewTabPageLayout.findViewById(R.id.optional_button);

        optionalButton.performClick();

        verify(mManager)
                .focusSearchBox(
                        /* beginVoiceSearch= */ false,
                        AutocompleteRequestType.AI_MODE,
                        OmniboxFocusReason.NTP_GALLERY_CHIP_CLICKED,
                        /* pastedText= */ null);
    }

    @Test
    public void testGetCreateImageChipIconRes_usesImageGenToolConfigIcon() {
        InputState inputState =
                new InputStateBuilder()
                        .withToolConfigs(
                                ToolConfig.newBuilder()
                                        .setToolValue(ToolMode.TOOL_MODE_DEEP_SEARCH)
                                        .setIcon(
                                                Icon.newBuilder()
                                                        .setIconId(IconResourceIds.TRAVEL_EXPLORE))
                                        .build(),
                                ToolConfig.newBuilder()
                                        .setToolValue(ToolMode.TOOL_MODE_IMAGE_GEN)
                                        .setIcon(
                                                Icon.newBuilder().setIconId(IconResourceIds.BANANA))
                                        .build())
                        .build();
        assertEquals(
                R.drawable.create_image_24dp,
                NewTabPageCoordinator.getCreateImageChipIconRes(inputState));
    }

    @Test
    public void testGetCreateImageChipIconRes_fallsBackToDefault() {
        assertEquals(
                R.drawable.image_create_24dp,
                NewTabPageCoordinator.getCreateImageChipIconRes(null));
        // No icon.
        assertEquals(
                R.drawable.image_create_24dp,
                NewTabPageCoordinator.getCreateImageChipIconRes(
                        new InputStateBuilder()
                                .withToolConfigs(
                                        ToolConfig.newBuilder()
                                                .setToolValue(ToolMode.TOOL_MODE_IMAGE_GEN)
                                                .build())
                                .build()));
        // Icon without a client drawable.
        assertEquals(
                R.drawable.image_create_24dp,
                NewTabPageCoordinator.getCreateImageChipIconRes(
                        new InputStateBuilder()
                                .withToolConfigs(
                                        ToolConfig.newBuilder()
                                                .setToolValue(ToolMode.TOOL_MODE_IMAGE_GEN)
                                                .setIcon(
                                                        Icon.newBuilder()
                                                                .setIconId(IconResourceIds.SOCCER))
                                                .build())
                                .build()));
    }

    @Test
    public void testGetGalleryChipIconRes_usesGallerySourcedLensImageConfigIcon() {
        InputState inputState =
                new InputStateBuilder()
                        .withInputTypeConfigs(
                                InputTypeConfig.newBuilder()
                                        .setInputType(InputType.INPUT_TYPE_LENS_IMAGE)
                                        .setInputSource(InputSource.INPUT_SOURCE_CAMERA)
                                        .setIcon(
                                                Icon.newBuilder()
                                                        .setIconId(IconResourceIds.TRAVEL_EXPLORE))
                                        .build(),
                                InputTypeConfig.newBuilder()
                                        .setInputType(InputType.INPUT_TYPE_LENS_IMAGE)
                                        .setInputSource(InputSource.INPUT_SOURCE_GALLERY)
                                        .setIcon(Icon.newBuilder().setIconId(IconResourceIds.BOLT))
                                        .build())
                        .build();
        assertEquals(R.drawable.bolt_24dp, NewTabPageCoordinator.getGalleryChipIconRes(inputState));
    }

    @Test
    public void testGetGalleryChipIconRes_fallsBackToDefault() {
        assertEquals(
                R.drawable.add_photo_alternate_24dp,
                NewTabPageCoordinator.getGalleryChipIconRes(null));
        // Only a camera-sourced config.
        assertEquals(
                R.drawable.add_photo_alternate_24dp,
                NewTabPageCoordinator.getGalleryChipIconRes(
                        new InputStateBuilder()
                                .withInputTypeConfigs(
                                        InputTypeConfig.newBuilder()
                                                .setInputType(InputType.INPUT_TYPE_LENS_IMAGE)
                                                .setInputSource(InputSource.INPUT_SOURCE_CAMERA)
                                                .setIcon(
                                                        Icon.newBuilder()
                                                                .setIconId(IconResourceIds.BOLT))
                                                .build())
                                .build()));
    }

    @Test
    public void testComposeplateDisabledWhenIncognitoDisabled() {
        // Incognito availability is read when the coordinator is created, so recreate it after
        // disabling incognito mode.
        mCoordinator.destroy();
        IncognitoUtils.setEnabledForTesting(false);
        createCoordinator();

        verifyComposeplateDisabledAndUninflatedBetweenDseChanges();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.NTP_AURORA_V2)
    public void testComposeplateEnabled_AuroraV2Disabled() {
        assertEquals(TriState.TRUE, mCoordinator.getIsComposeplateEnabledForTesting());
        assertNotNull(mCoordinator.getComposeplateCoordinatorForTesting());
        assertNotNull(mNewTabPageLayout.findViewById(R.id.composeplate_view));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.NTP_AURORA,
        ChromeFeatureList.NTP_AURORA_V2 + ":layout_type/3"
    })
    public void testComposeplateDisabled_RemoveEnabled() {
        verifyComposeplateDisabledAndUninflatedBetweenDseChanges();
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.NTP_AURORA,
        ChromeFeatureList.NTP_AURORA_V2 + ":layout_type/2"
    })
    public void testComposeplateDisabled_InsideMvtEnabled() {
        verifyComposeplateDisabledAndUninflatedBetweenDseChanges();
    }

    private void verifyComposeplateDisabledAndUninflatedBetweenDseChanges() {
        // Verifies that even when Google is the search provider and offers an AI Mode entry point,
        // the composeplate is prevented from being enabled or created.
        assertEquals(TriState.FALSE, mCoordinator.getIsComposeplateEnabledForTesting());
        assertNull(mCoordinator.getComposeplateCoordinatorForTesting());
        assertNull(mNewTabPageLayout.findViewById(R.id.composeplate_view));

        // Switching the default search engine doesn't enable or inflate the composeplate either.
        changeSearchEngine(/* isGoogle= */ false, /* hasAiModeButtonUiConfig= */ true);
        changeSearchEngine(/* isGoogle= */ true, /* hasAiModeButtonUiConfig= */ true);
        assertEquals(TriState.FALSE, mCoordinator.getIsComposeplateEnabledForTesting());
        assertNull(mCoordinator.getComposeplateCoordinatorForTesting());
        assertNull(mNewTabPageLayout.findViewById(R.id.composeplate_view));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.NTP_AURORA,
        ChromeFeatureList.NTP_AURORA_V2 + ":layout_type/1"
    })
    public void testComposeplateEnabledWhenIncognitoDisabled_BesideMvtEnabled() {
        mCoordinator.destroy();
        IncognitoUtils.setEnabledForTesting(/* enabled= */ false);
        createCoordinator();

        assertEquals(TriState.TRUE, mCoordinator.getIsComposeplateEnabledForTesting());
        assertNotNull(mCoordinator.getComposeplateCoordinatorForTesting());

        View containerView = mNewTabPageLayout.findViewById(R.id.composeplate_and_mvt_container);
        assertEquals(View.VISIBLE, containerView.getVisibility());

        View composeplateView = mNewTabPageLayout.findViewById(R.id.composeplate_view);
        assertEquals(View.VISIBLE, composeplateView.getVisibility());
        int expectedMarginEnd =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.composeplate_view_beside_mvt_margin_end);
        assertEquals(
                expectedMarginEnd,
                ((ViewGroup.MarginLayoutParams) composeplateView.getLayoutParams()).getMarginEnd());
        View aiModeButton = composeplateView.findViewById(R.id.composeplate_button);
        assertEquals(
                0, ((ViewGroup.MarginLayoutParams) aiModeButton.getLayoutParams()).getMarginEnd());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.NTP_AURORA,
        ChromeFeatureList.NTP_AURORA_V2 + ":layout_type/1"
    })
    public void testInitializeComposeplate_BesideMvt_AlignsPaddingWithMvt() {
        View mvTilesContainer = mNewTabPageLayout.findViewById(R.id.mv_tiles_container);
        View aiModeButton = mNewTabPageLayout.findViewById(R.id.composeplate_button);
        TextView buttonText = mNewTabPageLayout.findViewById(R.id.composeplate_button_text);

        int buttonMargin =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.composeplate_view_button_margin);
        assertEquals(buttonMargin + mvTilesContainer.getPaddingTop(), aiModeButton.getPaddingTop());
        assertEquals(mvTilesContainer.getPaddingBottom(), aiModeButton.getPaddingBottom());
        assertEquals(2, buttonText.getMaxLines());
    }

    /** Verifies that a monochrome resource icon is tinted like the other composeplate icons. */
    @Test
    @EnableFeatures(OmniboxFeatureList.AIM3P_ENTRYPOINT)
    public void testAiModeButtonIconChanged_ResourceIcon() {
        testAiModeButtonIconChangedImpl(
                new StatusIconResource(R.drawable.ic_search_24dp, Resources.ID_NULL),
                /* expectedShouldTint= */ true);
    }

    /** Verifies that a full color favicon isn't tinted, which would render it as a silhouette. */
    @Test
    @EnableFeatures(OmniboxFeatureList.AIM3P_ENTRYPOINT)
    public void testAiModeButtonIconChanged_BitmapIcon() {
        testAiModeButtonIconChangedImpl(
                createBitmapAiModeButtonIcon(), /* expectedShouldTint= */ false);
    }

    /**
     * Verifies that the icon is still updated while the AI Mode button is hidden. Otherwise, as the
     * supplier doesn't notify an unchanged icon, the button would show a stale icon once shown.
     */
    @Test
    @EnableFeatures(OmniboxFeatureList.AIM3P_ENTRYPOINT)
    public void testAiModeButtonIconChanged_WhileButtonHidden() {
        setupMockSubCoordinators();
        changeSearchEngine(/* isGoogle= */ false, /* hasAiModeButtonUiConfig= */ false);
        assertEquals(TriState.FALSE, mCoordinator.getIsComposeplateEnabledForTesting());
        clearInvocations(mMockComposeplate);

        mAiModeButtonIconSupplier.set(createBitmapAiModeButtonIcon());

        verify(mMockComposeplate).updateAiModeButtonIcon(any(), eq(false));
    }

    /**
     * Verifies that a newly created composeplate shows the current icon, since the icon observer is
     * notified before the composeplate exists.
     */
    @Test
    @EnableFeatures(OmniboxFeatureList.AIM3P_ENTRYPOINT)
    public void testAiModeButtonIcon_AppliedToNewComposeplate() {
        mCoordinator.destroy();
        Bitmap bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        mAiModeButtonIconSupplier.set(
                new StatusIconResource(JUnitTestGURLs.RED_1.getSpec(), bitmap, Resources.ID_NULL));

        createCoordinator();

        assertNotNull(mCoordinator.getComposeplateCoordinatorForTesting());
        ImageView iconView = mNewTabPageLayout.findViewById(R.id.composeplate_button_icon);
        assertEquals(bitmap, ((BitmapDrawable) iconView.getDrawable()).getBitmap());
        // A full color favicon isn't tinted, even after the composeplate background is applied.
        assertNull(iconView.getImageTintList());
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.AIM3P_ENTRYPOINT)
    public void testDestroy_RemovesAiModeButtonIconObserver() {
        assertTrue(mAiModeButtonIconSupplier.hasObservers());

        mCoordinator.destroy();

        assertFalse(mAiModeButtonIconSupplier.hasObservers());

        // Recreates the coordinator since #tearDown() destroys it.
        createCoordinator();
    }

    private void testAiModeButtonIconChangedImpl(
            StatusIconResource icon, boolean expectedShouldTint) {
        setupMockSubCoordinators();
        clearInvocations(mMockComposeplate);

        mAiModeButtonIconSupplier.set(icon);

        verify(mMockComposeplate).updateAiModeButtonIcon(any(), eq(expectedShouldTint));
    }

    /** Returns a full color icon, as fetched from a third party search engine's favicon URL. */
    private static StatusIconResource createBitmapAiModeButtonIcon() {
        return new StatusIconResource(
                JUnitTestGURLs.RED_1.getSpec(),
                Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888),
                Resources.ID_NULL);
    }

    private void testAiModeButtonUiConfigImpl(
            boolean isGoogle, boolean isConfigAvailableBeforeInitialize) {
        // Destroys the coordinator created in setUp() since the config must be supplied before
        // NewTabPageCoordinator#initialize() is called. This also detaches its observer, so the
        // supplier can be reset below.
        mCoordinator.destroy();
        AiModeButtonUiConfig config =
                isConfigAvailableBeforeInitialize ? createAiModeButtonUiConfig(isGoogle) : null;
        mAiModeButtonUiConfigSupplier.set(config);

        createCoordinator();

        if (!isConfigAvailableBeforeInitialize) {
            // Without a config, the AI Mode button isn't eligible to be shown yet.
            assertEquals(TriState.FALSE, mCoordinator.getIsComposeplateEnabledForTesting());
            assertNull(mCoordinator.getComposeplateCoordinatorForTesting());

            config = changeSearchEngine(isGoogle, /* hasAiModeButtonUiConfig= */ true);
        }

        assertEquals(TriState.TRUE, mCoordinator.getIsComposeplateEnabledForTesting());
        assertNotNull(mCoordinator.getComposeplateCoordinatorForTesting());
        // The newly created composeplate must render the config, rather than the layout's default.
        TextView buttonText = mNewTabPageLayout.findViewById(R.id.composeplate_button_text);
        assertEquals(config.text, buttonText.getText().toString());
    }

    private void testOptionalButtonVisibilitySwitchBackToGoogleImpl() {
        // The composeplate is created in setUp() since the default search engine is Google.
        assertNotNull(mCoordinator.getComposeplateCoordinatorForTesting());
        View optionalButton = mNewTabPageLayout.findViewById(R.id.optional_button);
        assertEquals(View.VISIBLE, optionalButton.getVisibility());

        // Third party search engine A offers an AI Mode entry point.
        changeSearchEngine(/* isGoogle= */ false, /* hasAiModeButtonUiConfig= */ true);
        assertEquals(View.GONE, optionalButton.getVisibility());

        // Third party search engine B doesn't offer an AI Mode entry point, so the composeplate is
        // hidden.
        changeSearchEngine(/* isGoogle= */ false, /* hasAiModeButtonUiConfig= */ false);
        assertEquals(TriState.FALSE, mCoordinator.getIsComposeplateEnabledForTesting());
        assertEquals(View.GONE, optionalButton.getVisibility());

        // Switches back to Google. The optional button must be shown, even though the composeplate
        // isn't enabled yet when the search provider info is updated.
        changeSearchEngine(/* isGoogle= */ true, /* hasAiModeButtonUiConfig= */ true);
        assertEquals(TriState.TRUE, mCoordinator.getIsComposeplateEnabledForTesting());
        assertEquals(View.VISIBLE, optionalButton.getVisibility());
    }

    private void testComposeplateButtonClickedImpl(boolean expectedFocusSearchBox) {
        // The composeplate is created in setUp() since the default search engine is Google.
        assertNotNull(mCoordinator.getComposeplateCoordinatorForTesting());
        View composeplateButton = mNewTabPageLayout.findViewById(R.id.composeplate_button);

        composeplateButton.performClick();

        if (expectedFocusSearchBox) {
            verify(mManager)
                    .focusSearchBox(
                            /* beginVoiceSearch= */ false,
                            AutocompleteRequestType.AI_MODE,
                            OmniboxFocusReason.NTP_AI_MODE,
                            /* pastedText= */ null);
        } else {
            verify(mManager, never())
                    .focusSearchBox(
                            anyBoolean(), eq(AutocompleteRequestType.AI_MODE), anyInt(), any());
        }
    }

    private void testSwitchSearchEngineImpl(
            boolean initialIsGoogle,
            boolean hasInitialConfig,
            boolean targetIsGoogle,
            boolean hasTargetConfig) {
        setupMockSubCoordinators();

        changeSearchEngine(initialIsGoogle, hasInitialConfig);
        assertEquals(
                hasInitialConfig ? TriState.TRUE : TriState.FALSE,
                mCoordinator.getIsComposeplateEnabledForTesting());

        clearInvocations(mMockComposeplate);

        AiModeButtonUiConfig targetConfig = changeSearchEngine(targetIsGoogle, hasTargetConfig);

        assertEquals(
                hasTargetConfig ? TriState.TRUE : TriState.FALSE,
                mCoordinator.getIsComposeplateEnabledForTesting());
        if (hasTargetConfig) {
            verify(mMockComposeplate).updateAiModeButtonUiConfig(eq(targetConfig));
        } else {
            verify(mMockComposeplate, never()).updateAiModeButtonUiConfig(any());
        }
        mCoordinator.updateActionButtonVisibility();
        verify(mMockComposeplate, atLeastOnce()).setVisibility(eq(hasTargetConfig), anyBoolean());
    }

    /**
     * Simulates a default search engine change: the search provider info is updated first, then
     * native pushes the {@link AiModeButtonUiConfig} of the new default search engine.
     *
     * @param isGoogle Whether the new default search engine is Google.
     * @param hasAiModeButtonUiConfig Whether the new default search engine offers an AI Mode entry
     *     point.
     * @return The config pushed to the supplier, or null if the engine doesn't offer an AI Mode
     *     entry point.
     */
    private AiModeButtonUiConfig changeSearchEngine(
            boolean isGoogle, boolean hasAiModeButtonUiConfig) {
        mCoordinator.setSearchProviderInfo(/* hasLogo= */ isGoogle, isGoogle);
        AiModeButtonUiConfig aiModeButtonUiConfig =
                hasAiModeButtonUiConfig ? createAiModeButtonUiConfig(isGoogle) : null;
        mAiModeButtonUiConfigSupplier.set(aiModeButtonUiConfig);
        return aiModeButtonUiConfig;
    }

    /**
     * Returns the {@link AiModeButtonUiConfig} of a search engine. Google's entry point is rendered
     * from built-in assets, so its favicon and navigation URLs are empty, while a third party
     * engine supplies both.
     *
     * @param isGoogle Whether the config belongs to Google.
     */
    private static AiModeButtonUiConfig createAiModeButtonUiConfig(boolean isGoogle) {
        return new AiModeButtonUiConfig(
                isGoogle ? "AI Mode" : "Red AI",
                isGoogle ? "Ask AI Mode in Google Search" : "Ask AI Mode",
                "AI Mode button",
                "Always show AI Mode",
                "Ask AI Mode",
                /* faviconUrl= */ isGoogle ? GURL.emptyGURL() : JUnitTestGURLs.RED_1,
                /* navigationUrl= */ isGoogle ? "" : THIRD_PARTY_AI_MODE_NAVIGATION_URL,
                /* navigationUrlEmpty= */ isGoogle ? GURL.emptyGURL() : JUnitTestGURLs.URL_2);
    }

    private void setupMockSubCoordinators() {
        when(mMockSearchBox.getView()).thenReturn(mSearchBoxView);
        mCoordinator.setSearchBoxCoordinatorForTesting(mMockSearchBox);
        mCoordinator.setLogoCoordinatorForTesting(mMockLogo);
        mCoordinator.setMostVisitedTilesCoordinatorForTesting(mMockTiles);
        mCoordinator.setComposeplateCoordinatorForTesting(mMockComposeplate);
    }

    private void verifySetSearchProviderInfo(
            boolean targetHasLogo,
            boolean targetIsGoogle,
            int expectedSearchBoxTopMargin,
            boolean expectedComposeplateVisible) {
        // To avoid early exit, calls setSearchProviderInfo once with a different pair of
        // (targetHasLogo, targetIsGoogle)
        mCoordinator.setSearchProviderInfo(!targetHasLogo, !targetIsGoogle);
        setupMockSubCoordinators();

        when(mManager.isVoiceSearchEnabled()).thenReturn(true);
        when(mMockSearchBox.isLensEnabled(anyInt())).thenReturn(false);

        clearInvocations(mMockSearchBox);
        clearInvocations(mMockLogo);
        clearInvocations(mMockTiles);
        clearInvocations(mMockComposeplate);

        mCoordinator.setSearchProviderInfo(targetHasLogo, targetIsGoogle);

        assertEquals(
                targetHasLogo,
                mCoordinator.getSearchProviderInfoDelegateForTesting().getSearchProviderHasLogo());
        assertEquals(
                targetIsGoogle,
                mCoordinator.getSearchProviderInfoDelegateForTesting().getSearchProviderIsGoogle());

        Resources resources = mActivity.getResources();
        int logoTopMargin = resources.getDimensionPixelSize(R.dimen.ntp_logo_margin_top);
        int expectedBottomMargin = NtpCustomizationUtils.getLogoViewBottomMarginPx(resources);
        int expectedStyle =
                mCoordinator.shouldApplyWhiteBackgroundOnSearchBox()
                        ? R.style.TextAppearance_FakeSearchBoxTextMediumDark
                        : R.style.TextAppearance_FakeSearchBoxTextMedium;

        verify(mMockSearchBox).setTopMargin(eq(expectedSearchBoxTopMargin));
        verify(mMockLogo).setTopMargin(eq(logoTopMargin));
        verify(mMockLogo).setBottomMargin(eq(expectedBottomMargin));
        verify(mMockTiles).updateTilesLayoutMargins(eq(targetHasLogo), eq(false));
        verify(mMockSearchBox).setSearchBoxTextAppearance(eq(expectedStyle));
        verify(mMockSearchBox).setVoiceSearchButtonVisibility(eq(true));
        verify(mMockSearchBox).setLensButtonVisibility(eq(false));
        verify(mMockComposeplate).setVisibility(eq(expectedComposeplateVisible), anyBoolean());
    }

    @Test
    public void testNtpScrollListener_hidesControls_whenNotLoading() {
        when(mTab.isLoading()).thenReturn(false);

        NtpScrollListener listener =
                new NtpScrollListener(mBrowserControlsVisibilityManager, mActivity, mTab);

        // Scroll past the threshold (20dp * 1.0 = 20px). Scroll down by 25px:
        listener.onScrolled(mRecyclerView, 0, 25);
        verify(mBrowserControlsVisibilityManager).hideAndroidControls(true);
    }

    @Test
    public void testNtpScrollListener_doesNotHideControls_whenLoading() {
        when(mTab.isLoading()).thenReturn(true);

        NtpScrollListener listener =
                new NtpScrollListener(mBrowserControlsVisibilityManager, mActivity, mTab);

        // Scroll past the threshold.
        listener.onScrolled(mRecyclerView, 0, 25);
        verify(mBrowserControlsVisibilityManager, never()).hideAndroidControls(true);
    }

    @Test
    public void testNtpScrollListener_showsControls_whenNotLoading() {
        when(mBrowserControlsVisibilityManager.getBottomControlHiddenRatio()).thenReturn(1.0f);
        when(mTab.isLoading()).thenReturn(false);

        NtpScrollListener listener =
                new NtpScrollListener(mBrowserControlsVisibilityManager, mActivity, mTab);

        // Scroll up past the threshold.
        listener.onScrolled(mRecyclerView, 0, -25);
        verify(mBrowserControlsVisibilityManager).showAndroidControls(true);
    }

    @Test
    public void testNtpScrollListener_doesNotShowControls_whenLoading() {
        when(mBrowserControlsVisibilityManager.getBottomControlHiddenRatio()).thenReturn(1.0f);
        when(mTab.isLoading()).thenReturn(true);

        NtpScrollListener listener =
                new NtpScrollListener(mBrowserControlsVisibilityManager, mActivity, mTab);

        // Scroll up past the threshold.
        listener.onScrolled(mRecyclerView, 0, -25);
        verify(mBrowserControlsVisibilityManager, never()).showAndroidControls(true);
    }

    @Test
    public void testNtpScrollListener_doesNotHideControls_whenConstraintsShown() {
        when(mTab.isLoading()).thenReturn(false);
        mVisibilityDelegate.showControlsPersistent();

        assertEquals(BrowserControlsState.SHOWN, (int) mVisibilityDelegate.get());

        NtpScrollListener listener =
                new NtpScrollListener(mBrowserControlsVisibilityManager, mActivity, mTab);

        // Scroll past the threshold.
        listener.onScrolled(mRecyclerView, 0, 25);
        verify(mBrowserControlsVisibilityManager, never()).hideAndroidControls(true);
    }

    @Test
    @Features.EnableFeatures({
        ChromeFeatureList.NTP_AURORA,
        ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2
    })
    public void testOnCustomizedBackgroundChanged_composeplateFlagNotInitialized_earlyExit() {
        setupMockSubCoordinators();
        mCoordinator.setIsComposeplateEnabledForTesting(TriState.NOT_SET);
        setupDiskImageBackground();
        mCoordinator.setIsWhiteBackgroundOnComposeplateApplied(TriState.NOT_SET);
        mCoordinator.setIsWhiteBackgroundOnSearchBoxApplied(TriState.NOT_SET);

        assertTrue(mCoordinator.shouldApplyWhiteBackgroundOnSearchBox());
        assertTrue(NtpCustomizationUtils.shouldApplyWhiteBackgroundOnComposeplate());

        mCoordinator.onCustomizedBackgroundChanged();

        verify(mMockSearchBox, never()).applyWhiteBackground(anyBoolean());
        verify(mMockComposeplate, never()).applyWhiteBackground(anyBoolean());
    }

    @Test
    @Features.EnableFeatures({
        ChromeFeatureList.NTP_AURORA,
        ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2
    })
    public void testOnCustomizedBackgroundChanged_searchBoxNotInitialized_earlyExit() {
        setupMockSubCoordinators();
        mCoordinator.setIsComposeplateEnabledForTesting(TriState.TRUE);
        mCoordinator.setSearchBoxCoordinatorForTesting(null);
        setupDiskImageBackground();
        mCoordinator.setIsWhiteBackgroundOnComposeplateApplied(TriState.NOT_SET);
        mCoordinator.setIsWhiteBackgroundOnSearchBoxApplied(TriState.NOT_SET);

        assertTrue(mCoordinator.shouldApplyWhiteBackgroundOnSearchBox());
        assertTrue(NtpCustomizationUtils.shouldApplyWhiteBackgroundOnComposeplate());

        mCoordinator.onCustomizedBackgroundChanged();

        verify(mMockSearchBox, never()).applyWhiteBackground(anyBoolean());
        verify(mMockComposeplate, never()).applyWhiteBackground(anyBoolean());
    }

    @Test
    public void
            testOnCustomizedBackgroundChanged_composeplateCoordinatorNotInitialized_earlyExit() {
        setupMockSubCoordinators();
        mCoordinator.setIsComposeplateEnabledForTesting(TriState.TRUE);
        mCoordinator.setComposeplateCoordinatorForTesting(null);

        mCoordinator.onCustomizedBackgroundChanged();

        verify(mMockComposeplate, never()).applyWhiteBackground(anyBoolean());
    }

    @Test
    public void testRecordNtpShown_recordsComposeplateButtonsImpression() {
        setupMockSubCoordinators();

        mCoordinator.recordNtpShown();

        verify(mMockComposeplate).recordButtonsImpression();
    }

    @Test
    public void testRecordNtpShown_composeplateCoordinatorNotInitialized() {
        setupMockSubCoordinators();
        mCoordinator.setComposeplateCoordinatorForTesting(null);

        mCoordinator.recordNtpShown();

        verify(mMockComposeplate, never()).recordButtonsImpression();
    }

    @Test
    @Features.DisableFeatures({
        ChromeFeatureList.NTP_AURORA,
        ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2
    })
    public void
            testOnCustomizedBackgroundChanged_searchBox_uninitializedAndShouldNotApply_doesNotApplyBackground() {
        setupMockSubCoordinators();
        mCoordinator.setIsComposeplateEnabledForTesting(TriState.TRUE);

        // If shouldn't apply a white background and the background hasn't been updated before, the
        // background is not applied.
        mCoordinator.setIsWhiteBackgroundOnSearchBoxApplied(TriState.NOT_SET);
        assertFalse(mCoordinator.shouldApplyWhiteBackgroundOnSearchBox());

        mCoordinator.onCustomizedBackgroundChanged();

        verify(mMockSearchBox, never()).applyWhiteBackground(anyBoolean());

        // If white background is disabled and the member variable was already set to false, verify
        // applyWhiteBackground
        // is not called again on the search box.
        clearInvocations(mMockSearchBox);
        mCoordinator.setIsWhiteBackgroundOnSearchBoxApplied(TriState.FALSE);

        mCoordinator.onCustomizedBackgroundChanged();

        verify(mMockSearchBox, never()).applyWhiteBackground(anyBoolean());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void
            testOnCustomizedBackgroundChanged_searchBox_alreadyAppliedAndShouldApply_doesNotReapplyBackground() {
        setupMockSubCoordinators();
        mCoordinator.setIsComposeplateEnabledForTesting(TriState.TRUE);
        setupDiskImageBackground();

        // If white background is enabled and was already set to true,verify applyWhiteBackground is
        // not called again on the search box.
        assertTrue(mCoordinator.shouldApplyWhiteBackgroundOnSearchBox());
        mCoordinator.setIsWhiteBackgroundOnSearchBoxApplied(TriState.TRUE);

        mCoordinator.onCustomizedBackgroundChanged();

        verify(mMockSearchBox, never()).applyWhiteBackground(anyBoolean());
    }

    @Test
    @Features.DisableFeatures({
        ChromeFeatureList.NTP_AURORA,
        ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2
    })
    public void
            testOnCustomizedBackgroundChanged_composeplate_uninitializedAndShouldNotApply_doesNotApplyBackground() {
        setupMockSubCoordinators();
        mCoordinator.setIsComposeplateEnabledForTesting(TriState.TRUE);

        // If shouldn't apply a white background and the background hasn't been updated before, the
        // background is not applied.
        mCoordinator.setIsWhiteBackgroundOnComposeplateApplied(TriState.NOT_SET);
        assertFalse(NtpCustomizationUtils.shouldApplyWhiteBackgroundOnComposeplate());

        mCoordinator.onCustomizedBackgroundChanged();

        verify(mMockComposeplate, never()).applyWhiteBackground(anyBoolean());

        // If white background is disabled and the member variable was already set to false, verify
        // applyWhiteBackground is not called again on the composeplate.
        mCoordinator.setIsWhiteBackgroundOnComposeplateApplied(TriState.FALSE);
        assertFalse(NtpCustomizationUtils.shouldApplyWhiteBackgroundOnComposeplate());

        mCoordinator.onCustomizedBackgroundChanged();

        verify(mMockComposeplate, never()).applyWhiteBackground(anyBoolean());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void
            testOnCustomizedBackgroundChanged_composeplate_alreadyAppliedAndShouldApply_doesNotReapplyBackground() {
        setupMockSubCoordinators();
        mCoordinator.setIsComposeplateEnabledForTesting(TriState.TRUE);
        setupDiskImageBackground();

        // If the background has been updated to true before and it should remain true,
        // no additional invocation of applyWhiteBackground on the composeplate.
        clearInvocations(mMockComposeplate);
        assertTrue(NtpCustomizationUtils.shouldApplyWhiteBackgroundOnComposeplate());
        mCoordinator.setIsWhiteBackgroundOnComposeplateApplied(TriState.TRUE);

        mCoordinator.onCustomizedBackgroundChanged();

        verify(mMockComposeplate, never()).applyWhiteBackground(anyBoolean());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void
            testOnCustomizedBackgroundChanged_searchBox_previouslyFalseAndShouldApply_appliesWhiteBackground() {
        setupMockSubCoordinators();
        mCoordinator.setIsComposeplateEnabledForTesting(TriState.TRUE);
        setupDiskImageBackground();

        // Applies the white background if previously null.
        assertTrue(mCoordinator.shouldApplyWhiteBackgroundOnSearchBox());
        mCoordinator.setIsWhiteBackgroundOnSearchBoxApplied(TriState.NOT_SET);

        mCoordinator.onCustomizedBackgroundChanged();

        verify(mMockSearchBox).applyWhiteBackground(true);

        // Applies the white background if previously false.
        clearInvocations(mMockSearchBox);
        mCoordinator.setIsWhiteBackgroundOnSearchBoxApplied(TriState.FALSE);

        mCoordinator.onCustomizedBackgroundChanged();

        verify(mMockSearchBox).applyWhiteBackground(true);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.NTP_AURORA)
    public void
            testOnCustomizedBackgroundChanged_searchBox_auroraEnabled_initialLaunchAppliesBackground() {
        setupMockSubCoordinators();
        mCoordinator.setIsComposeplateEnabledForTesting(TriState.TRUE);

        // On initial launch with default theme (desiredState = false), currentState is null.
        assertFalse(mCoordinator.shouldApplyWhiteBackgroundOnSearchBox());
        mCoordinator.setIsWhiteBackgroundOnSearchBoxApplied(TriState.NOT_SET);

        mCoordinator.onCustomizedBackgroundChanged();

        // Aurora forces the initial update on launch.
        verify(mMockSearchBox).applyWhiteBackground(false);

        // If called again with false, it should not re-trigger since currentState is now false.
        clearInvocations(mMockSearchBox);
        mCoordinator.onCustomizedBackgroundChanged();
        verify(mMockSearchBox, never()).applyWhiteBackground(anyBoolean());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void
            testOnCustomizedBackgroundChanged_composeplate_previouslyFalseAndShouldApply_appliesWhiteBackground() {
        setupMockSubCoordinators();
        mCoordinator.setIsComposeplateEnabledForTesting(TriState.TRUE);
        setupDiskImageBackground();

        // Applies the white background if previously null.
        assertTrue(NtpCustomizationUtils.shouldApplyWhiteBackgroundOnComposeplate());
        mCoordinator.setIsWhiteBackgroundOnComposeplateApplied(TriState.NOT_SET);

        mCoordinator.onCustomizedBackgroundChanged();

        verify(mMockComposeplate).applyWhiteBackground(true);

        // Applies the white background if previously false.
        clearInvocations(mMockComposeplate);
        mCoordinator.setIsWhiteBackgroundOnComposeplateApplied(TriState.FALSE);

        mCoordinator.onCustomizedBackgroundChanged();

        verify(mMockComposeplate).applyWhiteBackground(true);
    }

    @Test
    @Features.DisableFeatures({
        ChromeFeatureList.NTP_AURORA,
        ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2
    })
    public void
            testOnCustomizedBackgroundChanged_searchBox_alreadyAppliedAndShouldNotApply_removesWhiteBackground() {
        setupMockSubCoordinators();
        mCoordinator.setIsComposeplateEnabledForTesting(TriState.TRUE);

        // Removes white background if previously true.
        assertFalse(mCoordinator.shouldApplyWhiteBackgroundOnSearchBox());
        mCoordinator.setIsWhiteBackgroundOnSearchBoxApplied(TriState.TRUE);

        mCoordinator.onCustomizedBackgroundChanged();

        verify(mMockSearchBox).applyWhiteBackground(false);
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void
            testOnCustomizedBackgroundChanged_composeplate_alreadyAppliedAndShouldNotApply_removesWhiteBackground() {
        setupMockSubCoordinators();
        mCoordinator.setIsComposeplateEnabledForTesting(TriState.TRUE);

        // Removes white background if previously true.
        assertFalse(mCoordinator.shouldApplyWhiteBackgroundOnSearchBox());
        mCoordinator.setIsWhiteBackgroundOnComposeplateApplied(TriState.TRUE);

        mCoordinator.onCustomizedBackgroundChanged();

        verify(mMockComposeplate).applyWhiteBackground(false);
    }

    private void setupDiskImageBackground() {
        NtpCustomizationConfigManager configManager = new NtpCustomizationConfigManager();
        NtpCustomizationConfigManager.setInstanceForTesting(configManager);
        configManager.setBackgroundTypeForTesting(
                NtpCustomizationUtils.NtpBackgroundType.IMAGE_FROM_DISK);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.NTP_AURORA + ":padding_style/0"})
    public void testOnDisplayStyleChanged_Phone_NonDefault() {
        FeatureOverrides.overrideParam(
                ChromeFeatureList.NTP_AURORA, "padding_style", NewTabPageUtils.PaddingStyle.MEDIUM);
        createCoordinator(/* isLff= */ false);
        verify(mUiConfig).addObserver(mDisplayStyleObserverCaptor.capture());
        setupMockSubCoordinators();
        verify(mMockLogo, never()).setTopMargin(anyInt());

        mDisplayStyleObserverCaptor.getValue().onDisplayStyleChanged(null);

        Resources resources = mActivity.getResources();
        int expectedTopMargin = LogoUtils.getTopMarginForLogo(resources);
        verify(mMockLogo).setTopMargin(eq(expectedTopMargin));
        verify(mMockComposeplate).onDisplayStyleChanged();
    }

    @Test
    @EnableFeatures({ChromeFeatureList.NTP_AURORA + ":padding_style/0"})
    public void testOnDisplayStyleChanged_Phone_Default() {
        FeatureOverrides.overrideParam(
                ChromeFeatureList.NTP_AURORA,
                "padding_style",
                NewTabPageUtils.PaddingStyle.DEFAULT);
        createCoordinator(/* isLff= */ false);
        verify(mUiConfig, never()).addObserver(any());
    }

    @Test
    @Config(qualifiers = "land")
    @EnableFeatures({ChromeFeatureList.NTP_AURORA + ":padding_style/0"})
    public void testOnDisplayStyleChanged_Phone_Landscape_NonDefault() {
        FeatureOverrides.overrideParam(
                ChromeFeatureList.NTP_AURORA, "padding_style", NewTabPageUtils.PaddingStyle.LARGE);
        createCoordinator(/* isLff= */ false);
        verify(mUiConfig).addObserver(mDisplayStyleObserverCaptor.capture());
        setupMockSubCoordinators();
        verify(mMockLogo, never()).setTopMargin(anyInt());

        mDisplayStyleObserverCaptor.getValue().onDisplayStyleChanged(null);

        Resources resources = mActivity.getResources();
        int expectedTopMargin = LogoUtils.getTopMarginForLogo(resources);
        verify(mMockLogo).setTopMargin(eq(expectedTopMargin));
        verify(mMockComposeplate).onDisplayStyleChanged();
    }

    @Test
    // Tests for middle window size, i.e., between 420dp and 840dp.
    @Config(qualifiers = "w700dp")
    @EnableFeatures({ChromeFeatureList.NTP_AURORA + ":padding_style/0"})
    public void testOnDisplayStyleChanged_Tablet_WideToRegularWindow() {
        testOnDisplayStyleChangedOnTabletImpl(
                HorizontalDisplayStyle.WIDE, HorizontalDisplayStyle.REGULAR);
    }

    @Test
    // Tests for middle window size, i.e., between 420dp and 840dp.
    @Config(qualifiers = "w700dp")
    @EnableFeatures({ChromeFeatureList.NTP_AURORA + ":padding_style/0"})
    public void testOnDisplayStyleChanged_Tablet_RegularToWideWindow() {
        testOnDisplayStyleChangedOnTabletImpl(
                HorizontalDisplayStyle.REGULAR, HorizontalDisplayStyle.WIDE);
    }

    /**
     * Verifies that resizing the window on tablets updates the search box margins.
     *
     * @param initialDisplayStyle The {@link HorizontalDisplayStyle} before the change.
     * @param newDisplayStyle The {@link HorizontalDisplayStyle} after the change.
     */
    private void testOnDisplayStyleChangedOnTabletImpl(
            @HorizontalDisplayStyle int initialDisplayStyle,
            @HorizontalDisplayStyle int newDisplayStyle) {
        setHorizontalDisplayStyle(initialDisplayStyle);
        createCoordinator(/* isLff= */ true);
        verify(mUiConfig).addObserver(mDisplayStyleObserverCaptor.capture());
        setupMockSubCoordinators();
        verify(mMockLogo, never()).updateDoodleOnTablet(anyBoolean());
        assertEquals(
                getExpectedSearchBoxTwoSideMarginOnTablet(initialDisplayStyle),
                mCoordinator.getSearchBoxTwoSideMarginForTesting());

        setHorizontalDisplayStyle(newDisplayStyle);
        notifyDisplayStyleObserver();

        verify(mMockLogo).updateDoodleOnTablet(anyBoolean());
        verify(mMockComposeplate, never()).onDisplayStyleChanged();
        assertEquals(
                getExpectedSearchBoxTwoSideMarginOnTablet(newDisplayStyle),
                mCoordinator.getSearchBoxTwoSideMarginForTesting());
    }

    /**
     * Returns the expected two side margins of the search box on tablets in a window narrower than
     * {@code R.dimen.ntp_search_box_expanded_window_min_width} (840dp), but still in state of
     * HorizontalDisplayStyle.WIDE. The side margin is 0 so that the search box matches the MVT
     * width, otherwise the narrow-window margins.
     *
     * @param horizontalDisplayStyle The {@link HorizontalDisplayStyle} of the window.
     */
    private int getExpectedSearchBoxTwoSideMarginOnTablet(
            @HorizontalDisplayStyle int horizontalDisplayStyle) {
        if (horizontalDisplayStyle == HorizontalDisplayStyle.WIDE) return 0;

        return 2
                * mActivity
                        .getResources()
                        .getDimensionPixelSize(
                                R.dimen.ntp_search_box_lateral_margin_narrow_window_tablet);
    }

    /**
     * Returns the expected two side margins of the search box on tablets in a window at least
     * {@code R.dimen.ntp_search_box_expanded_window_min_width} wide.
     */
    private int getExpandedWindowSearchBoxTwoSideMargin() {
        return 2
                * mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.ntp_search_box_lateral_margin_tablet);
    }

    private void setHorizontalDisplayStyle(@HorizontalDisplayStyle int horizontalDisplayStyle) {
        when(mUiConfig.getCurrentDisplayStyle())
                .thenReturn(
                        new UiConfig.DisplayStyle(
                                horizontalDisplayStyle, VerticalDisplayStyle.REGULAR));
    }

    /**
     * Notifies the latest registered {@link DisplayStyleObserver}, i.e., the one of the coordinator
     * created last, as {@link UiConfig} does on every configuration change, size change and
     * horizontal inset change. The display style argument is null, since the NTP reads the current
     * display style from {@link UiConfig}.
     */
    private void notifyDisplayStyleObserver() {
        verify(mUiConfig, atLeastOnce()).addObserver(mDisplayStyleObserverCaptor.capture());
        mDisplayStyleObserverCaptor.getValue().onDisplayStyleChanged(null);
    }

    @Test
    public void testSideUiObserver_liveResize_doesNotUpdateUiConfigInset() {
        SideUiObserver observer = captureSideUiObserver();

        observer.onSideUiSpecsChangeStarted(
                new SideUiSpecs(/* leftContainerWidth= */ 300, /* rightContainerWidth= */ 0),
                UiUpdateRequest.getRequestForTesting(
                        /* suppressAnimations= */ true, UpdateReason.RESIZE_LIVE));

        verify(mUiConfig, never()).setHorizontalInset(anyInt());
    }

    @Test
    public void testSideUiObserver_resizeCommitted_updatesUiConfigInset() {
        SideUiObserver observer = captureSideUiObserver();

        int leftWidthPx = 300;
        observer.onSideUiSpecsChangeStarted(
                new SideUiSpecs(leftWidthPx, /* rightContainerWidth= */ 0),
                UiUpdateRequest.getRequestForTesting(
                        /* suppressAnimations= */ true, UpdateReason.RESIZE_COMMITTED));

        float density = mActivity.getResources().getDisplayMetrics().density;
        verify(mUiConfig).setHorizontalInset(Math.round(leftWidthPx / density));
    }

    @Test
    public void testSearchBoxTextWatcher_ForwardsTextAndClearsSearchBox() {
        TextView searchBoxTextView = mNewTabPageLayout.findViewById(R.id.search_box_text);
        assertNotNull(searchBoxTextView);

        searchBoxTextView.setText("first paste");
        verify(mManager)
                .focusSearchBox(
                        false,
                        AutocompleteRequestType.SEARCH,
                        OmniboxFocusReason.FAKE_BOX_LONG_PRESS,
                        "first paste");
        assertEquals("", searchBoxTextView.getText().toString());

        clearInvocations(mManager);
        searchBoxTextView.setText("second paste");
        verify(mManager)
                .focusSearchBox(
                        false,
                        AutocompleteRequestType.SEARCH,
                        OmniboxFocusReason.FAKE_BOX_LONG_PRESS,
                        "second paste");
        assertEquals("", searchBoxTextView.getText().toString());
    }

    /**
     * Makes the {@link SideUiStateProvider} available to the coordinator created in {@link
     * #setUp()}, and returns the {@link SideUiObserver} it registers. Invocations on {@link
     * #mUiConfig} made during registration are cleared.
     */
    private SideUiObserver captureSideUiObserver() {
        when(mSideUiStateProvider.getCurrentSideUiSpecs())
                .thenReturn(
                        new SideUiSpecs(/* leftContainerWidth= */ 0, /* rightContainerWidth= */ 0));
        mSideUiStateProviderSupplier.set(mSideUiStateProvider);
        ShadowLooper.idleMainLooper();
        verify(mSideUiStateProvider).addObserver(mSideUiObserverCaptor.capture());
        clearInvocations(mUiConfig);
        return mSideUiObserverCaptor.getValue();
    }

    /**
     * Configures and returns a mock customization coordinator for testing the theme tip trigger.
     */
    private NtpCustomizationCoordinator setUpThemeTipTrigger() {
        mCoordinator.destroy();
        createCoordinator(/* isLff= */ false);
        NtpCustomizationTestHelper.setupEdgeToEdge(mWindowAndroid);
        ChromeFeatureList.sNewTabPageCustomizationV2ForceShowTipBottomSheet.setForTesting(true);
        ChromeFeatureList.sNewTabPageCustomizationV2ShowTipBottomSheet.setForTesting(true);
        NtpCustomizationPromoManager.setEnableForTesting(true);
        NtpCustomizationCoordinatorFactory factory = mock(NtpCustomizationCoordinatorFactory.class);
        NtpCustomizationCoordinatorFactory.setInstanceForTesting(factory);
        NtpCustomizationCoordinator customizationCoordinator =
                mock(NtpCustomizationCoordinator.class);
        when(factory.create(any(), any(), any(), anyInt(), any(), any(), any()))
                .thenReturn(customizationCoordinator);
        assertFalse(NtpCustomizationUtils.isThemeTipBottomSheetShownFromSharedPreference());
        return customizationCoordinator;
    }
}
