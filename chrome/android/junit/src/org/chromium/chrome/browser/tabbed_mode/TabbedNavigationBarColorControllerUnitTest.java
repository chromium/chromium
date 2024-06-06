// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Color;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {TabbedNavigationBarColorControllerUnitTest.ShadowSemanticColorUtils.class})
public class TabbedNavigationBarColorControllerUnitTest {
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

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

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
    }

    @After
    public void teardown() {
        ChromeFeatureList.sNavBarColorMatchesTabBackground.setForTesting(false);
    }

    @Test
    public void testMatchTabBackgroundColor() {
        ChromeFeatureList.sNavBarColorMatchesTabBackground.setForTesting(true);
        when(mTab.getBackgroundColor()).thenReturn(Color.BLUE);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        mNavColorController.updateActiveTabForTesting();

        assertTrue(
                "Should be using tab bg color.",
                mNavColorController.getUseActiveTabColorForTesting());
        assertEquals(
                "Incorrect nav bar color.",
                Color.BLUE,
                mNavColorController.getNavigationBarColorForTesting());
        assertEquals(
                "Incorrect nav bar divider color.",
                Color.BLUE,
                mNavColorController.getNavigationBarDividerColor(false, false));
    }

    @Test
    public void testMatchBottomAttachedColor() {
        ChromeFeatureList.sNavBarColorMatchesTabBackground.setForTesting(true);
        when(mTab.getBackgroundColor()).thenReturn(Color.BLUE);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        mNavColorController.updateActiveTabForTesting();

        mNavColorController.onBottomAttachedColorChanged(Color.RED, false, false);
        assertTrue(
                "Should be using the bottom attached UI color.",
                mNavColorController.getUseBottomAttachedUiColorForTesting());
        assertEquals(
                "The nav bar color should be the bottom attached UI color.",
                Color.RED,
                mNavColorController.getNavigationBarColorForTesting());
        assertEquals(
                "The nav bar divider color should be the bottom attached UI color.",
                Color.RED,
                mNavColorController.getNavigationBarDividerColor(false, false));

        mNavColorController.onBottomAttachedColorChanged(null, false, false);
        assertFalse(
                "Should no longer be using the bottom attached UI color.",
                mNavColorController.getUseBottomAttachedUiColorForTesting());
        assertEquals(
                "The nav bar color should match the tab background.",
                Color.BLUE,
                mNavColorController.getNavigationBarColorForTesting());
        assertEquals(
                "The nav bar divider color should match the tab background.",
                Color.BLUE,
                mNavColorController.getNavigationBarDividerColor(false, false));
    }

    @Test
    public void testMatchBottomAttachedColor_forceShowDivider() {
        ChromeFeatureList.sNavBarColorMatchesTabBackground.setForTesting(true);
        when(mTab.getBackgroundColor()).thenReturn(Color.BLUE);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        mNavColorController.updateActiveTabForTesting();
        Mockito.clearInvocations(mWindow);

        mNavColorController.onBottomAttachedColorChanged(Color.RED, true, true);
        verify(mWindow, atLeastOnce()).setNavigationBarDividerColor(eq(NAV_DIVIDER_COLOR));
    }

    @Test
    public void testGetNavigationBarDividerColor() {
        ChromeFeatureList.sNavBarColorMatchesTabBackground.setForTesting(true);
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
    public void testToEdgeDoesntMatchTabBackgroundColor() {
        ChromeFeatureList.sNavBarColorMatchesTabBackground.setForTesting(true);
        when(mTab.getBackgroundColor()).thenReturn(Color.BLUE);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        when(mEdgeToEdgeController.getBottomInset()).thenReturn(100);
        mNavColorController.updateActiveTabForTesting();

        assertFalse(
                "Shouldn't be using tab background color.",
                mNavColorController.getUseActiveTabColorForTesting());
        assertEquals(
                "Incorrect nav bar color.",
                Color.TRANSPARENT,
                mNavColorController.getNavigationBarColorForTesting());
    }
}
