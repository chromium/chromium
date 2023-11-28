// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.multiwindow.MultiWindowTestUtils.enableMultiInstance;

import android.content.Context;
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
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorButton;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager.TabModelStartupInfo;
import org.chromium.chrome.browser.compositor.scene_layer.TabStripSceneLayer;
import org.chromium.chrome.browser.compositor.scene_layer.TabStripSceneLayerJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.dragdrop.DragAndDropDelegate;

/** Tests for {@link StripLayoutHelperManager}. */
@RunWith(BaseRobolectricTestRunner.class)
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
    @Mock private Tab mSelectedTab;
    @Mock private StripLayoutTab mHoveredStripTab;
    @Mock private ViewStub mTabHoverCardViewStub;
    @Mock private ObservableSupplierImpl<TabContentManager> mTabContentManagerSupplier;
    @Mock private BrowserControlsStateProvider mBrowserControlStateProvider;
    @Mock private WindowAndroid mWindowAndroid;

    private StripLayoutHelperManager mStripLayoutHelperManager;
    private Context mContext;
    private ObservableSupplierImpl<TabModelStartupInfo> mTabModelStartupInfoSupplier;
    private static final float SCREEN_WIDTH = 800.f;
    private static final float SCREEN_HEIGHT = 1600.f;
    private static final float VISIBLE_VIEWPORT_Y = 200.f;
    private static final int ORIENTATION = 2;
    private static final float BUTTON_END_PADDING = 8.f;

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
                        mTabContentManagerSupplier,
                        mBrowserControlStateProvider,
                        mWindowAndroid);
        mStripLayoutHelperManager.setTabModelSelector(mTabModelSelector, mTabCreatorManager);
    }

    @Test
    public void testGetBackgroundColor() {
        mStripLayoutHelperManager.onContextChanged(mContext);
        assertEquals(
                ChromeColors.getSurfaceColor(mContext, R.dimen.default_elevation_3),
                mStripLayoutHelperManager.getBackgroundColor());
    }

    @Test
    public void testModelSelectorButtonXPosition() {
        // Set model selector button position.
        mStripLayoutHelperManager.setModelSelectorButtonVisibleForTesting(true);
        mStripLayoutHelperManager.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, VISIBLE_VIEWPORT_Y, ORIENTATION);

        // Verify model selector button x-position.
        // stripWidth(800) - buttonEndPadding(8) - MsbWidth(32) = 760
        assertEquals(
                "Model selector button x-position is not as expected",
                760.f,
                mStripLayoutHelperManager.getModelSelectorButton().getX(),
                0.0);
    }

    @Test
    public void testModelSelectorButtonXPosition_RTL() {
        // Set model selector button position.
        LocalizationUtils.setRtlForTesting(true);
        mStripLayoutHelperManager.setModelSelectorButtonVisibleForTesting(true);
        mStripLayoutHelperManager.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, VISIBLE_VIEWPORT_Y, ORIENTATION);

        // Verify model selector button position.
        assertEquals(
                "Model selector button x-position is not as expected",
                BUTTON_END_PADDING,
                mStripLayoutHelperManager.getModelSelectorButton().getX(),
                0.0);
    }

    @Test
    public void testModelSelectorButtonYPosition() {
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
    @Feature("Advanced Peripherals Support")
    public void testModelSelectorButtonHoverHighlightProperties() {
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
    public void testFadeDrawable_Right() {
        // Verify fade drawable.
        assertEquals(
                "Fade drawable resource is not as expected",
                R.drawable.tab_strip_fade_medium,
                mStripLayoutHelperManager.getRightFadeDrawable());
    }

    @Test
    public void testFadeDrawable_Left() {
        // setup
        mStripLayoutHelperManager.setModelSelectorButtonVisibleForTesting(true);

        // Verify fade drawable.
        assertEquals(
                "Fade drawable resource is not as expected",
                R.drawable.tab_strip_fade_short,
                mStripLayoutHelperManager.getLeftFadeDrawable());
    }

    @Test
    public void testFadeDrawable_Left_RTL_ModelSelectorButtonVisible() {
        // setup
        mStripLayoutHelperManager.setModelSelectorButtonVisibleForTesting(true);
        LocalizationUtils.setRtlForTesting(true);

        // Verify fade drawable.
        assertEquals(
                "Fade drawable resource is not as expected",
                R.drawable.tab_strip_fade_long,
                mStripLayoutHelperManager.getLeftFadeDrawable());
    }

    @Test
    public void testFadeDrawable_Left_RTL() {
        // setup
        LocalizationUtils.setRtlForTesting(true);

        // Verify fade drawable.
        assertEquals(
                "Fade drawable resource is not as expected",
                R.drawable.tab_strip_fade_medium,
                mStripLayoutHelperManager.getLeftFadeDrawable());
    }

    @Test
    public void testFadeDrawable_Right_RTL() {
        // setup
        mStripLayoutHelperManager.setModelSelectorButtonVisibleForTesting(true);
        LocalizationUtils.setRtlForTesting(true);

        // Verify fade drawable.
        assertEquals(
                "Fade drawable resource is not as expected",
                R.drawable.tab_strip_fade_short,
                mStripLayoutHelperManager.getRightFadeDrawable());
    }

    @Test
    public void testButtonIconColor_DisableButtonStyle() {
        // Verify button icon color after disabling button style.
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
    }

    @Test
    @Config(sdk = VERSION_CODES.S)
    @EnableFeatures(ChromeFeatureList.TAB_LINK_DRAG_DROP_ANDROID)
    public void testGetDragListener() throws NameNotFoundException {
        enableMultiInstance();
        initializeTest();
        assertNotNull("DragListener should be set.", mStripLayoutHelperManager.getDragListener());
    }
}
