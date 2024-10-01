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
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.multiwindow.MultiWindowTestUtils.enableMultiInstance;

import android.app.Activity;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewStub;

import androidx.annotation.ColorInt;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.ColorUtils;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorButton;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.AreaMotionEventFilter;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager.StripVisibilityState;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager.TabModelStartupInfo;
import org.chromium.chrome.browser.compositor.scene_layer.TabStripSceneLayer;
import org.chromium.chrome.browser.compositor.scene_layer.TabStripSceneLayerJni;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.chrome.browser.toolbar.ToolbarFeatures;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.dragdrop.DragAndDropDelegate;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

/** Tests for {@link StripLayoutHelperManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, qualifiers = "sw600dp")
@DisableFeatures(ChromeFeatureList.TAB_STRIP_INCOGNITO_MIGRATION)
public class StripLayoutHelperManagerTest {
    @Rule public JniMocker mJniMocker = new JniMocker();
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TabStripSceneLayer.Natives mTabStripSceneMock;
    @Mock private TabStripSceneLayer mTabStripTreeProvider;
    @Mock private LayoutManagerHost mManagerHost;
    @Mock private LayoutUpdateHost mUpdateHost;
    @Mock private LayoutRenderHost mRenderHost;
    @Mock private ObservableSupplierImpl<LayerTitleCache> mLayerTitleCacheSupplier;
    @Mock private ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock private MultiInstanceManager mMultiInstanceManager;
    @Mock private View mToolbarContainerView;
    @Mock private DragAndDropDelegate mDragDropDelegate;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private ObservableSupplierImpl<TabModel> mTabModelSupplier;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabModelFilterProvider mTabModelFilterProvider;
    @Mock private TabModel mStandardTabModel;
    @Mock private Profile mProfile;
    @Mock private Tab mSelectedTab;
    @Mock private StripLayoutTab mHoveredStripTab;
    @Mock private ViewStub mTabHoverCardViewStub;
    @Mock private ObservableSupplierImpl<TabContentManager> mTabContentManagerSupplier;
    @Mock private BrowserControlsStateProvider mBrowserControlStateProvider;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ToolbarManager mToolbarManager;
    @Mock private StatusBarColorController mStatusBarColorController;
    @Mock private DesktopWindowStateProvider mDesktopWindowStateProvider;
    @Mock private ActionConfirmationManager mActionConfirmationManager;
    @Mock private DataSharingTabManager mDataSharingTabManager;
    @Captor private ArgumentCaptor<List<Rect>> mSystemExclusionRectCaptor;

    private StripLayoutHelperManager mStripLayoutHelperManager;
    private Activity mActivity;
    private ObservableSupplierImpl<TabModelStartupInfo> mTabModelStartupInfoSupplier;
    private ObservableSupplierImpl<Integer> mTabStripHeightSupplier;
    private int mToolbarPrimaryColor;
    private static final float SCREEN_WIDTH = 800.f;
    private static final float SCREEN_HEIGHT = 1600.f;
    private static final float VISIBLE_VIEWPORT_Y = 200.f;
    private static final int ORIENTATION = 2;
    private static final float BUTTON_END_PADDING = 8.f;
    private static final int TAB_STRIP_HEIGHT_PX = 40;
    private static final int FADE_TRANSITION_DURATION_MS = 200;

    @Before
    public void beforeTest() {
        mJniMocker.mock(TabStripSceneLayerJni.TEST_HOOKS, mTabStripSceneMock);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        TabStripSceneLayer.setTestFlag(true);

        when(mToolbarContainerView.getContext()).thenReturn(mActivity);
        when(mToolbarManager.getStatusBarColorController()).thenReturn(mStatusBarColorController);
        when(mDesktopWindowStateProvider.isInUnfocusedDesktopWindow()).thenReturn(false);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));

        initializeTest();
    }

    @After
    public void tearDown() {
        TabStripSceneLayer.setTestFlag(false);
    }

    private void initializeTest() {
        when(mTabModelFilterProvider.getTabModelFilter(anyBoolean()))
                .thenReturn(mTabGroupModelFilter);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mStandardTabModel);
        when(mTabModelSelector.getTabModelFilterProvider()).thenReturn(mTabModelFilterProvider);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mStandardTabModel);
        when(mTabModelSelector.getCurrentTabModelSupplier()).thenReturn(mTabModelSupplier);
        when(mStandardTabModel.getProfile()).thenReturn(mProfile);

        mTabModelStartupInfoSupplier = new ObservableSupplierImpl<>();

        mTabStripHeightSupplier = new ObservableSupplierImpl<>();
        mTabStripHeightSupplier.set(TAB_STRIP_HEIGHT_PX);
        mToolbarPrimaryColor = SemanticColorUtils.getToolbarBackgroundPrimary(mActivity);
        when(mToolbarManager.getTabStripHeightSupplier()).thenReturn(mTabStripHeightSupplier);
        when(mToolbarManager.getPrimaryColor()).thenReturn(mToolbarPrimaryColor);

        mStripLayoutHelperManager =
                new StripLayoutHelperManager(
                        mActivity,
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
                        mWindowAndroid,
                        mToolbarManager,
                        mDesktopWindowStateProvider,
                        mActionConfirmationManager,
                        mDataSharingTabManager);
        mStripLayoutHelperManager.setTabModelSelector(mTabModelSelector, mTabCreatorManager);
        mStripLayoutHelperManager.disableAnimationsForTesting();
    }

    @Test
    public void testGetBackgroundColor_ActivityFocusChange_TsloDisabled() {
        ToolbarFeatures.setIsTabStripLayoutOptimizationEnabledForTesting(false);
        assertEquals(
                "Initial strip background color is incorrect.",
                ChromeColors.getSurfaceColor(mActivity, R.dimen.default_elevation_3),
                mStripLayoutHelperManager.getBackgroundColor());
        // Assume the current activity lost focus.
        mStripLayoutHelperManager.onTopResumedActivityChanged(false);
        assertEquals(
                "Strip background color should not be updated when activity focus state changes.",
                ChromeColors.getSurfaceColor(mActivity, R.dimen.default_elevation_3),
                mStripLayoutHelperManager.getBackgroundColor());
    }

    @Test
    public void testGetBackgroundColor_ActivityFocusChange_NotInDesktopWindow() {
        ToolbarFeatures.setIsTabStripLayoutOptimizationEnabledForTesting(true);
        when(mDesktopWindowStateProvider.getAppHeaderState()).thenReturn(new AppHeaderState());
        assertEquals(
                "Initial strip background color is incorrect.",
                ChromeColors.getSurfaceColor(mActivity, R.dimen.default_elevation_3),
                mStripLayoutHelperManager.getBackgroundColor());
        // Assume the current activity lost focus.
        mStripLayoutHelperManager.onTopResumedActivityChanged(false);
        assertEquals(
                "Strip background color should not be updated when activity focus state changes"
                        + " while not in desktop window.",
                ChromeColors.getSurfaceColor(mActivity, R.dimen.default_elevation_3),
                mStripLayoutHelperManager.getBackgroundColor());
    }

    @Test
    public void testGetBackgroundColor_ActivityFocusChange_LightTheme() {
        ToolbarFeatures.setIsTabStripLayoutOptimizationEnabledForTesting(true);
        initializeTest();
        doTestBackgroundColorOnActivityFocusChange(
                /* isNightMode= */ false, /* isIncognito= */ false);
    }

    @Test
    @Config(qualifiers = "night")
    public void testGetBackgroundColor_ActivityFocusChange_DarkTheme() {
        ToolbarFeatures.setIsTabStripLayoutOptimizationEnabledForTesting(true);
        initializeTest();
        doTestBackgroundColorOnActivityFocusChange(
                /* isNightMode= */ true, /* isIncognito= */ false);
    }

    @Test
    public void testGetBackgroundColor_ActivityFocusChange_Incognito() {
        ToolbarFeatures.setIsTabStripLayoutOptimizationEnabledForTesting(true);
        initializeTest();
        mStripLayoutHelperManager.setIsIncognitoForTesting(true);
        doTestBackgroundColorOnActivityFocusChange(
                /* isNightMode= */ false, /* isIncognito= */ true);
    }

    private void doTestBackgroundColorOnActivityFocusChange(
            boolean isNightMode, boolean isIncognito) {
        var appHeaderState = Mockito.mock(AppHeaderState.class);
        doReturn(true).when(appHeaderState).isInDesktopWindow();
        when(mDesktopWindowStateProvider.getAppHeaderState()).thenReturn(appHeaderState);
        @ColorInt
        int focusedColor =
                ChromeColors.getSurfaceColor(
                        mActivity,
                        isNightMode ? R.dimen.default_elevation_2 : R.dimen.default_elevation_3);
        @ColorInt
        int unfocusedColor =
                ChromeColors.getSurfaceColor(
                        mActivity,
                        isNightMode ? R.dimen.default_elevation_1 : R.dimen.default_elevation_2);

        if (isIncognito) {
            focusedColor = mActivity.getColor(R.color.default_bg_color_dark_elev_2_baseline);
            unfocusedColor = mActivity.getColor(R.color.default_bg_color_dark_elev_1_baseline);
        }

        // Initially use the default tab strip background.
        assertEquals(
                "Initial strip background color is incorrect.",
                focusedColor,
                mStripLayoutHelperManager.getBackgroundColor());
        // Assume the current activity lost focus.
        mStripLayoutHelperManager.onTopResumedActivityChanged(false);
        assertEquals(
                "Strip background color should be updated when activity focus state changes to"
                        + " false.",
                unfocusedColor,
                mStripLayoutHelperManager.getBackgroundColor());
        // Assume the current activity gained focus.
        mStripLayoutHelperManager.onTopResumedActivityChanged(true);
        assertEquals(
                "Strip background color should be updated when activity focus state changes to"
                        + " true.",
                focusedColor,
                mStripLayoutHelperManager.getBackgroundColor());
    }

    @Test
    public void testGetBackgroundColor_ActivityStartsInUnfocusedDesktopWindow() {
        // Assume that the app starts in an unfocused desktop window.
        var appHeaderState =
                new AppHeaderState(new Rect(), new Rect(), /* isInDesktopWindow= */ true);
        when(mDesktopWindowStateProvider.getAppHeaderState()).thenReturn(appHeaderState);
        when(mDesktopWindowStateProvider.isInUnfocusedDesktopWindow()).thenReturn(true);
        initializeTest();

        @ColorInt
        int unfocusedLightThemeColor =
                ChromeColors.getSurfaceColor(mActivity, R.dimen.default_elevation_2);
        assertEquals(
                "Strip background color is incorrect.",
                unfocusedLightThemeColor,
                mStripLayoutHelperManager.getBackgroundColor());
    }

    @Test
    public void testUpdateForeGroundColor() {
        initializeTest();

        mStripLayoutHelperManager.onAppHeaderStateChanged(new AppHeaderState());

        int normalColor = TabUiThemeUtil.getTabStripBackgroundColor(mActivity, false);
        verify(mDesktopWindowStateProvider).updateForegroundColor(normalColor);

        Mockito.reset(mDesktopWindowStateProvider);
        mStripLayoutHelperManager.setIsIncognitoForTesting(true);
        mStripLayoutHelperManager.onAppHeaderStateChanged(new AppHeaderState());

        int incognitoColor = TabUiThemeUtil.getTabStripBackgroundColor(mActivity, true);
        verify(mDesktopWindowStateProvider).updateForegroundColor(incognitoColor);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STRIP_LAYOUT_OPTIMIZATION)
    public void testModelSelectorButtonDrawX() {
        // Set model selector button position.
        mStripLayoutHelperManager.setModelSelectorButtonVisibleForTesting(true);
        mStripLayoutHelperManager.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, VISIBLE_VIEWPORT_Y, ORIENTATION);

        // Verify model selector button x-position.
        // stripWidth(800) - buttonEndPadding(8) - MsbWidth(32) = 760
        assertEquals(
                "Model selector button x-position is not as expected",
                760.f,
                mStripLayoutHelperManager.getModelSelectorButton().getDrawX(),
                0.0);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STRIP_LAYOUT_OPTIMIZATION)
    public void testModelSelectorButtonDrawX_Rtl() {
        // Set model selector button position.
        LocalizationUtils.setRtlForTesting(true);
        mStripLayoutHelperManager.setModelSelectorButtonVisibleForTesting(true);
        mStripLayoutHelperManager.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, VISIBLE_VIEWPORT_Y, ORIENTATION);

        // Verify model selector button position.
        assertEquals(
                "Model selector button x-position is not as expected",
                BUTTON_END_PADDING,
                mStripLayoutHelperManager.getModelSelectorButton().getDrawX(),
                0.0);
    }

    @Test
    public void testModelSelectorButtonDrawY() {
        // Set model selector button position.
        mStripLayoutHelperManager.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, VISIBLE_VIEWPORT_Y, ORIENTATION);

        // Verify model selector button y-position.
        assertEquals(
                "Model selector button y-position is not as expected",
                3.f,
                mStripLayoutHelperManager.getModelSelectorButton().getDrawY(),
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

        @ColorInt
        int hoverBackgroundDefaultColor =
                ColorUtils.setAlphaComponent(
                        SemanticColorUtils.getDefaultTextColor(mActivity), (int) (0.08 * 255));
        assertEquals(
                "Model selector button hover highlight default tint is not as expected",
                hoverBackgroundDefaultColor,
                msb.getBackgroundTint());

        // Verify model selector button hover highlight pressed tint.
        when(msb.isPressed()).thenReturn(true);
        when(msb.isHovered()).thenReturn(false);
        when(msb.isPressedFromMouse()).thenReturn(true);
        @ColorInt
        int hoverBackgroundPressedColor =
                ColorUtils.setAlphaComponent(
                        SemanticColorUtils.getDefaultTextColor(mActivity), (int) (0.12 * 255));
        assertEquals(
                "Model selector button hover highlight pressed tint is not as expected",
                hoverBackgroundPressedColor,
                msb.getBackgroundTint());
        when(msb.isPressed()).thenReturn(false);

        // Verify model selector button incognito hover highlight default tint.
        when(msb.isHovered()).thenReturn(true);
        when(msb.isIncognito()).thenReturn(true);
        @ColorInt
        int hoverBackgroundDefaultIncognitoColor =
                ColorUtils.setAlphaComponent(
                        mActivity.getColor(R.color.tab_strip_button_hover_bg_color),
                        (int) (0.08 * 255));
        assertEquals(
                "Model selector button hover highlight pressed tint is not as expected",
                hoverBackgroundDefaultIncognitoColor,
                msb.getBackgroundTint());

        // Verify model selector button incognito hover highlight pressed tint.
        when(msb.isPressed()).thenReturn(true);
        when(msb.isHovered()).thenReturn(false);
        when(msb.isPressedFromMouse()).thenReturn(true);
        @ColorInt
        int hoverBackgroundPressedIncognitoColor =
                ColorUtils.setAlphaComponent(
                        mActivity.getColor(R.color.tab_strip_button_hover_bg_color),
                        (int) (0.12 * 255));
        assertEquals(
                "Model selector button hover highlight pressed tint is not as expected",
                hoverBackgroundPressedIncognitoColor,
                msb.getBackgroundTint());
    }

    @Test
    @Feature("Advanced Peripherals Support")
    public void testModelSelectorButtonHoverEnter() {
        mStripLayoutHelperManager.setModelSelectorButtonVisibleForTesting(true);

        int x = (int) mStripLayoutHelperManager.getModelSelectorButton().getDrawX();
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
    public void testModelSelectorButtonHoverOnDown() {
        mStripLayoutHelperManager.setModelSelectorButtonVisibleForTesting(true);

        // Verify model selector button is in pressed state, not hover state, when click is from
        // mouse.
        mStripLayoutHelperManager.simulateOnDownForTesting(
                mStripLayoutHelperManager.getModelSelectorButton().getDrawX() + 1, 0, true, 1);
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
    public void testFadeDrawable_Left_Rtl_ModelSelectorButtonVisible() {
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
    public void testFadeDrawable_Left_Rtl() {
        // setup
        LocalizationUtils.setRtlForTesting(true);

        // Verify fade drawable.
        assertEquals(
                "Fade drawable resource is not as expected",
                R.drawable.tab_strip_fade_medium,
                mStripLayoutHelperManager.getLeftFadeDrawable());
    }

    @Test
    public void testFadeDrawable_Right_Rtl() {
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
                AppCompatResources.getColorStateList(
                                mActivity, R.color.default_icon_color_tint_list)
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
        ToolbarFeatures.setIsTabStripLayoutOptimizationEnabledForTesting(true);
        initializeTest();
        // Setup and stub required mocks.
        int hoveredTabId = 1;
        int selectedTabId = 2;
        mStripLayoutHelperManager.setTabStripTreeProviderForTesting(mTabStripTreeProvider);

        when(mStandardTabModel.index()).thenReturn(selectedTabId);
        when(mStandardTabModel.getTabAt(selectedTabId)).thenReturn(mSelectedTab);
        when(mSelectedTab.getId()).thenReturn(selectedTabId);

        when(mHoveredStripTab.getTabId()).thenReturn(hoveredTabId);
        var activeLayoutHelper = mStripLayoutHelperManager.getActiveStripLayoutHelper();
        activeLayoutHelper.setLastHoveredTabForTesting(mHoveredStripTab);

        // Update the paddings.
        int leftPadding = 10;
        int rightPadding = 20;
        int topPaddingPx = 5;
        var appHeaderState =
                new AppHeaderState(
                        new Rect(0, 0, (int) SCREEN_WIDTH, (int) SCREEN_HEIGHT),
                        new Rect(
                                leftPadding,
                                0,
                                (int) (SCREEN_WIDTH - rightPadding),
                                TAB_STRIP_HEIGHT_PX + topPaddingPx),
                        true);
        mStripLayoutHelperManager.onAppHeaderStateChanged(appHeaderState);
        mStripLayoutHelperManager.onHeightChanged(TAB_STRIP_HEIGHT_PX + topPaddingPx);

        // Invoke the method.
        mStripLayoutHelperManager.getUpdatedSceneOverlayTree(
                new RectF(), new RectF(), mRenderHost.getResourceManager(), 0f);

        // Verify the call to #pushAndUpdateStrip.
        verify(mTabStripTreeProvider)
                .pushAndUpdateStrip(
                        eq(mStripLayoutHelperManager),
                        any(),
                        any(),
                        any(),
                        any(),
                        eq(0f),
                        eq(selectedTabId),
                        eq(hoveredTabId),
                        anyInt(),
                        anyFloat(),
                        eq((float) leftPadding),
                        eq((float) rightPadding),
                        eq((float) topPaddingPx));
    }

    @Test
    @Config(sdk = VERSION_CODES.R)
    public void testDragDropInstances_Success() {
        enableMultiInstance();
        initializeTest();
        assertNotNull(
                "Tab drag source should be set.",
                mStripLayoutHelperManager.getTabDragSourceForTesting());
    }

    @Test
    @Config(sdk = VERSION_CODES.Q)
    public void testDragDropInstances_MultiInstanceNotEnabled_ReturnsNull() {
        initializeTest();
        assertNull(
                "Tab drag source should not be set.",
                mStripLayoutHelperManager.getTabDragSourceForTesting());
    }

    @Test
    @Config(sdk = VERSION_CODES.S)
    public void testGetDragListener() {
        enableMultiInstance();
        initializeTest();
        assertNotNull("DragListener should be set.", mStripLayoutHelperManager.getDragListener());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STRIP_LAYOUT_OPTIMIZATION)
    public void testTabStripHeightTransition_Hide() {
        mStripLayoutHelperManager.setTabStripTreeProviderForTesting(mTabStripTreeProvider);

        // Call without tab strip transition.
        float yOffset = 10;
        mStripLayoutHelperManager.getUpdatedSceneOverlayTree(
                new RectF(), new RectF(), mRenderHost.getResourceManager(), yOffset);
        verify(mTabStripTreeProvider)
                .pushAndUpdateStrip(
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        /* yOffset= */ eq(yOffset),
                        anyInt(),
                        anyInt(),
                        eq(mToolbarPrimaryColor),
                        /* scrimOpacity= */ eq(0f),
                        anyFloat(),
                        anyFloat(),
                        anyFloat());

        // With tab strip transition, the yOffset will be forced to be 0.
        mTabStripHeightSupplier.set(0);
        mStripLayoutHelperManager.onHeightChanged(0);
        float progress = 0.75f; // 1 - yOffset / TAB_STRIP_HEIGHT = 1 - 10 / 40 = 0.75f
        float expectedOpacity =
                StripLayoutHelperManager.TAB_STRIP_TRANSITION_INTERPOLATOR.getInterpolation(
                        progress);
        mStripLayoutHelperManager.getUpdatedSceneOverlayTree(
                new RectF(), new RectF(), mRenderHost.getResourceManager(), yOffset);
        verify(mTabStripTreeProvider)
                .pushAndUpdateStrip(
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        /* yOffset= */ eq(0f),
                        anyInt(),
                        anyInt(),
                        eq(mToolbarPrimaryColor),
                        /* scrimOpacity= */ eq(expectedOpacity),
                        anyFloat(),
                        anyFloat(),
                        anyFloat());

        // With tab strip transition finished, the yOffset will be forced to be the negative of the
        // tab strip height.
        mStripLayoutHelperManager.onHeightTransitionFinished();
        mStripLayoutHelperManager.getUpdatedSceneOverlayTree(
                new RectF(), new RectF(), mRenderHost.getResourceManager(), yOffset);
        verify(mTabStripTreeProvider)
                .pushAndUpdateStrip(
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        /* yOffset= */ eq(yOffset - TAB_STRIP_HEIGHT_PX),
                        anyInt(),
                        anyInt(),
                        eq(mToolbarPrimaryColor),
                        /* scrimOpacity= */ eq(0f),
                        anyFloat(),
                        anyFloat(),
                        anyFloat());

        // Verify StatusBarColorController method invocations.
        InOrder inOrder = Mockito.inOrder(mStatusBarColorController);
        // Invocation during the transition.
        inOrder.verify(mStatusBarColorController)
                .setTabStripColorOverlay(mToolbarPrimaryColor, expectedOpacity);
        // Invocation after the transition finished.
        inOrder.verify(mStatusBarColorController).setTabStripHiddenOnTablet(true);
        inOrder.verify(mStatusBarColorController)
                .setTabStripColorOverlay(ScrimProperties.INVALID_COLOR, 0f);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STRIP_LAYOUT_OPTIMIZATION)
    public void testTabStripHeightTransition_Show() {
        doTestTabStripHeightTransition_Show(mToolbarPrimaryColor);
    }

    @Test
    public void testGetVirtualViews() {
        List<VirtualView> views = new ArrayList<>();
        mStripLayoutHelperManager.getVirtualViews(views);
        assertFalse("Views are not empty during regular mode.", views.isEmpty());
    }

    @Test
    public void testGetVirtualViews_TabSwitcher() {
        List<VirtualView> views = new ArrayList<>();
        mStripLayoutHelperManager
                .getTabSwitcherObserver()
                .onFinishedShowing(LayoutType.TAB_SWITCHER);
        mStripLayoutHelperManager.getVirtualViews(views);
        assertTrue("Views are empty when tab switcher is showing.", views.isEmpty());

        mStripLayoutHelperManager.getTabSwitcherObserver().onStartedHiding(LayoutType.TAB_SWITCHER);
        mStripLayoutHelperManager.getVirtualViews(views);
        assertFalse("Views are not empty after tab switcher is hiding.", views.isEmpty());
    }

    @Test
    public void testGetVirtualViews_BrowserControlsOffset() {
        List<VirtualView> views = new ArrayList<>();
        doReturn(-1).when(mBrowserControlStateProvider).getTopControlOffset();
        mStripLayoutHelperManager.getVirtualViews(views);
        assertTrue("Views empty when browser controls partially visible.", views.isEmpty());
    }

    @Test
    public void testGetVirtualViews_TabStripHeightTransition() {
        List<VirtualView> views = new ArrayList<>();
        mStripLayoutHelperManager.setIsTabStripHidden(true);
        mStripLayoutHelperManager.getVirtualViews(views);
        assertTrue("Views are empty when tab strip hidden.", views.isEmpty());
        verify(mStatusBarColorController).setTabStripHiddenOnTablet(true);

        mStripLayoutHelperManager.setIsTabStripHidden(false);
        mStripLayoutHelperManager.onHeightChanged(40);
        mStripLayoutHelperManager.getVirtualViews(views);
        assertTrue("Views are empty during tab strip transition.", views.isEmpty());
        // Invoked once by #setIsTabStripHidden(), once by #onHeightChanged().
        verify(mStatusBarColorController, times(2)).setTabStripHiddenOnTablet(false);

        mStripLayoutHelperManager.onHeightTransitionFinished();
        mStripLayoutHelperManager.getVirtualViews(views);
        assertFalse("Views are not empty after tab strip transition.", views.isEmpty());
    }

    @Test
    public void testCalculateScrimOpacityDuringTransition_Show() {
        // Test hide->show transition with simulated values.
        mStripLayoutHelperManager.onHeightChanged(TAB_STRIP_HEIGHT_PX);
        float actual = mStripLayoutHelperManager.calculateScrimOpacityDuringHeightTransition(20f);
        float expected =
                StripLayoutHelperManager.TAB_STRIP_TRANSITION_INTERPOLATOR.getInterpolation(0.5f);
        assertEquals(expected, actual, 0f);
        actual = mStripLayoutHelperManager.calculateScrimOpacityDuringHeightTransition(30f);
        expected =
                StripLayoutHelperManager.TAB_STRIP_TRANSITION_INTERPOLATOR.getInterpolation(0.25f);
        assertEquals(expected, actual, 0f);
        // If an unexpected source happened to update the compositor frame during strip transition
        // when the yOffset=0, ignore this update.
        actual = mStripLayoutHelperManager.calculateScrimOpacityDuringHeightTransition(0f);
        assertEquals(expected, actual, 0f);
        mStripLayoutHelperManager.onHeightTransitionFinished();
    }

    @Test
    public void testCalculateScrimOpacityDuringTransition_Hide() {
        // Test show->hide transition with simulated values.
        mStripLayoutHelperManager.onHeightChanged(0);
        float actual = mStripLayoutHelperManager.calculateScrimOpacityDuringHeightTransition(30f);
        float expected =
                StripLayoutHelperManager.TAB_STRIP_TRANSITION_INTERPOLATOR.getInterpolation(0.25f);
        assertEquals(expected, actual, 0f);
        actual = mStripLayoutHelperManager.calculateScrimOpacityDuringHeightTransition(20f);
        expected =
                StripLayoutHelperManager.TAB_STRIP_TRANSITION_INTERPOLATOR.getInterpolation(0.5f);
        assertEquals(expected, actual, 0f);
        // If an unexpected source happened to update the compositor frame during strip transition
        // when the yOffset=-10, ignore this update.
        actual = mStripLayoutHelperManager.calculateScrimOpacityDuringHeightTransition(30f);
        assertEquals(expected, actual, 0f);
        mStripLayoutHelperManager.onHeightTransitionFinished();
    }

    private void doTestTabStripHeightTransition_Show(int scrimColor) {
        // Assume tab strip is hidden from the beginning.
        mTabStripHeightSupplier.set(0);
        mStripLayoutHelperManager.onHeightChanged(0);
        mStripLayoutHelperManager.onHeightTransitionFinished();
        mStripLayoutHelperManager.setTabStripTreeProviderForTesting(mTabStripTreeProvider);

        // The yOffset will be forced to be reduced by the tab strip height to be kept invisible.
        float yOffset = -10;
        mStripLayoutHelperManager.getUpdatedSceneOverlayTree(
                new RectF(), new RectF(), mRenderHost.getResourceManager(), yOffset);
        verify(mTabStripTreeProvider)
                .pushAndUpdateStrip(
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        /* yOffset= */ eq(yOffset - TAB_STRIP_HEIGHT_PX),
                        anyInt(),
                        anyInt(),
                        eq(scrimColor),
                        /* scrimOpacity= */ eq(0f),
                        anyFloat(),
                        anyFloat(),
                        anyFloat());

        // With tab strip transition, the yOffset will be forced to be 0.
        mTabStripHeightSupplier.set(TAB_STRIP_HEIGHT_PX);
        mStripLayoutHelperManager.onHeightChanged(TAB_STRIP_HEIGHT_PX);
        float progress =
                0.25f; // 1 - (TAB_STRIP_HEIGHT+yOffset) / TAB_STRIP_HEIGHT = 1 - 30 / 40 = 0.25f
        float expectedOpacity =
                StripLayoutHelperManager.TAB_STRIP_TRANSITION_INTERPOLATOR.getInterpolation(
                        progress);
        mStripLayoutHelperManager.getUpdatedSceneOverlayTree(
                new RectF(), new RectF(), mRenderHost.getResourceManager(), yOffset);
        verify(mTabStripTreeProvider)
                .pushAndUpdateStrip(
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        /* yOffset= */ eq(0f),
                        anyInt(),
                        anyInt(),
                        eq(scrimColor),
                        /* scrimOpacity= */ eq(expectedOpacity),
                        anyFloat(),
                        anyFloat(),
                        anyFloat());

        // When transition finished while tabs strip showing, yOffset will be forwarded to cc
        // correctly.
        mStripLayoutHelperManager.onHeightTransitionFinished();
        mStripLayoutHelperManager.getUpdatedSceneOverlayTree(
                new RectF(), new RectF(), mRenderHost.getResourceManager(), yOffset);
        verify(mTabStripTreeProvider)
                .pushAndUpdateStrip(
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        /* yOffset= */ eq(yOffset),
                        anyInt(),
                        anyInt(),
                        eq(scrimColor),
                        /* scrimOpacity= */ eq(0f),
                        anyFloat(),
                        anyFloat(),
                        anyFloat());

        // Verify StatusBarColorController method invocations.
        InOrder inOrder = Mockito.inOrder(mStatusBarColorController);
        // Invocations before the transition started.
        inOrder.verify(mStatusBarColorController).setTabStripHiddenOnTablet(true);
        inOrder.verify(mStatusBarColorController)
                .setTabStripColorOverlay(ScrimProperties.INVALID_COLOR, 0f);
        // Invocations during the transition.
        inOrder.verify(mStatusBarColorController).setTabStripHiddenOnTablet(false);
        inOrder.verify(mStatusBarColorController)
                .setTabStripColorOverlay(scrimColor, expectedOpacity);
        // Invocation after the transition finished.
        inOrder.verify(mStatusBarColorController)
                .setTabStripColorOverlay(ScrimProperties.INVALID_COLOR, 0f);
    }

    @Test
    public void testTouchEventsIgnoredOnPaddings() {
        // Update the size and paddings.
        int leftPadding = 10;
        int rightPadding = 20;
        int topPadding = 5;
        int newHeight = TAB_STRIP_HEIGHT_PX + topPadding;
        var appHeaderState =
                new AppHeaderState(
                        new Rect(0, 0, (int) SCREEN_WIDTH, (int) SCREEN_HEIGHT),
                        new Rect(
                                leftPadding,
                                0,
                                (int) (SCREEN_WIDTH - rightPadding),
                                TAB_STRIP_HEIGHT_PX),
                        true);
        mStripLayoutHelperManager.onAppHeaderStateChanged(appHeaderState);
        mStripLayoutHelperManager.onHeightChanged(newHeight);
        mStripLayoutHelperManager.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, VISIBLE_VIEWPORT_Y, ORIENTATION);

        float yCenterOfStrip = newHeight / 2f;
        assertFalse("Event on paddings should be ignored.", motionEventHandled(0, yCenterOfStrip));
        assertFalse("Event on paddings should be ignored.", motionEventHandled(1, yCenterOfStrip));
        assertFalse(
                "Event on margins should be ignored.",
                motionEventHandled(leftPadding - 1, yCenterOfStrip));
        assertTrue(
                "Event not on margins should be handled.",
                motionEventHandled(leftPadding, yCenterOfStrip));

        assertFalse(
                "Event on margins should be ignored.",
                motionEventHandled(SCREEN_WIDTH, yCenterOfStrip));
        assertFalse(
                "Event on margins should be ignored.",
                motionEventHandled(SCREEN_WIDTH - 1, yCenterOfStrip));
        assertFalse(
                "Event on margins should be ignored.",
                motionEventHandled(SCREEN_WIDTH - rightPadding, yCenterOfStrip));
        assertTrue(
                "Event not on margins should be handled.",
                motionEventHandled(SCREEN_WIDTH - rightPadding - 1, yCenterOfStrip));
    }

    @Test
    public void testTopPadding() {
        ToolbarFeatures.setIsTabStripLayoutOptimizationEnabledForTesting(true);
        initializeTest();
        int topPadding = 10;
        mStripLayoutHelperManager.onHeightChanged(TAB_STRIP_HEIGHT_PX + topPadding);
        mStripLayoutHelperManager.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, VISIBLE_VIEWPORT_Y, ORIENTATION);

        assertFalse(
                "Event on top padding should not be handled.",
                motionEventHandled(SCREEN_WIDTH / 2, 0));
        assertFalse(
                "Event on top padding should not be handled.",
                motionEventHandled(SCREEN_WIDTH / 2, topPadding - 1));
        assertTrue(
                "Event should be handled below top padding.",
                motionEventHandled(SCREEN_WIDTH / 2, topPadding));
        assertTrue(
                "Ensure top padding increase the entire height",
                motionEventHandled(SCREEN_WIDTH / 2, topPadding + TAB_STRIP_HEIGHT_PX - 1));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.Q)
    public void testUpdateTouchableAreas_WithModelSelectorButton_StripVisible() {
        doTestUpdateTouchableAreas_WithModelSelectorButton(/* showStrip= */ true);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.Q)
    public void testUpdateTouchableAreas_WithModelSelectorButton_StripInvisible() {
        doTestUpdateTouchableAreas_WithModelSelectorButton(/* showStrip= */ false);
    }

    private void doTestUpdateTouchableAreas_WithModelSelectorButton(boolean showStrip) {
        int leftPadding = 10;
        int rightPadding = 20;
        int topPadding = 5;
        var appHeaderState =
                new AppHeaderState(
                        new Rect(0, 0, (int) SCREEN_WIDTH, (int) SCREEN_HEIGHT),
                        new Rect(
                                leftPadding,
                                0,
                                (int) (SCREEN_WIDTH - rightPadding),
                                TAB_STRIP_HEIGHT_PX + topPadding),
                        true);
        ToolbarFeatures.setIsTabStripLayoutOptimizationEnabledForTesting(true);
        initializeTest();

        // Ensure incognito icon is showing.
        mStripLayoutHelperManager.setModelSelectorButtonVisibleForTesting(true);

        mStripLayoutHelperManager.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, VISIBLE_VIEWPORT_Y, ORIENTATION);
        mStripLayoutHelperManager.onAppHeaderStateChanged(appHeaderState);
        mStripLayoutHelperManager.onHeightChanged(TAB_STRIP_HEIGHT_PX + topPadding);

        float startOpacity = showStrip ? 1f : 0f;
        float endOpacity = showStrip ? 0f : 1f;
        mStripLayoutHelperManager.onFadeTransitionRequested(
                startOpacity, endOpacity, FADE_TRANSITION_DURATION_MS);

        mStripLayoutHelperManager.updateOverlay(0, 0);

        verify(mToolbarContainerView)
                .setSystemGestureExclusionRects(mSystemExclusionRectCaptor.capture());

        if (showStrip) {
            assertEquals(
                    "Number of exclusion rects is wrong.",
                    2,
                    mSystemExclusionRectCaptor.getValue().size());

            Rect rect = mSystemExclusionRectCaptor.getValue().get(0);
            assertEquals("rect.top should be the top padding of the strip.", topPadding, rect.top);
            assertEquals(
                    "rect.bottom should be the height of the strip.",
                    TAB_STRIP_HEIGHT_PX + topPadding,
                    rect.bottom);

            Rect rect2 = mSystemExclusionRectCaptor.getValue().get(1);
            // Left: 728 = width(800) - rightPadding(20) - modelSelectorWidth(32) - endPadding(8) -
            // clickSlop(12)
            // Top: 5 = max(topPadding(5) , topPadding(5) + modelSelectorYOffset(3) -
            // clickSlop(12)))
            // Right: 784 =  width(800) - rightPadding(20) - endPadding(8) + clickSlop(12)
            // Bottom: 45 = min(height(45),  topPadding(5) + modelSelectorHeight(32) +
            // clickSlop(12))
            assertEquals(
                    "2nd rect should represent model selector button.",
                    new Rect(728, 5, 784, 45),
                    rect2);
        } else {
            assertEquals(
                    "Number of exclusion rects is wrong.",
                    1,
                    mSystemExclusionRectCaptor.getValue().size());

            Rect rect = mSystemExclusionRectCaptor.getValue().get(0);
            assertEquals("rect.left should be 0.", 0, rect.left);
            assertEquals("rect.top should be 0.", 0, rect.top);
            assertEquals("rect.right should be 0.", 0, rect.right);
            assertEquals("rect.bottom should be 0.", 0, rect.bottom);
        }
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.Q)
    public void testUpdateTouchableAreas_NoModelSelectorButton() {
        int leftPadding = 10;
        int rightPadding = 20;
        int topPadding = 5;
        var appHeaderState =
                new AppHeaderState(
                        new Rect(0, 0, (int) SCREEN_WIDTH, (int) SCREEN_HEIGHT),
                        new Rect(
                                leftPadding,
                                0,
                                (int) (SCREEN_WIDTH - rightPadding),
                                TAB_STRIP_HEIGHT_PX + topPadding),
                        true);
        ToolbarFeatures.setIsTabStripLayoutOptimizationEnabledForTesting(true);
        initializeTest();

        // Ensure incognito icon is NOT showing.
        mStripLayoutHelperManager.setModelSelectorButtonVisibleForTesting(false);

        mStripLayoutHelperManager.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, VISIBLE_VIEWPORT_Y, ORIENTATION);
        mStripLayoutHelperManager.onAppHeaderStateChanged(appHeaderState);
        mStripLayoutHelperManager.onHeightChanged(TAB_STRIP_HEIGHT_PX + topPadding);
        mStripLayoutHelperManager.updateOverlay(0, 0);

        verify(mToolbarContainerView)
                .setSystemGestureExclusionRects(mSystemExclusionRectCaptor.capture());
        assertEquals(
                "Number of exclusion rects is wrong.",
                1,
                mSystemExclusionRectCaptor.getValue().size());

        Rect rect = mSystemExclusionRectCaptor.getValue().get(0);
        assertEquals("rect.top should be the top padding of the strip.", topPadding, rect.top);
        assertEquals(
                "rect.bottom should be the height of the strip.",
                TAB_STRIP_HEIGHT_PX + topPadding,
                rect.bottom);
    }

    @Test
    public void testResizeDesktopWindow() {
        // Initially resize the window to hide the strip by triggering the fade transition.
        resizeDesktopWindowAndTriggerFadeTransition(/* showStrip= */ false);
        // Simulate a size change that keeps the strip invisible without re-triggering the fade
        // transition.
        mStripLayoutHelperManager.onSizeChanged(
                SCREEN_WIDTH - 1, SCREEN_HEIGHT, VISIBLE_VIEWPORT_Y, ORIENTATION);
        // Verify that a motion event on the strip is still not handled.
        assertFalse(
                "Strip motion event should not be handled.",
                motionEventHandled(SCREEN_WIDTH / 2, TAB_STRIP_HEIGHT_PX / 2f));

        // Resize the window to show the strip by triggering the fade transition.
        resizeDesktopWindowAndTriggerFadeTransition(/* showStrip= */ true);
        // Simulate a size change that keeps the strip visible without re-triggering the fade
        // transition.
        mStripLayoutHelperManager.onSizeChanged(
                SCREEN_WIDTH + 1, SCREEN_HEIGHT, VISIBLE_VIEWPORT_Y, ORIENTATION);
        // Verify that a motion event on the strip is still handled.
        assertTrue(
                "Strip motion event should be handled.",
                motionEventHandled(SCREEN_WIDTH / 2, TAB_STRIP_HEIGHT_PX / 2f));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_STRIP_INCOGNITO_MIGRATION)
    public void testIncognitoSwitcherDisabled() {
        initializeTest();
        assertNull(
                "Incognto switcher button should not be created.",
                mStripLayoutHelperManager.getModelSelectorButton());
    }

    private void resizeDesktopWindowAndTriggerFadeTransition(boolean showStrip) {
        // Rerun initialization after setting the FF.
        ToolbarFeatures.setIsTabStripLayoutOptimizationEnabledForTesting(true);
        initializeTest();

        int leftPadding = 10;
        int rightPadding = 20;
        int topPadding = 5;
        // Simulate the |mTopPadding| update when switching to a desktop window.
        mStripLayoutHelperManager.onHeightChanged(TAB_STRIP_HEIGHT_PX + topPadding);
        // Simulate a window size change in a desktop window.
        var appHeaderState =
                new AppHeaderState(
                        new Rect(0, 0, (int) SCREEN_WIDTH, (int) SCREEN_HEIGHT),
                        new Rect(
                                leftPadding,
                                0,
                                (int) (SCREEN_WIDTH - rightPadding),
                                TAB_STRIP_HEIGHT_PX + topPadding),
                        true);
        float startOpacity = showStrip ? 1f : 0f;
        float endOpacity = showStrip ? 0f : 1f;
        mStripLayoutHelperManager.onAppHeaderStateChanged(appHeaderState);
        mStripLayoutHelperManager.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, VISIBLE_VIEWPORT_Y, ORIENTATION);
        mStripLayoutHelperManager.onFadeTransitionRequested(startOpacity, endOpacity, 0);

        var expectedVisibilityState =
                showStrip ? StripVisibilityState.VISIBLE : StripVisibilityState.INVISIBLE;
        assertEquals(
                "Strip visibility after fade transition is incorrect.",
                expectedVisibilityState,
                mStripLayoutHelperManager.getStripVisibilityState());
        // Verify that the correct rect is set in the motion event filter.
        RectF motionEventFilterArea =
                ((AreaMotionEventFilter) mStripLayoutHelperManager.getEventFilter())
                        .getEventAreaForTesting();
        // Motion event area should be an empty rect on an invisible strip.
        var expectedMotionEventArea =
                showStrip
                        ? new RectF(
                                leftPadding,
                                topPadding,
                                SCREEN_WIDTH - rightPadding,
                                TAB_STRIP_HEIGHT_PX + topPadding)
                        : new RectF();
        assertEquals(
                "Motion event filter area is incorrect.",
                expectedMotionEventArea,
                motionEventFilterArea);
        var yCenterOfStrip = TAB_STRIP_HEIGHT_PX / 2;
        // Verify that a motion event is handled or not, based on the strip visibility state.
        assertEquals(
                "Strip motion event handling based on strip visibility state is incorrect.",
                showStrip,
                motionEventHandled(leftPadding, yCenterOfStrip));
    }

    private boolean motionEventHandled(float x, float y) {
        MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, x, y, 0);
        return mStripLayoutHelperManager.getEventFilter().onInterceptTouchEvent(event, false);
    }
}
