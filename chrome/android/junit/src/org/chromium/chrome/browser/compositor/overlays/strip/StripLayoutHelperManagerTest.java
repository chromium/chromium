// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.RectF;
import android.view.ContextThemeWrapper;
import android.view.View;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorButton;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager.TabModelStartupInfo;
import org.chromium.chrome.browser.compositor.scene_layer.TabStripSceneLayer;
import org.chromium.chrome.browser.compositor.scene_layer.TabStripSceneLayerJni;
import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementFieldTrial;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.base.LocalizationUtils;

/** Tests for {@link StripLayoutHelperManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures({ChromeFeatureList.TAB_STRIP_REDESIGN})
@Config(manifest = Config.NONE, qualifiers = "sw600dp")
public class StripLayoutHelperManagerTest {
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Mock
    private TabStripSceneLayer.Natives mTabStripSceneMock;
    @Mock
    private TabStripSceneLayer mTabStripTreeProvider;
    @Mock
    private LayoutManagerHost mManagerHost;
    @Mock
    private LayoutUpdateHost mUpdateHost;
    @Mock
    private LayoutRenderHost mRenderHost;
    @Mock
    private Supplier<LayerTitleCache> mLayerTitleCacheSupplier;
    @Mock
    private ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock
    private MultiInstanceManager mMultiInstanceManager;
    @Mock
    private View mToolbarContainerView;
    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private TabCreatorManager mTabCreatorManager;
    @Mock
    private TabModelFilterProvider mTabModelFilterProvider;
    @Mock
    private TabModel mTabModel;
    @Mock
    private Tab mSelectedTab;
    @Mock
    private StripLayoutTab mHoveredStripTab;

    private StripLayoutHelperManager mStripLayoutHelperManager;
    private Context mContext;
    private ObservableSupplierImpl<TabModelStartupInfo> mTabModelStartupInfoSupplier;
    private static final float SCREEN_WIDTH = 800.f;
    private static final float SCREEN_HEIGHT = 1600.f;
    private static final float VISIBLE_VIEWPORT_Y = 200.f;
    private static final int ORIENTATION = 2;
    private static final float BUTTON_END_PADDING_TSR = 12.f;

    @Before
    public void beforeTest() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(TabStripSceneLayerJni.TEST_HOOKS, mTabStripSceneMock);
        mContext = new ContextThemeWrapper(
                ApplicationProvider.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        TabStripSceneLayer.setTestFlag(true);
        initializeTest();
    }

    @After
    public void tearDown() {
        TabStripSceneLayer.setTestFlag(false);
    }

    private void initializeTest() {
        when(mTabModelSelector.getTabModelFilterProvider()).thenReturn(mTabModelFilterProvider);

        mTabModelStartupInfoSupplier = new ObservableSupplierImpl<>();
        mStripLayoutHelperManager = new StripLayoutHelperManager(mContext, mManagerHost,
                mUpdateHost, mRenderHost, mLayerTitleCacheSupplier, mTabModelStartupInfoSupplier,
                mLifecycleDispatcher, mMultiInstanceManager, mToolbarContainerView);
        mStripLayoutHelperManager.setTabModelSelector(mTabModelSelector, mTabCreatorManager);
    }

    private void initializeTestWithTsrArm(BooleanCachedFieldTrialParameter param) {
        // Since we check TSR arm and determine model selector button properties(eg. color/bg color,
        // width, etc) inside constructor, so need to set TSR arm before initialize test each time
        // we switch arm.
        param.setForTesting(true);
        initializeTest();
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testGetBackgroundColorDetached() {
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_DETACHED.setForTesting(true);
        mStripLayoutHelperManager.onContextChanged(mContext);
        assertEquals(ChromeColors.getSurfaceColor(mContext, R.dimen.default_elevation_0),
                mStripLayoutHelperManager.getBackgroundColor());
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testGetBackgroundColorFolio() {
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_FOLIO.setForTesting(true);
        mStripLayoutHelperManager.onContextChanged(mContext);
        assertEquals(ChromeColors.getSurfaceColor(mContext, R.dimen.default_elevation_3),
                mStripLayoutHelperManager.getBackgroundColor());
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.TAB_STRIP_REDESIGN)
    public void testModelSelectorButtonXPosition() {
        // Set model selector button position.
        mStripLayoutHelperManager.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, VISIBLE_VIEWPORT_Y, ORIENTATION);

        // Verify model selector button x-position.
        // stripWidth(800) - buttonEndPadding(12) - MsbWidth(24) = 764
        assertEquals("Model selector button x-position is not as expected", 764.f,
                mStripLayoutHelperManager.getModelSelectorButton().getX(), 0.0);
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.TAB_STRIP_REDESIGN)
    public void testModelSelectorButtonXPosition_Rtl() {
        // Set model selector button position.
        LocalizationUtils.setRtlForTesting(true);
        mStripLayoutHelperManager.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, VISIBLE_VIEWPORT_Y, ORIENTATION);

        // Verify model selector button x-position.
        // msbEndPadding(12)
        assertEquals("Model selector button x-position is not as expected", 12.f,
                mStripLayoutHelperManager.getModelSelectorButton().getX(), 0.0);
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testModelSelectorButtonXPosition_TSR() {
        // Set model selector button position.
        mStripLayoutHelperManager.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, VISIBLE_VIEWPORT_Y, ORIENTATION);

        // Verify model selector button x-position.
        // stripWidth(800) - buttonEndPadding(12) - MsbWidth(32) = 756
        assertEquals("Model selector button x-position is not as expected", 756.f,
                mStripLayoutHelperManager.getModelSelectorButton().getX(), 0.0);
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testModelSelectorButtonXPosition_RTL_TSR() {
        // Set model selector button position.
        LocalizationUtils.setRtlForTesting(true);
        mStripLayoutHelperManager.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, VISIBLE_VIEWPORT_Y, ORIENTATION);

        // Verify model selector button position.
        assertEquals("Model selector button x-position is not as expected", BUTTON_END_PADDING_TSR,
                mStripLayoutHelperManager.getModelSelectorButton().getX(), 0.0);
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testModelSelectorButtonYPosition_Folio() {
        // setup
        initializeTestWithTsrArm(TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_FOLIO);

        // Set model selector button position.
        mStripLayoutHelperManager.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, VISIBLE_VIEWPORT_Y, ORIENTATION);

        // Verify model selector button y-position.
        assertEquals("Model selector button y-position is not as expected", 3.f,
                mStripLayoutHelperManager.getModelSelectorButton().getY(), 0.0);
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testModelSelectorButtonYPosition_Detached() {
        // setup
        initializeTestWithTsrArm(TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_DETACHED);

        // Set model selector button position.
        mStripLayoutHelperManager.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, VISIBLE_VIEWPORT_Y, ORIENTATION);

        // Verify model selector button y-position.
        assertEquals("Model selector button y-position is not as expected", 5.f,
                mStripLayoutHelperManager.getModelSelectorButton().getY(), 0.0);
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.TAB_STRIP_REDESIGN)
    public void testFadeDrawable_Left() {
        // Verify fade drawable.
        assertEquals("Fade drawable resource is not as expected", R.drawable.tab_strip_fade_short,
                mStripLayoutHelperManager.getLeftFadeDrawable());
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.TAB_STRIP_REDESIGN)
    public void testFadeDrawable_Right() {
        // Verify fade drawable.
        assertEquals("Fade drawable resource is not as expected", R.drawable.tab_strip_fade_short,
                mStripLayoutHelperManager.getRightFadeDrawable());
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.TAB_STRIP_REDESIGN)
    public void testFadeDrawable_Right_ModelSelectorButtonVisible() {
        // setup
        mStripLayoutHelperManager.setModelSelectorButtonVisibleForTesting(true);

        // Verify fade drawable.
        assertEquals("Fade drawable resource is not as expected", R.drawable.tab_strip_fade_long,
                mStripLayoutHelperManager.getRightFadeDrawable());
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testFadeDrawable_Right_ModelSelectorButtonVisible_TSR() {
        // setup
        mStripLayoutHelperManager.setModelSelectorButtonVisibleForTesting(true);

        // Verify fade drawable.
        assertEquals("Fade drawable resource is not as expected",
                R.drawable.tab_strip_fade_long_tsr,
                mStripLayoutHelperManager.getRightFadeDrawable());
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testFadeDrawable_Right_TSR() {
        // Verify fade drawable.
        assertEquals("Fade drawable resource is not as expected",
                R.drawable.tab_strip_fade_medium_tsr,
                mStripLayoutHelperManager.getRightFadeDrawable());
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testFadeDrawable_Left_TSR() {
        // setup
        mStripLayoutHelperManager.setModelSelectorButtonVisibleForTesting(true);

        // Verify fade drawable.
        assertEquals("Fade drawable resource is not as expected",
                R.drawable.tab_strip_fade_short_tsr,
                mStripLayoutHelperManager.getLeftFadeDrawable());
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testFadeDrawable_Left_RTL_ModelSelectorButtonVisible_TSR() {
        // setup
        mStripLayoutHelperManager.setModelSelectorButtonVisibleForTesting(true);
        LocalizationUtils.setRtlForTesting(true);

        // Verify fade drawable.
        assertEquals("Fade drawable resource is not as expected",
                R.drawable.tab_strip_fade_long_tsr,
                mStripLayoutHelperManager.getLeftFadeDrawable());
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testFadeDrawable_Left_RTL_TSR() {
        // setup
        LocalizationUtils.setRtlForTesting(true);

        // Verify fade drawable.
        assertEquals("Fade drawable resource is not as expected",
                R.drawable.tab_strip_fade_medium_tsr,
                mStripLayoutHelperManager.getLeftFadeDrawable());
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testFadeDrawable_Right_RTL_TSR() {
        // setup
        mStripLayoutHelperManager.setModelSelectorButtonVisibleForTesting(true);
        LocalizationUtils.setRtlForTesting(true);

        // Verify fade drawable.
        assertEquals("Fade drawable resource is not as expected",
                R.drawable.tab_strip_fade_short_tsr,
                mStripLayoutHelperManager.getRightFadeDrawable());
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testButtonIconColor() {
        // Verify TSR button icon color.
        assertEquals("Unexpected incognito button color.",
                mContext.getResources().getColor(R.color.model_selector_button_icon_color),
                ((TintedCompositorButton) mStripLayoutHelperManager.getModelSelectorButton())
                        .getTint());
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testButtonIconColor_DisableButtonStyle() {
        // setup
        initializeTestWithTsrArm(TabUiFeatureUtilities.TAB_STRIP_REDESIGN_DISABLE_BUTTON_STYLE);

        // Verify TSR button icon color after disabling button style.
        assertEquals("Unexpected incognito button color.",
                AppCompatResources.getColorStateList(mContext, R.color.default_icon_color_tint_list)
                        .getDefaultColor(),
                ((TintedCompositorButton) mStripLayoutHelperManager.getModelSelectorButton())
                        .getTint());
    }

    @Test
    @Feature("TabStripPerformance")
    @Features.EnableFeatures(ChromeFeatureList.TAB_STRIP_STARTUP_REFACTORING)
    public void testSetTabModelStartupInfo() {
        // Setup
        int expectedStandardCount = 5;
        int expectedIncognitoCount = 0;
        int expectedStandardActiveTabIndex = 2;
        int expectedIncognitoActiveTabIndex = Tab.INVALID_TAB_ID;
        boolean expectedStandardCreatedTabOnStartup = false;
        boolean expectedIncognitoCreatedTabOnStartup = false;
        TabModelStartupInfo startupInfo =
                new TabModelStartupInfo(expectedStandardCount, expectedIncognitoCount,
                        expectedStandardActiveTabIndex, expectedIncognitoActiveTabIndex,
                        expectedStandardCreatedTabOnStartup, expectedIncognitoCreatedTabOnStartup);
        mTabModelStartupInfoSupplier.set(startupInfo);

        // Verify
        StripLayoutHelper standardHelper = mStripLayoutHelperManager.getStripLayoutHelper(false);
        assertEquals("Unexpected standard tab count.", expectedStandardCount,
                standardHelper.getTabCountOnStartupForTesting());
        assertEquals("Unexpected standard active tab index.", expectedStandardActiveTabIndex,
                standardHelper.getActiveTabIndexOnStartupForTesting());
        assertEquals("Unexpected standard tab created on startup value",
                expectedStandardCreatedTabOnStartup,
                standardHelper.getCreatedTabOnStartupForTesting());

        StripLayoutHelper incognitoHelper = mStripLayoutHelperManager.getStripLayoutHelper(true);
        assertEquals("Unexpected incognito tab count.", expectedIncognitoCount,
                incognitoHelper.getTabCountOnStartupForTesting());
        assertEquals("Unexpected incognito active tab index.", expectedIncognitoActiveTabIndex,
                incognitoHelper.getActiveTabIndexOnStartupForTesting());
        assertEquals("Unexpected incognito tab created on startup value",
                expectedIncognitoCreatedTabOnStartup,
                standardHelper.getCreatedTabOnStartupForTesting());
    }

    @Test
    public void testGetUpdatedSceneOverlayTree() {
        // Setup and stub required mocks.
        int hoveredTabId = 1;
        int selectedTabId = 2;
        mStripLayoutHelperManager.setTabStripTreeProviderForTesting(mTabStripTreeProvider);

        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.index()).thenReturn(selectedTabId);
        when(mTabModel.getTabAt(selectedTabId)).thenReturn(mSelectedTab);
        when(mSelectedTab.getId()).thenReturn(selectedTabId);

        when(mHoveredStripTab.getId()).thenReturn(hoveredTabId);
        var activeLayoutHelper = mStripLayoutHelperManager.getActiveStripLayoutHelper();
        activeLayoutHelper.setLastHoveredTabForTesting(mHoveredStripTab);

        // Invoke the method.
        mStripLayoutHelperManager.getUpdatedSceneOverlayTree(
                new RectF(), new RectF(), mRenderHost.getResourceManager(), 0f);

        // Verify the call to #pushAndUpdateStrip.
        verify(mTabStripTreeProvider)
                .pushAndUpdateStrip(mStripLayoutHelperManager, mLayerTitleCacheSupplier.get(),
                        mRenderHost.getResourceManager(),
                        activeLayoutHelper.getStripLayoutTabsToRender(), 0f, selectedTabId,
                        hoveredTabId);
    }
}
