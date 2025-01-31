// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
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
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.NavigationBarColorProvider;
import org.chromium.components.browser_ui.edge_to_edge.EdgeToEdgeSystemBarColorHelper;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.util.ColorUtils;

@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {TabbedNavigationBarColorControllerUnitTest.ShadowSemanticColorUtils.class},
        sdk = 28)
@EnableFeatures(ChromeFeatureList.NAV_BAR_COLOR_MATCHES_TAB_BACKGROUND)
public class TabbedNavigationBarColorControllerUnitTest {
    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();

    @Implements(SemanticColorUtils.class)
    static class ShadowSemanticColorUtils {
        @Implementation
        public static int getBottomSystemNavDividerColor(Context context) {
            return NAV_DIVIDER_COLOR;
        }
    }

    private static final int NAV_DIVIDER_COLOR = Color.LTGRAY;

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
    @EnableFeatures({ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN})
    public void testOnScrimOverlapChanged() {
        when(mTab.getBackgroundColor()).thenReturn(Color.LTGRAY);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);

        // Verify the nav bar color.
        mNavColorController.onBottomAttachedColorChanged(Color.LTGRAY, false, false);
        runColorUpdateAnimation();
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarColor(eq(Color.LTGRAY));

        // Apply a scrim to the nav bar via onScrimOverlapChanged.
        @ColorInt int fullScrimColor = ColorUtils.applyAlphaFloat(Color.RED, .5f);
        mNavColorController.onScrimOverlapChanged(fullScrimColor);

        @ColorInt
        int expectedColorWithScrim = ColorUtils.overlayColor(Color.LTGRAY, fullScrimColor);

        // Verify that the scrim was properly applied to the nav bar.
        verify(mEdgeToEdgeSystemBarColorHelper).setNavigationBarColor(eq(expectedColorWithScrim));

        mNavColorController.updateActiveTabForTesting();
        runColorUpdateAnimation();

        // Verify that a subsequent call to updateNavigationBarColor doesn't remove the scrim
        // effect. setNavigationBarColor(expectedColorWithScrim) is called once via
        // onScrimOverlapChanged and once via updateActiveTabForTesting.
        verify(mEdgeToEdgeSystemBarColorHelper, times(2))
                .setNavigationBarColor(eq(expectedColorWithScrim));
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
}
