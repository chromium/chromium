// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.graphics.RectF;
import android.os.Build.VERSION_CODES;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.ViewStub;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.ColorUtils;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
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
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
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
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.dragdrop.DragAndDropDelegate;

/** Tests for {@link StripLayoutHelperManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.TAB_STRIP_REDESIGN})
@Config(manifest = Config.NONE, qualifiers = "sw600dp")
public class StripLayoutHelperManagerTest {
    @Rule public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule public JniMocker mJniMocker = new JniMocker();
    @Mock private TabStripSceneLayer.Natives mTabStripSceneMock;
    @Mock private TabStripSceneLayer mTabStripTreeProvider;
    @Mock private LayoutManagerHost mManagerHost;
    @Mock private LayoutUpdateHost mUpdateHost;
    @Mock private LayoutRenderHost mRenderHost;
    @Mock private Supplier<LayerTitleCache> mLayerTitleCacheSupplier;
    @Mock private ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock private MultiInstanceManager mMultiInstanceManager;
    @Mock private View mToolbarContainerView;
    @Mock private DragAndDropDelegate mDragDropDelegate;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private TabModelFilterProvider mTabModelFilterProvider;
    @Mock private TabModel mStandardTabModel;
    @Mock private TabModel mIncognitoTabModel;
    @Mock private Tab mSelectedTab;
    @Mock private StripLayoutTab mHoveredStripTab;
    @Mock private ViewStub mTabHoverCardViewStub;
    @Mock private ObservableSupplierImpl<TabContentManager> mTabContentManagerSupplier;

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
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        when(mToolbarContainerView.getContext()).thenReturn(mContext);
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
        mStripLayoutHelperManager =
                new StripLayoutHelperManager(
                        mContext,
                        mManagerHost,
                        mUpdateHost,
                        mRenderHost,
                        mLayerTitleCacheSupplier,
                        mTabModelStartupInfoSupplier,
                        mLifecycleDispatcher,
                        mMultiInstanceManager,
                        mDragDropDelegate,
                        mToolbarContainerView,
                        mTabHoverCardViewStub,
                        mTabContentManagerSupplier);
        mStripLayoutHelperManager.setTabModelSelector(mTabModelSelector, mTabCreatorManager);
    }

    private void initializeTestWithTsrArm(BooleanCachedFieldTrialParameter param, boolean value) {
        // Since we check TSR arm and determine model selector button properties(eg. color/bg color,
        // width, etc) inside constructor, so need to set TSR arm before initialize test each time
        // we switch arm.
        param.setForTesting(value);
        initializeTest();
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testGetBackgroundColorDetached() {
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_DETACHED.setForTesting(true);
        initializeTestWithTsrArm(TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_FOLIO, false);
        mStripLayoutHelperManager.onContextChanged(mContext);
        assertEquals(
                ChromeColors.getSurfaceColor(mContext, R.dimen.default_elevation_0),
                mStripLayoutHelperManager.getBackgroundColor());
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testGetBackgroundColorFolio() {
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_FOLIO.setForTesting(true);
        mStripLayoutHelperManager.onContextChanged(mContext);
        assertEquals(
                ChromeColors.getSurfaceColor(mContext, R.dimen.default_elevation_3),
                mStripLayoutHelperManager.getBackgroundColor());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STRIP_REDESIGN)
    public void testModelSelectorButtonXPosition() {
        // Set model selector button position.
        mStripLayoutHelperManager.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, VISIBLE_VIEWPORT_Y, ORIENTATION);

        // Verify model selector button x-position.
        // stripWidth(800) - buttonEndPadding(12) - MsbWidth(24) = 764
        assertEquals(
                "Model selector button x-position is not as expected",
                764.f,
                mStripLayoutHelperManager.getModelSelectorButton().getX(),
                0.0);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STRIP_REDESIGN)
    public void testModelSelectorButtonXPosition_Rtl() {
        // Set model selector button position.
        LocalizationUtils.setRtlForTesting(true);
        mStripLayoutHelperManager.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, VISIBLE_VIEWPORT_Y, ORIENTATION);

        // Verify model selector button x-position.
        // msbEndPadding(12)
        assertEquals(
                "Model selector button x-position is not as expected",
                12.f,
                mStripLayoutHelperManager.getModelSelectorButton().getX(),
                0.0);
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testModelSelectorButtonXPosition_TSR() {
        // Set model selector button position.
        mStripLayoutHelperManager.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, VISIBLE_VIEWPORT_Y, ORIENTATION);

        // Verify model selector button x-position.
        // stripWidth(800) - buttonEndPadding(12) - MsbWidth(32) = 756
        assertEquals(
                "Model selector button x-position is not as expected",
                756.f,
                mStripLayoutHelperManager.getModelSelectorButton().getX(),
                0.0);
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testModelSelectorButtonXPosition_RTL_TSR() {
        // Set model selector button position.
        LocalizationUtils.setRtlForTesting(true);
        mStripLayoutHelperManager.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, VISIBLE_VIEWPORT_Y, ORIENTATION);

        // Verify model selector button position.
        assertEquals(
                "Model selector button x-position is not as expected",
                BUTTON_END_PADDING_TSR,
                mStripLayoutHelperManager.getModelSelectorButton().getX(),
                0.0);
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testModelSelectorButtonYPosition_Folio() {
        // setup
        initializeTestWithTsrArm(TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_FOLIO, true);

        // Set model selector button position.
        mStripLayoutHelperManager.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, VISIBLE_VIEWPORT_Y, ORIENTATION);

        // Verify model selector button y-position.
        assertEquals(
                "Model selector button y-position is not as expected",
                3.f,
                mStripLayoutHelperManager.getModelSelectorButton().getY(),
                0.0);
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testModelSelectorButtonYPosition_Detached() {
        // setup
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_FOLIO.setForTesting(false);
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_DETACHED.setForTesting(true);
        initializeTest();

        // Set model selector button position.
        mStripLayoutHelperManager.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, VISIBLE_VIEWPORT_Y, ORIENTATION);

        // Verify model selector button y-position.
        assertEquals(
                "Model selector button y-position is not as expected",
                5.f,
                mStripLayoutHelperManager.getModelSelectorButton().getY(),
                0.0);
    }

    @Test
    @Feature("Advanced Peripherals Support")
    public void testModelSelectorButtonHoverHighlightProperties() {
        // setup
        initializeTestWithTsrArm(TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_DETACHED, true);

        // Set model selector button position.
        mStripLayoutHelperManager.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, VISIBLE_VIEWPORT_Y, ORIENTATION);

        // Verify model selector button hover highlight resource id.
        assertEquals(
                "Model selector button hover highlight is not as expected",
                R.drawable.bg_circle_tab_strip_button,
                ((TintedCompositorButton) mStripLayoutHelperManager.getModelSelectorButton())
                        .getBackgroundResourceId());

        // Verify model selector button hover highlight default tint.
        TintedCompositorButton msb =
                ((TintedCompositorButton) spy(mStripLayoutHelperManager.getModelSelectorButton()));
        when(msb.isHovered()).thenReturn(true);
        when(msb.isPressedFromMouse()).thenReturn(false);

        int hoverBackgroundDefaultColor =
                ColorUtils.setAlphaComponent(
                        SemanticColorUtils.getDefaultTextColor(mContext), (int) (0.08 * 255));
        assertEquals(
                "Model selector button hover highlight default tint is not as expected",
                hoverBackgroundDefaultColor,
                msb.getBackgroundTint());

        // Verify model selector button hover highlight pressed tint.
        when(msb.isPressed()).thenReturn(true);
        when(msb.isHovered()).thenReturn(false);
        when(msb.isPressedFromMouse()).thenReturn(true);
        int hoverBackgroundPressedColor =
                ColorUtils.setAlphaComponent(
                        SemanticColorUtils.getDefaultTextColor(mContext), (int) (0.12 * 255));
        assertEquals(
                "Model selector button hover highlight pressed tint is not as expected",
                hoverBackgroundPressedColor,
                msb.getBackgroundTint());
        when(msb.isPressed()).thenReturn(false);

        // Verify model selector button incognito hover highlight default tint.
        when(msb.isHovered()).thenReturn(true);
        when(msb.isIncognito()).thenReturn(true);
        int hoverBackgroundDefaultIncognitoColor =
                ColorUtils.setAlphaComponent(
                        mContext.getResources().getColor(R.color.tab_strip_button_hover_bg_color),
                        (int) (0.08 * 255));
        assertEquals(
                "Model selector button hover highlight pressed tint is not as expected",
                hoverBackgroundDefaultIncognitoColor,
                msb.getBackgroundTint());

        // Verify model selector button incognito hover highlight pressed tint.
        when(msb.isPressed()).thenReturn(true);
        when(msb.isHovered()).thenReturn(false);
        when(msb.isPressedFromMouse()).thenReturn(true);
        int hoverBackgroundPressedIncognitoColor =
                ColorUtils.setAlphaComponent(
                        mContext.getResources().getColor(R.color.tab_strip_button_hover_bg_color),
                        (int) (0.12 * 255));
        assertEquals(
                "Model selector button hover highlight pressed tint is not as expected",
                hoverBackgroundPressedIncognitoColor,
                msb.getBackgroundTint());
    }

    @Test
    @Feature("Advanced Peripherals Support")
    @EnableFeatures(ChromeFeatureList.ADVANCED_PERIPHERALS_SUPPORT_TAB_STRIP)
    public void testModelSelectorButtonHoverEnter() {
        // Setup
        initializeTestWithTsrArm(TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_DETACHED, true);
        mStripLayoutHelperManager.setModelSelectorButtonVisibleForTesting(true);

        int x = (int) mStripLayoutHelperManager.getModelSelectorButton().getX();
        mStripLayoutHelperManager
                .getActiveStripLayoutHelper()
                .onHoverEnter(
                        x + 1, 0); // mouse position within MSB range(32dp width + 12dp click slop).
        assertTrue(
                "Model selector button should be hovered",
                mStripLayoutHelperManager.getModelSelectorButton().isHovered());

        // Verify model selector button is NOT hovered when mouse is not on the button.
        mStripLayoutHelperManager
                .getActiveStripLayoutHelper()
                .onHoverEnter(
                        x + 45,
                        0); // mouse position out of MSB range(32dp width + 12dp click slop).
        assertFalse(
                "Model selector button should NOT be hovered",
                mStripLayoutHelperManager.getModelSelectorButton().isHovered());
    }

    @Test
    @Feature("Advanced Peripherals Support")
    @EnableFeatures(ChromeFeatureList.ADVANCED_PERIPHERALS_SUPPORT_TAB_STRIP)
    public void testModelSelectorButtonHoverOnDown() {
        // Setup
        initializeTestWithTsrArm(TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_DETACHED, true);
        mStripLayoutHelperManager.setModelSelectorButtonVisibleForTesting(true);

        // Verify model selector button is in pressed state, not hover state, when click is from
        // mouse.
        mStripLayoutHelperManager.simulateOnDownForTesting(
                mStripLayoutHelperManager.getModelSelectorButton().getX() + 1, 0, true, 1);
        assertFalse(
                "Model selector button should not be hovered",
                mStripLayoutHelperManager.getModelSelectorButton().isHovered());
        assertTrue(
                "Model selector button should be pressed from mouse",
                mStripLayoutHelperManager.getModelSelectorButton().isPressedFromMouse());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STRIP_REDESIGN)
    public void testFadeDrawable_Left() {
        // Verify fade drawable.
        assertEquals(
                "Fade drawable resource is not as expected",
                R.drawable.tab_strip_fade_short,
                mStripLayoutHelperManager.getLeftFadeDrawable());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STRIP_REDESIGN)
    public void testFadeDrawable_Right() {
        // Verify fade drawable.
        assertEquals(
                "Fade drawable resource is not as expected",
                R.drawable.tab_strip_fade_short,
                mStripLayoutHelperManager.getRightFadeDrawable());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STRIP_REDESIGN)
    public void testFadeDrawable_Right_ModelSelectorButtonVisible() {
        // setup
        mStripLayoutHelperManager.setModelSelectorButtonVisibleForTesting(true);

        // Verify fade drawable.
        assertEquals(
                "Fade drawable resource is not as expected",
                R.drawable.tab_strip_fade_long,
                mStripLayoutHelperManager.getRightFadeDrawable());
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testFadeDrawable_Right_ModelSelectorButtonVisible_TSR() {
        // setup
        mStripLayoutHelperManager.setModelSelectorButtonVisibleForTesting(true);

        // Verify fade drawable.
        assertEquals(
                "Fade drawable resource is not as expected",
                R.drawable.tab_strip_fade_long_tsr,
                mStripLayoutHelperManager.getRightFadeDrawable());
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testFadeDrawable_Right_TSR() {
        // Verify fade drawable.
        assertEquals(
                "Fade drawable resource is not as expected",
                R.drawable.tab_strip_fade_medium_tsr,
                mStripLayoutHelperManager.getRightFadeDrawable());
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testFadeDrawable_Left_TSR() {
        // setup
        mStripLayoutHelperManager.setModelSelectorButtonVisibleForTesting(true);

        // Verify fade drawable.
        assertEquals(
                "Fade drawable resource is not as expected",
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
        assertEquals(
                "Fade drawable resource is not as expected",
                R.drawable.tab_strip_fade_long_tsr,
                mStripLayoutHelperManager.getLeftFadeDrawable());
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testFadeDrawable_Left_RTL_TSR() {
        // setup
        LocalizationUtils.setRtlForTesting(true);

        // Verify fade drawable.
        assertEquals(
                "Fade drawable resource is not as expected",
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
        assertEquals(
                "Fade drawable resource is not as expected",
                R.drawable.tab_strip_fade_short_tsr,
                mStripLayoutHelperManager.getRightFadeDrawable());
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testButtonIconColor() {
        // setup
        initializeTestWithTsrArm(
                TabUiFeatureUtilities.TAB_STRIP_REDESIGN_DISABLE_BUTTON_STYLE, false);

        // Verify TSR button icon color.
        assertEquals(
                "Unexpected incognito button color.",
                mContext.getResources().getColor(R.color.model_selector_button_icon_color),
                ((TintedCompositorButton) mStripLayoutHelperManager.getModelSelectorButton())
                        .getTint());
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testButtonIconColor_DisableButtonStyle() {
        // setup
        initializeTestWithTsrArm(
                TabUiFeatureUtilities.TAB_STRIP_REDESIGN_DISABLE_BUTTON_STYLE, true);

        // Verify TSR button icon color after disabling button style.
        assertEquals(
                "Unexpected incognito button color.",
                AppCompatResources.getColorStateList(mContext, R.color.default_icon_color_tint_list)
                        .getDefaultColor(),
                ((TintedCompositorButton) mStripLayoutHelperManager.getModelSelectorButton())
                        .getTint());
    }

    @Test
    @Feature("TabStripPerformance")
    @EnableFeatures(ChromeFeatureList.TAB_STRIP_STARTUP_REFACTORING)
    public void testSetTabModelStartupInfo() {
        // Setup
        int expectedStandardCount = 5;
        int expectedIncognitoCount = 0;
        int expectedStandardActiveTabIndex = 2;
        int expectedIncognitoActiveTabIndex = Tab.INVALID_TAB_ID;
        boolean expectedStandardCreatedTabOnStartup = false;
        boolean expectedIncognitoCreatedTabOnStartup = false;
        TabModelStartupInfo startupInfo =
                new TabModelStartupInfo(
                        expectedStandardCount,
                        expectedIncognitoCount,
                        expectedStandardActiveTabIndex,
                        expectedIncognitoActiveTabIndex,
                        expectedStandardCreatedTabOnStartup,
                        expectedIncognitoCreatedTabOnStartup);
        mTabModelStartupInfoSupplier.set(startupInfo);

        // Verify
        StripLayoutHelper standardHelper = mStripLayoutHelperManager.getStripLayoutHelper(false);
        assertEquals(
                "Unexpected standard tab count.",
                expectedStandardCount,
                standardHelper.getTabCountOnStartupForTesting());
        assertEquals(
                "Unexpected standard active tab index.",
                expectedStandardActiveTabIndex,
                standardHelper.getActiveTabIndexOnStartupForTesting());
        assertEquals(
                "Unexpected standard tab created on startup value",
                expectedStandardCreatedTabOnStartup,
                standardHelper.getCreatedTabOnStartupForTesting());

        StripLayoutHelper incognitoHelper = mStripLayoutHelperManager.getStripLayoutHelper(true);
        assertEquals(
                "Unexpected incognito tab count.",
                expectedIncognitoCount,
                incognitoHelper.getTabCountOnStartupForTesting());
        assertEquals(
                "Unexpected incognito active tab index.",
                expectedIncognitoActiveTabIndex,
                incognitoHelper.getActiveTabIndexOnStartupForTesting());
        assertEquals(
                "Unexpected incognito tab created on startup value",
                expectedIncognitoCreatedTabOnStartup,
                standardHelper.getCreatedTabOnStartupForTesting());
    }

    @Test
    public void testGetUpdatedSceneOverlayTree() {
        // Setup and stub required mocks.
        int hoveredTabId = 1;
        int selectedTabId = 2;
        mStripLayoutHelperManager.setTabStripTreeProviderForTesting(mTabStripTreeProvider);

        when(mTabModelSelector.getCurrentModel()).thenReturn(mStandardTabModel);
        when(mStandardTabModel.index()).thenReturn(selectedTabId);
        when(mStandardTabModel.getTabAt(selectedTabId)).thenReturn(mSelectedTab);
        when(mSelectedTab.getId()).thenReturn(selectedTabId);

        when(mHoveredStripTab.getId()).thenReturn(hoveredTabId);
        var activeLayoutHelper = mStripLayoutHelperManager.getActiveStripLayoutHelper();
        activeLayoutHelper.setLastHoveredTabForTesting(mHoveredStripTab);

        // Invoke the method.
        mStripLayoutHelperManager.getUpdatedSceneOverlayTree(
                new RectF(), new RectF(), mRenderHost.getResourceManager(), 0f);

        // Verify the call to #pushAndUpdateStrip.
        verify(mTabStripTreeProvider)
                .pushAndUpdateStrip(
                        mStripLayoutHelperManager,
                        mLayerTitleCacheSupplier.get(),
                        mRenderHost.getResourceManager(),
                        activeLayoutHelper.getStripLayoutTabsToRender(),
                        0f,
                        selectedTabId,
                        hoveredTabId);
    }

    @Test
    @Config(sdk = VERSION_CODES.R)
    @EnableFeatures(ChromeFeatureList.TAB_LINK_DRAG_DROP_ANDROID)
    public void testDragDropInstances_Success() throws NameNotFoundException {
        enableMultiInstance();
        initializeTest();
        assertNotNull(
                "Tab drag source should be set.",
                mStripLayoutHelperManager.getTabDragSourceForTesting());
        assertNotNull(
                "Tab drop target should be set.",
                mStripLayoutHelperManager.getTabDropTargetForTesting());
    }

    @Test
    @Config(sdk = VERSION_CODES.Q)
    @EnableFeatures(ChromeFeatureList.TAB_LINK_DRAG_DROP_ANDROID)
    public void testDragDropInstances_MultiInstanceNotEnabled_ReturnsNull()
            throws NameNotFoundException {
        enableMultiInstance();
        initializeTest();
        assertNull(
                "Tab drag source should not be set.",
                mStripLayoutHelperManager.getTabDragSourceForTesting());
        assertNull(
                "Tab drop target should not be set.",
                mStripLayoutHelperManager.getTabDropTargetForTesting());
    }

    @Test
    @DisableFeatures({
        ChromeFeatureList.TAB_LINK_DRAG_DROP_ANDROID,
        ChromeFeatureList.TAB_DRAG_DROP_ANDROID
    })
    public void testDragDropInstances_FlagsDisabled_ReturnsNull() throws NameNotFoundException {
        enableMultiInstance();
        initializeTest();
        assertNull(
                "Tab drag source should not be set.",
                mStripLayoutHelperManager.getTabDragSourceForTesting());
        assertNull(
                "Tab drop target should not be set.",
                mStripLayoutHelperManager.getTabDropTargetForTesting());
    }

    private void enableMultiInstance() throws NameNotFoundException {
        Context mApplicationContext = Mockito.spy(ContextUtils.getApplicationContext());
        PackageManager packageManager = mock(PackageManager.class);
        when(mApplicationContext.getPackageManager()).thenReturn(packageManager);
        ActivityInfo activityInfo = mock(ActivityInfo.class);
        when(packageManager.getActivityInfo(any(), anyInt())).thenReturn(activityInfo);
        ContextUtils.initApplicationContextForTests(mApplicationContext);
        activityInfo.launchMode = ActivityInfo.LAUNCH_SINGLE_INSTANCE_PER_TASK;
    }
}
