// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Color;
import android.os.Build;
import android.view.ContextThemeWrapper;

import androidx.annotation.ColorInt;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.NavigationBarColorProvider;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.edge_to_edge.EdgeToEdgeSystemBarColorHelper;
import org.chromium.ui.util.ColorUtils;

import java.util.HashSet;
import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = 29)
@DisableFeatures(ChromeFeatureList.NAV_BAR_COLOR_ANIMATION)
public class TabbedNavigationBarColorControllerUnitTest {
    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();

    private static final int NAV_DIVIDER_COLOR = Color.LTGRAY;
    private static final int NUM_UNIQUE_ANIMATION_COLORS = 5;

    private TabbedNavigationBarColorController mNavColorController;
    private Context mContext;
    @Mock private TabModelSelector mTabModelSelector;
    private ObservableSupplierImpl<LayoutManager> mLayoutManagerSupplier;
    @Mock private LayoutManager mLayoutManager;
    @Mock private FullscreenManager mFullscreenManager;
    private ObservableSupplierImpl<EdgeToEdgeController> mEdgeToEdgeControllerObservableSupplier;
    private ObservableSupplierImpl<Integer> mOverviewColorSupplier;
    @Mock private EdgeToEdgeController mEdgeToEdgeController;
    @Mock private BottomAttachedUiObserver mBottomAttachedUiObserver;
    @Mock private Tab mTab;
    @Mock private NavigationBarColorProvider.Observer mObserver;
    @Mock private ObservableSupplierImpl<TabModel> mTabModelSupplier;
    @Mock private EdgeToEdgeSystemBarColorHelper mEdgeToEdgeSystemBarColorHelper;

    @Before
    public void setUp() {
        SemanticColorUtils.setBottomSystemNavDividerColorForTesting(NAV_DIVIDER_COLOR);
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mLayoutManagerSupplier = new ObservableSupplierImpl<>();
        mEdgeToEdgeControllerObservableSupplier = new ObservableSupplierImpl<>();
        mOverviewColorSupplier = new ObservableSupplierImpl<>();

        when(mTabModelSelector.getCurrentTab()).thenReturn(mTab);
        when(mTabModelSelector.getCurrentTabModelSupplier()).thenReturn(mTabModelSupplier);

        mNavColorController =
                new TabbedNavigationBarColorController(
                        mContext,
                        mTabModelSelector,
                        mLayoutManagerSupplier,
                        mFullscreenManager,
                        mEdgeToEdgeControllerObservableSupplier,
                        mOverviewColorSupplier,
                        mEdgeToEdgeSystemBarColorHelper,
                        mBottomAttachedUiObserver);
        mLayoutManagerSupplier.set(mLayoutManager);
        mEdgeToEdgeControllerObservableSupplier.set(mEdgeToEdgeController);
        mNavColorController.addObserver(mObserver);

        runColorUpdateAnimation();
    }

    @Test
    public void testMatchBottomAttachedColor() {
        when(mTab.getBackgroundColor()).thenReturn(Color.BLUE);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        mNavColorController.updateActiveTabForTesting();
        runColorUpdateAnimation();

        Mockito.clearInvocations(mEdgeToEdgeSystemBarColorHelper);

        mNavColorController.onBottomAttachedColorChanged(Color.RED, false, false);
        assertTrue(
                "Should be using the bottom attached UI color.",
                mNavColorController.getUseBottomAttachedUiColorForTesting());
        runColorUpdateAnimation();
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarColor(eq(Color.RED));
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarDividerColor(eq(Color.RED));

        Mockito.clearInvocations(mEdgeToEdgeSystemBarColorHelper);

        mNavColorController.onBottomAttachedColorChanged(null, false, false);
        assertFalse(
                "Should no longer be using the bottom attached UI color.",
                mNavColorController.getUseBottomAttachedUiColorForTesting());

        runColorUpdateAnimation();
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarColor(eq(Color.BLUE));
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarDividerColor(eq(Color.BLUE));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void testMatchBottomAttachedColor_toEdge() {
        when(mTab.getBackgroundColor()).thenReturn(Color.BLUE);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        when(mEdgeToEdgeController.isDrawingToEdge()).thenReturn(true);
        mNavColorController.updateActiveTabForTesting();
        runColorUpdateAnimation();

        Mockito.clearInvocations(mEdgeToEdgeSystemBarColorHelper);
        mNavColorController.onBottomAttachedColorChanged(Color.RED, false, false);
        assertTrue(
                "Should be using the bottom attached UI color.",
                mNavColorController.getUseBottomAttachedUiColorForTesting());
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarColor(eq(Color.RED));
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarDividerColor(eq(Color.RED));

        Mockito.clearInvocations(mEdgeToEdgeSystemBarColorHelper);
        mNavColorController.onBottomAttachedColorChanged(null, false, false);
        assertFalse(
                "Should no longer be using the bottom attached UI color.",
                mNavColorController.getUseBottomAttachedUiColorForTesting());
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarColor(eq(Color.BLUE));
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarDividerColor(eq(Color.BLUE));
    }

    @Test
    public void testMatchBottomAttachedColor_forceShowDivider() {
        when(mTab.getBackgroundColor()).thenReturn(Color.BLUE);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        mNavColorController.updateActiveTabForTesting();
        runColorUpdateAnimation();

        Mockito.clearInvocations(mEdgeToEdgeSystemBarColorHelper);

        mNavColorController.onBottomAttachedColorChanged(Color.RED, true, true);
        runColorUpdateAnimation();
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarColor(eq(Color.RED));
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarDividerColor(eq(NAV_DIVIDER_COLOR));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void testMatchBottomAttachedColor_forceShowDivider_toEdge() {
        when(mTab.getBackgroundColor()).thenReturn(Color.BLUE);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        when(mEdgeToEdgeController.isDrawingToEdge()).thenReturn(true);
        mNavColorController.updateActiveTabForTesting();
        runColorUpdateAnimation();

        Mockito.clearInvocations(mEdgeToEdgeSystemBarColorHelper);
        mNavColorController.onBottomAttachedColorChanged(Color.RED, true, false);
        runColorUpdateAnimation();
        assertTrue(
                "Should be using the bottom attached UI color.",
                mNavColorController.getUseBottomAttachedUiColorForTesting());
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarColor(eq(Color.RED));
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarDividerColor(eq(NAV_DIVIDER_COLOR));

        Mockito.clearInvocations(mEdgeToEdgeSystemBarColorHelper);
        mNavColorController.onBottomAttachedColorChanged(null, false, false);
        runColorUpdateAnimation();
        assertFalse(
                "Should no longer be using the bottom attached UI color.",
                mNavColorController.getUseBottomAttachedUiColorForTesting());
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarColor(eq(Color.BLUE));
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarDividerColor(eq(Color.BLUE));
    }

    @Test
    public void testGetNavigationBarDividerColor() {
        assertEquals(
                "The nav bar divider color should be the bottom attached UI color.",
                NAV_DIVIDER_COLOR,
                mNavColorController.getNavigationBarDividerColor(false, true));
        assertEquals(
                "The nav bar divider color should match the tab background.",
                mContext.getColor(R.color.bottom_system_nav_divider_color_light),
                mNavColorController.getNavigationBarDividerColor(true, true));
    }

    @Test
    public void testMatchTabBackgroundColor() {
        when(mTab.getBackgroundColor()).thenReturn(Color.BLUE);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        when(mEdgeToEdgeController.getBottomInset()).thenReturn(100);
        when(mEdgeToEdgeController.isDrawingToEdge()).thenReturn(false);

        mNavColorController.updateActiveTabForTesting();
        runColorUpdateAnimation();

        assertTrue(
                "Should be using tab background color for the navigation bar color.",
                mNavColorController.getUseActiveTabColorForTesting());
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarColor(eq(Color.BLUE));
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarDividerColor(eq(Color.BLUE));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void testMatchTabBackgroundColor_toEdge() {
        when(mTab.getBackgroundColor()).thenReturn(Color.BLUE);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        when(mEdgeToEdgeController.getBottomInset()).thenReturn(100);
        when(mEdgeToEdgeController.isDrawingToEdge()).thenReturn(true);

        mNavColorController.updateActiveTabForTesting();
        runColorUpdateAnimation();

        assertTrue(
                "Should be using tab background color for the navigation bar color.",
                mNavColorController.getUseActiveTabColorForTesting());
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarColor(eq(Color.BLUE));
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarDividerColor(eq(Color.BLUE));
    }

    @Test
    public void testSetNavigationBarScrimFraction() {
        when(mTab.getBackgroundColor()).thenReturn(Color.LTGRAY);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        when(mEdgeToEdgeController.getBottomInset()).thenReturn(0);
        when(mEdgeToEdgeController.isDrawingToEdge()).thenReturn(false);

        mNavColorController.updateActiveTabForTesting();
        runColorUpdateAnimation();
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarColor(eq(Color.LTGRAY));

        @ColorInt int fullScrimColor = ColorUtils.applyAlphaFloat(Color.RED, .5f);
        mNavColorController.setNavigationBarScrimColor(fullScrimColor);
        @ColorInt
        int expectedColorWithScrim = ColorUtils.overlayColor(Color.LTGRAY, fullScrimColor);
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarColor(eq(expectedColorWithScrim));

        mNavColorController.setNavigationBarScrimColor(Color.TRANSPARENT);
        verify(mEdgeToEdgeSystemBarColorHelper, times(2)).setNavigationBarColor(eq(Color.LTGRAY));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.NAV_BAR_COLOR_ANIMATION,
        ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE
    })
    @DisableFeatures({ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN})
    @Config(sdk = 30) // Min version needed for e2e everywhere
    public void testNavBarColorAnimationsEdgeToEdgeEverywhere() {
        when(mTab.getBackgroundColor()).thenReturn(Color.BLUE);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);

        mNavColorController.updateActiveTabForTesting();
        runColorUpdateAnimation();
        // Verify that our starting nav bar color is blue.
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarColor(eq(Color.BLUE));

        Mockito.clearInvocations(mEdgeToEdgeSystemBarColorHelper);
        // Change nav bar color to red with animations enabled.
        mNavColorController.onBottomAttachedColorChanged(Color.RED, false, false);
        runColorUpdateAnimation();
        // Capture all of the animation colors.
        ArgumentCaptor<Integer> colorsArgumentCaptor = ArgumentCaptor.forClass(Integer.class);
        verify(mEdgeToEdgeSystemBarColorHelper, atLeastOnce())
                .setNavigationBarColor(colorsArgumentCaptor.capture());

        verifyColorAnimationSteps(colorsArgumentCaptor.getAllValues());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.NAV_BAR_COLOR_ANIMATION,
        ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN
    })
    @DisableFeatures({ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE})
    public void testNavBarColorAnimationsEdgeToEdgeBottomChin() {
        mNavColorController.setIsBottomChinEnabledForTesting(true);
        when(mTab.getBackgroundColor()).thenReturn(Color.BLUE);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);

        mNavColorController.updateActiveTabForTesting();
        runColorUpdateAnimation();
        // Verify that our starting nav bar color is blue.
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarColor(eq(Color.BLUE));

        Mockito.clearInvocations(mEdgeToEdgeSystemBarColorHelper);
        // Change nav bar color to red with animations enabled.
        mNavColorController.onBottomAttachedColorChanged(Color.RED, false, false);
        runColorUpdateAnimation();
        // Capture all of the animation colors.
        ArgumentCaptor<Integer> colorsArgumentCaptor = ArgumentCaptor.forClass(Integer.class);
        verify(mEdgeToEdgeSystemBarColorHelper, atLeastOnce())
                .setNavigationBarColor(colorsArgumentCaptor.capture());

        verifyColorAnimationSteps(colorsArgumentCaptor.getAllValues());
    }

    // Disable the dedicated feature flag.
    @Test
    @EnableFeatures({
        ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN,
        ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE
    })
    @DisableFeatures({ChromeFeatureList.NAV_BAR_COLOR_ANIMATION})
    @Config(sdk = 30) // Min version needed for e2e everywhere
    public void testNavBarColorAnimationsDisabled() {
        when(mTab.getBackgroundColor()).thenReturn(Color.BLUE);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);

        mNavColorController.updateActiveTabForTesting();
        runColorUpdateAnimation();
        // Verify that our starting nav bar color is blue.
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarColor(eq(Color.BLUE));

        Mockito.clearInvocations(mEdgeToEdgeSystemBarColorHelper);
        // Change nav bar color to red.
        mNavColorController.onBottomAttachedColorChanged(Color.RED, false, false);
        runColorUpdateAnimation();

        // After clearing invocations and changing the nav bar color, verify that
        // setNavigationBarColor is called exactly once with Color.RED since animations are
        // disabled.
        verify(mEdgeToEdgeSystemBarColorHelper, times(1)).setNavigationBarColor(eq(Color.RED));
        verify(mEdgeToEdgeSystemBarColorHelper, times(1)).setNavigationBarColor(anyInt());
    }

    // Disable the two cached params.
    @Test
    @EnableFeatures({
        ChromeFeatureList.NAV_BAR_COLOR_ANIMATION
                + ":disable_bottom_chin_color_animation/true/disable_edge_to_edge_layout_color_animation/true"
    })
    public void testNavBarColorAnimationsCachedParamsDisabled() {
        when(mTab.getBackgroundColor()).thenReturn(Color.BLUE);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);

        mNavColorController.updateActiveTabForTesting();
        runColorUpdateAnimation();
        // Verify that our starting nav bar color is blue.
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarColor(eq(Color.BLUE));

        Mockito.clearInvocations(mEdgeToEdgeSystemBarColorHelper);
        // Change nav bar color to red.
        mNavColorController.onBottomAttachedColorChanged(Color.RED, false, false);
        runColorUpdateAnimation();

        // After clearing invocations and changing the nav bar color, verify that
        // setNavigationBarColor is called exactly once with Color.RED since animations are
        // disabled.
        verify(mEdgeToEdgeSystemBarColorHelper, times(1)).setNavigationBarColor(eq(Color.RED));
        verify(mEdgeToEdgeSystemBarColorHelper, times(1)).setNavigationBarColor(anyInt());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.NAV_BAR_COLOR_ANIMATION,
        ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN
    })
    @DisableFeatures({ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE})
    public void testHideNavBarDuringOmniboxSwipe() {
        mNavColorController.setIsBottomChinEnabledForTesting(true);
        Mockito.clearInvocations(mEdgeToEdgeSystemBarColorHelper);

        ArgumentCaptor<LayoutStateObserver> argumentCaptor =
                ArgumentCaptor.forClass(LayoutStateObserver.class);

        // mLayoutManagerSupplier.set(mLayoutManager) in this file should trigger setLayoutManager.
        verify(mLayoutManager).addObserver(argumentCaptor.capture());

        LayoutStateObserver layoutStateObserver = argumentCaptor.getValue();

        // Simulate omnibox swipe.
        layoutStateObserver.onStartedShowing(LayoutType.TOOLBAR_SWIPE);
        runColorUpdateAnimation();

        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarColor(eq(Color.TRANSPARENT));
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarDividerColor(eq(Color.TRANSPARENT));
    }

    @Test
    public void testOverviewColorEnabled() {
        mNavColorController.enableOverviewMode();

        mOverviewColorSupplier.set(Color.BLUE);
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarColor(eq(Color.BLUE));

        mNavColorController.disableOverviewMode();
    }

    @Test
    public void testOverviewModeDisabled() {
        when(mTab.getBackgroundColor()).thenReturn(Color.LTGRAY);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        mNavColorController.updateActiveTabForTesting();

        mNavColorController.enableOverviewMode();
        mOverviewColorSupplier.set(Color.BLUE);
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarColor(eq(Color.BLUE));

        Mockito.clearInvocations(mEdgeToEdgeSystemBarColorHelper);

        mNavColorController.disableOverviewMode();
        mOverviewColorSupplier.set(Color.RED);
        // Color should reset to the tab background color.
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarColor(eq(Color.LTGRAY));
    }

    private void runColorUpdateAnimation() {
        // Run the color  transition animation so color is applied to the window.
        ShadowLooper.idleMainLooper();
    }

    private void verifyColorAnimationSteps(List<Integer> capturedColors) {
        assertTrue(
                "There should be at least five unique animation colors: the start color, the end"
                        + " color, and at least three in-between.",
                new HashSet<>(capturedColors).size() >= NUM_UNIQUE_ANIMATION_COLORS);
        assertEquals(
                "The first animation color should be blue.",
                Color.BLUE,
                (int) capturedColors.get(0));
        assertEquals(
                "The last animation color should be red.",
                Color.RED,
                (int) capturedColors.get(capturedColors.size() - 1));
    }
}
