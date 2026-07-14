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
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.DeviceInfo;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.composeplate.ComposeplateCoordinator;
import org.chromium.chrome.browser.composeplate.ComposeplateUtils;
import org.chromium.chrome.browser.composeplate.ComposeplateUtilsJni;
import org.chromium.chrome.browser.feed.FeedSurfaceScrollDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.logo.LogoCoordinator;
import org.chromium.chrome.browser.magic_stack.ModuleRegistry;
import org.chromium.chrome.browser.ntp.search.SearchBoxCoordinator;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinatorFactory;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.omnibox.SearchEngineService;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.segmentation_platform.client_util.HomeModulesRankingHelper;
import org.chromium.chrome.browser.segmentation_platform.client_util.HomeModulesRankingHelperJni;
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
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.TouchEnabledDelegate;
import org.chromium.chrome.test.util.browser.offlinepages.FakeOfflinePageBridge;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
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
@Config(manifest = Config.NONE)
@EnableFeatures({
    ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER_V2,
    SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
    SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS
})
public class NewTabPageCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();
    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    @Mock private ComposeplateUtils.Natives mMockComposeplateUtilsJni;
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
    @Mock private Supplier<GURL> mComposeplateUrlSupplier;
    @Mock private SearchEngineService mSearchEngineService;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private IdentityManager mIdentityManager;
    @Mock private SigninManager mSigninManager;
    @Mock private SyncService mSyncService;
    @Mock private BackPressManager mBackPressManager;
    @Mock private SearchBoxCoordinator mMockSearchBox;
    @Mock private LogoCoordinator mMockLogo;
    @Mock private MostVisitedTilesCoordinator mMockTiles;
    @Mock private ComposeplateCoordinator mMockComposeplate;
    @Mock private View mMockSearchBoxView;
    @Mock private BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    @Mock private RecyclerView mRecyclerView;

    private Activity mActivity;
    private NewTabPageLayout mNewTabPageLayout;
    private NewTabPageCoordinator mCoordinator;
    private final OneshotSupplierImpl<ModuleRegistry> mModuleRegistrySupplier =
            new OneshotSupplierImpl<>();

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        // Setup for MV tiles.
        mSuggestionsDeps.getFactory().mostVisitedSites = new FakeMostVisitedSites();
        mSuggestionsDeps.getFactory().offlinePageBridge = new FakeOfflinePageBridge();

        // Setup for signin and sync.
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        IdentityServicesProvider.setIdentityManagerForTesting(mIdentityManager);
        IdentityServicesProvider.setSigninManagerForTesting(mSigninManager);

        // Setup for the composeplate buttons.
        ComposeplateUtilsJni.setInstanceForTesting(mMockComposeplateUtilsJni);
        when(mMockComposeplateUtilsJni.isAimEntrypointEligible(mProfile)).thenReturn(true);
        when(mMockComposeplateUtilsJni.isEnabledByPolicy(mProfile)).thenReturn(true);
        IncognitoUtils.setEnabledForTesting(true);

        // Setup for home modules.
        HomeModulesRankingHelperJni.setInstanceForTesting(mHomeModulesRankingHelperJniMock);
        mModuleRegistrySupplier.set(mModuleRegistry);

        // Setup for lens.
        WeakReference<Context> contextWeakReference = new WeakReference<>(mActivity);
        when(mWindowAndroid.getContext()).thenReturn(contextWeakReference);

        // Setup for search.
        SearchEngineService.setInstanceForTesting(mSearchEngineService);

        when(mMostRecentTab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mProfile.isOffTheRecord()).thenReturn(false);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);

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
                        /* isLff= */ false,
                        mTabStripHeightSupplier,
                        mHomeSurfaceTracker,
                        mBackPressManager);

        mCoordinator.initialize(
                mTileGroupDelegate,
                /* searchProviderHasLogo= */ true,
                /* searchProviderIsGoogle= */ true,
                mScrollDelegate,
                mTouchEnabledDelegate,
                mUiConfig,
                mLifecycleDispatcher,
                mComposeplateUrlSupplier);
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
        assertEquals(expectedBoundedWidth, composeplateView.getLayoutParams().width);

        View logoView = layout.findViewById(R.id.logo_container_view);
        assertNotNull(logoView);
        assertEquals(measureWidth, logoView.getLayoutParams().width);

        View mvtView = layout.findViewById(R.id.mv_tiles_container);
        assertNotNull(mvtView);
        assertEquals(expectedBoundedWidth, mvtView.getLayoutParams().width);
    }

    @Test
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
                NtpCustomizationUtils.shouldApplyWhiteBackgroundOnSearchBox()
                        ? R.style.TextAppearance_FakeSearchBoxTextMediumDark
                        : R.style.TextAppearance_FakeSearchBoxTextMedium;
        verify(mMockSearchBox).setSearchBoxTextAppearance(eq(expectedStyle));
    }

    @Test
    public void testUpdateActionButtonVisibility() {
        setupMockSubCoordinators();
        when(mManager.isVoiceSearchEnabled()).thenReturn(true);
        when(mMockSearchBox.isLensEnabled(anyInt())).thenReturn(false);
        mCoordinator.setIsComposeplateEnabledForTesting(true);
        mCoordinator.setSearchProviderInfo(/* hasLogo= */ true, /* isGoogle= */ true);
        clearInvocations(mMockSearchBox, mMockComposeplate);

        mCoordinator.updateActionButtonVisibility();

        verify(mMockSearchBox).setVoiceSearchButtonVisibility(eq(true));
        verify(mMockSearchBox).setLensButtonVisibility(eq(false));
        verify(mMockComposeplate).setVisibility(eq(true), anyBoolean());
    }

    @Test
    public void testInitializeComposeplate() {
        mCoordinator.destroy();

        when(mMockComposeplateUtilsJni.isAimEntrypointEligible(mProfile)).thenReturn(false);
        createCoordinator();
        assertNull(mCoordinator.getComposeplateCoordinatorForTesting());

        SearchBoxCoordinator mockSearchBox = mock(SearchBoxCoordinator.class);
        when(mockSearchBox.getView()).thenReturn(mock(View.class));
        mCoordinator.setSearchBoxCoordinatorForTesting(mockSearchBox);

        mCoordinator.initializeComposeplate();

        assertNotNull(mCoordinator.getComposeplateCoordinatorForTesting());
    }

    /**
     * Verifies when switching to a 3rd party search engine with no logo, expects fallback top
     * margin and hidden composeplate.
     */
    @Test
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
    public void testSetSearchProviderInfo_GoogleWithLogo() {
        verifySetSearchProviderInfo(
                /* targetHasLogo= */ true,
                /* targetIsGoogle= */ true,
                /* expectedSearchBoxTopMargin= */ 0,
                /* expectedComposeplateVisible= */ true);
    }

    @Test
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
        View mockView = mock(View.class);
        when(mockSearchBox.getView()).thenReturn(mockView);
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

    @Test
    public void testUpdateActionButtonVisibility_ComposeplateHiddenWhenIncognitoDisabled() {
        setupMockSubCoordinators();

        mCoordinator.setIsComposeplateEnabledForTesting(true);
        mCoordinator.setSearchProviderInfo(/* hasLogo= */ true, /* isGoogle= */ true);

        // Disables incognito mode.
        IncognitoUtils.setEnabledForTesting(false);
        mCoordinator.updateActionButtonVisibility();

        // Verifies that even when composeplate is enabled and Google is the search provider,
        // disabling incognito mode hides the composeplate button.
        verify(mMockComposeplate).setVisibility(eq(false), anyBoolean());
    }

    private void setupMockSubCoordinators() {
        when(mMockSearchBox.getView()).thenReturn(mMockSearchBoxView);
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
        mCoordinator.setIsComposeplateEnabledForTesting(true);

        mCoordinator.setSearchProviderInfo(targetHasLogo, targetIsGoogle);
        mCoordinator.updateActionButtonVisibility();

        Resources resources = mActivity.getResources();
        int logoTopMargin = resources.getDimensionPixelSize(R.dimen.ntp_logo_margin_top);
        int expectedBottomMargin = NtpCustomizationUtils.getLogoViewBottomMarginPx(resources);
        int expectedStyle =
                NtpCustomizationUtils.shouldApplyWhiteBackgroundOnSearchBox()
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
        when(mBrowserControlsVisibilityManager.getBottomControlsHeight()).thenReturn(100);
        when(mTab.isLoading()).thenReturn(false);

        NewTabPage.NtpScrollListener listener =
                new NewTabPage.NtpScrollListener(
                        mBrowserControlsVisibilityManager, mActivity, mTab);

        // Scroll past the threshold (20dp * 1.0 = 20px). Scroll down by 25px:
        listener.onScrolled(mRecyclerView, 0, 25);
        verify(mBrowserControlsVisibilityManager).hideAndroidControls(true);
    }

    @Test
    public void testNtpScrollListener_doesNotHideControls_whenLoading() {
        when(mBrowserControlsVisibilityManager.getBottomControlsHeight()).thenReturn(100);
        when(mTab.isLoading()).thenReturn(true);

        NewTabPage.NtpScrollListener listener =
                new NewTabPage.NtpScrollListener(
                        mBrowserControlsVisibilityManager, mActivity, mTab);

        // Scroll past the threshold.
        listener.onScrolled(mRecyclerView, 0, 25);
        verify(mBrowserControlsVisibilityManager, never()).hideAndroidControls(true);
    }

    @Test
    public void testNtpScrollListener_showsControls_whenNotLoading() {
        when(mBrowserControlsVisibilityManager.getBottomControlsHeight()).thenReturn(100);
        when(mBrowserControlsVisibilityManager.getBottomControlHiddenRatio()).thenReturn(1.0f);
        when(mTab.isLoading()).thenReturn(false);

        NewTabPage.NtpScrollListener listener =
                new NewTabPage.NtpScrollListener(
                        mBrowserControlsVisibilityManager, mActivity, mTab);

        // Scroll up past the threshold.
        listener.onScrolled(mRecyclerView, 0, -25);
        verify(mBrowserControlsVisibilityManager).showAndroidControls(true);
    }

    @Test
    public void testNtpScrollListener_doesNotShowControls_whenLoading() {
        when(mBrowserControlsVisibilityManager.getBottomControlsHeight()).thenReturn(100);
        when(mBrowserControlsVisibilityManager.getBottomControlHiddenRatio()).thenReturn(1.0f);
        when(mTab.isLoading()).thenReturn(true);

        NewTabPage.NtpScrollListener listener =
                new NewTabPage.NtpScrollListener(
                        mBrowserControlsVisibilityManager, mActivity, mTab);

        // Scroll up past the threshold.
        listener.onScrolled(mRecyclerView, 0, -25);
        verify(mBrowserControlsVisibilityManager, never()).showAndroidControls(true);
    }
}
