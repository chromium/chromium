// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
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
import org.mockito.MockitoAnnotations;

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

@RunWith(BaseRobolectricTestRunner.class)
public class TabbedNavigationBarColorControllerUnitTest {
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
                        mEdgeToEdgeControllerObservableSupplier);
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
                mNavColorController.getNavigationBarDividerColor(false));
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
