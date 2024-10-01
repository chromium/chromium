// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Color;
import android.os.Build;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
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
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

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
    @Mock private Window mWindow;
    @Mock private View mDecorView;
    @Mock private ViewGroup mRootView;
    private Context mContext;
    @Mock private TabModelSelector mTabModelSelector;
    private ObservableSupplierImpl<LayoutManager> mLayoutManagerSupplier;
    @Mock private LayoutManager mLayoutManager;
    @Mock private FullscreenManager mFullscreenManager;
    private ObservableSupplierImpl<EdgeToEdgeController> mEdgeToEdgeControllerObservableSupplier;
    @Mock private EdgeToEdgeController mEdgeToEdgeController;
    @Mock private BottomAttachedUiObserver mBottomAttachedUiObserver;
    @Mock private Tab mTab;
    @Mock private NavigationBarColorProvider.Observer mObserver;
    @Mock private ObservableSupplierImpl<TabModel> mTabModelSupplier;

    @Captor private ArgumentCaptor<Integer> mWindowColorCaptor;
    @Captor private ArgumentCaptor<Integer> mWindowDividerColorCaptor;
    @Captor private ArgumentCaptor<Integer> mNavigationBarColorChangedCaptor;
    @Captor private ArgumentCaptor<Integer> mNavigationBarDividerColorChangedCaptor;
    @Captor private ArgumentCaptor<Integer> mRootViewSystemVisibility;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mLayoutManagerSupplier = new ObservableSupplierImpl<>();
        mEdgeToEdgeControllerObservableSupplier = new ObservableSupplierImpl<>();

        when(mWindow.getContext()).thenReturn(mContext);
        when(mWindow.getDecorView()).thenReturn(mDecorView);
        when(mDecorView.getRootView()).thenReturn(mRootView);
        when(mRootView.getContext()).thenReturn(mContext);
        when(mTabModelSelector.getCurrentTab()).thenReturn(mTab);
        when(mTabModelSelector.getCurrentTabModelSupplier()).thenReturn(mTabModelSupplier);

        mNavColorController =
                new TabbedNavigationBarColorController(
                        mWindow,
                        mTabModelSelector,
                        mLayoutManagerSupplier,
                        mFullscreenManager,
                        mEdgeToEdgeControllerObservableSupplier,
                        mBottomAttachedUiObserver);
        mLayoutManagerSupplier.set(mLayoutManager);
        mEdgeToEdgeControllerObservableSupplier.set(mEdgeToEdgeController);
        mNavColorController.addObserver(mObserver);

        // Setup the capture after TabbedNavigationBarColorController is initialized so it does
        // not capture value during the initializations.
        runColorUpdateAnimation();
        doNothing().when(mWindow).setNavigationBarColor(mWindowColorCaptor.capture());
        doNothing().when(mWindow).setNavigationBarDividerColor(mWindowDividerColorCaptor.capture());
        doNothing()
                .when(mObserver)
                .onNavigationBarColorChanged(mNavigationBarColorChangedCaptor.capture());
        doNothing()
                .when(mObserver)
                .onNavigationBarDividerChanged(mNavigationBarDividerColorChangedCaptor.capture());
        doNothing().when(mRootView).setSystemUiVisibility(mRootViewSystemVisibility.capture());
    }

    @Test
    public void testMatchBottomAttachedColor() {
        when(mTab.getBackgroundColor()).thenReturn(Color.BLUE);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        mNavColorController.updateActiveTabForTesting();
        runColorUpdateAnimation();

        mNavColorController.onBottomAttachedColorChanged(Color.RED, false, false);
        assertTrue(
                "Should be using the bottom attached UI color.",
                mNavColorController.getUseBottomAttachedUiColorForTesting());
        assertNavBarColor(Color.RED);
        assertNavBarDividerColor(Color.RED);

        runColorUpdateAnimation();
        assertWindowNavBarColor(Color.RED);
        assertWindowNavBarDividerColor(Color.RED);

        mNavColorController.onBottomAttachedColorChanged(null, false, false);
        assertFalse(
                "Should no longer be using the bottom attached UI color.",
                mNavColorController.getUseBottomAttachedUiColorForTesting());
        assertNavBarColor(Color.BLUE);
        assertNavBarDividerColor(Color.BLUE);

        runColorUpdateAnimation();
        assertWindowNavBarColor(Color.BLUE);
        assertWindowNavBarDividerColor(Color.BLUE);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void testMatchBottomAttachedColor_toEdge() {
        when(mTab.getBackgroundColor()).thenReturn(Color.BLUE);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        when(mEdgeToEdgeController.isDrawingToEdge()).thenReturn(true);
        when(mWindow.getNavigationBarColor()).thenReturn(Color.RED);
        mNavColorController.updateActiveTabForTesting();
        runColorUpdateAnimation();

        Mockito.clearInvocations(mWindow);
        mNavColorController.onBottomAttachedColorChanged(Color.RED, false, false);
        assertTrue(
                "Should be using the bottom attached UI color.",
                mNavColorController.getUseBottomAttachedUiColorForTesting());
        assertNavBarColor(Color.RED);
        assertNavBarDividerColor(Color.RED);
        assertWindowNavBarColor(Color.TRANSPARENT);
        assertWindowNavBarDividerColor(Color.TRANSPARENT);

        Mockito.clearInvocations(mWindow);
        mNavColorController.onBottomAttachedColorChanged(null, false, false);
        assertFalse(
                "Should no longer be using the bottom attached UI color.",
                mNavColorController.getUseBottomAttachedUiColorForTesting());
        assertNavBarColor(Color.BLUE);
        assertNavBarDividerColor(Color.BLUE);
        assertWindowNavBarColor(Color.TRANSPARENT);
        assertWindowNavBarDividerColor(Color.TRANSPARENT);
    }

    @Test
    public void testMatchBottomAttachedColor_forceShowDivider() {
        when(mTab.getBackgroundColor()).thenReturn(Color.BLUE);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        mNavColorController.updateActiveTabForTesting();
        runColorUpdateAnimation();

        mNavColorController.onBottomAttachedColorChanged(Color.RED, true, true);
        runColorUpdateAnimation();
        assertWindowNavBarColor(Color.RED);
        assertWindowNavBarDividerColor(NAV_DIVIDER_COLOR);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void testMatchBottomAttachedColor_forceShowDivider_toEdge() {
        when(mTab.getBackgroundColor()).thenReturn(Color.BLUE);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        when(mEdgeToEdgeController.isDrawingToEdge()).thenReturn(true);
        mNavColorController.updateActiveTabForTesting();
        runColorUpdateAnimation();

        Mockito.clearInvocations(mWindow);
        mNavColorController.onBottomAttachedColorChanged(Color.RED, true, false);
        runColorUpdateAnimation();
        assertTrue(
                "Should be using the bottom attached UI color.",
                mNavColorController.getUseBottomAttachedUiColorForTesting());
        assertNavBarColor(Color.RED);
        assertNavBarDividerColor(NAV_DIVIDER_COLOR, true);
        assertWindowNavBarColor(Color.TRANSPARENT);
        assertWindowNavBarDividerColor(Color.TRANSPARENT);

        Mockito.clearInvocations(mWindow);
        mNavColorController.onBottomAttachedColorChanged(null, false, false);
        runColorUpdateAnimation();
        assertFalse(
                "Should no longer be using the bottom attached UI color.",
                mNavColorController.getUseBottomAttachedUiColorForTesting());
        assertNavBarColor(Color.BLUE);
        assertNavBarDividerColor(Color.BLUE);
        assertWindowNavBarColor(Color.TRANSPARENT);
        assertWindowNavBarDividerColor(Color.TRANSPARENT);
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
        assertNavBarColor(Color.BLUE);
        assertNavBarDividerColor(Color.BLUE);
        assertWindowNavBarColor(Color.BLUE);
        assertWindowNavBarDividerColor(Color.BLUE);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void testMatchTabBackgroundColor_toEdge() {
        when(mTab.getBackgroundColor()).thenReturn(Color.BLUE);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        when(mEdgeToEdgeController.getBottomInset()).thenReturn(100);
        when(mEdgeToEdgeController.isDrawingToEdge()).thenReturn(true);
        when(mWindow.getNavigationBarColor()).thenReturn(Color.RED);

        Mockito.clearInvocations(mWindow);
        mNavColorController.updateActiveTabForTesting();
        runColorUpdateAnimation();

        assertTrue(
                "Should be using tab background color for the navigation bar color.",
                mNavColorController.getUseActiveTabColorForTesting());
        assertNavBarColor(Color.BLUE);
        assertNavBarDividerColor(Color.BLUE);
        assertWindowNavBarColor(Color.TRANSPARENT);
        assertWindowNavBarDividerColor(Color.TRANSPARENT);
    }

    @Test
    public void testSetNavigationBarScrimFraction() {
        when(mTab.getBackgroundColor()).thenReturn(Color.LTGRAY);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        when(mEdgeToEdgeController.getBottomInset()).thenReturn(0);
        when(mEdgeToEdgeController.isDrawingToEdge()).thenReturn(false);
        when(mWindow.getNavigationBarColor()).thenReturn(Color.RED);

        Mockito.clearInvocations(mWindow);
        mNavColorController.updateActiveTabForTesting();
        runColorUpdateAnimation();
        assertWindowNavBarColor(Color.LTGRAY);

        mNavColorController.setNavigationBarScrimFraction(1.0f);
        // Light gray + the default scrim color overlay.
        assertWindowNavBarColor(0xFF474747);
        assertEquals(
                "The navigation bar icons should not be using dark icons for a dark navigation"
                        + " bar.",
                0,
                (mRootViewSystemVisibility.getValue() & View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR));

        mNavColorController.setNavigationBarScrimFraction(0.0f);
        assertWindowNavBarColor(Color.LTGRAY);
        assertEquals(
                "The navigation bar icons should be using dark icons for the light navigation"
                        + " bar.",
                View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR,
                (mRootViewSystemVisibility.getValue() & View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR));
    }

    private void runColorUpdateAnimation() {
        // Run the color  transition animation so color is applied to the window.
        ShadowLooper.idleMainLooper();
    }

    private void assertNavBarColor(int color) {
        assertEquals(
                "The nav bar color should match the active tab.",
                color,
                mNavColorController.getNavigationBarColorForTesting());
        assertEquals(
                "New color is not delivered to the observer.",
                color,
                (int) mNavigationBarColorChangedCaptor.getValue());
    }

    private void assertWindowNavBarColor(int color) {
        assertEquals(
                "The window (OS) nav bar color is different.",
                color,
                (int) mWindowColorCaptor.getValue());
    }

    private void assertNavBarDividerColor(int color) {
        assertNavBarDividerColor(color, /* forceShowDivider= */ false);
    }

    private void assertNavBarDividerColor(int color, boolean forceShowDivider) {
        assertEquals(
                "Incorrect nav bar divider color.",
                color,
                mNavColorController.getNavigationBarDividerColor(false, forceShowDivider));
        assertEquals(
                "New color is not delivered to the observer.",
                color,
                (int) mNavigationBarDividerColorChangedCaptor.getValue());
    }

    private void assertWindowNavBarDividerColor(int color) {
        assertEquals(
                "Incorrect divider color set to the window.",
                color,
                (int) mWindowDividerColorCaptor.getValue());
    }
}
