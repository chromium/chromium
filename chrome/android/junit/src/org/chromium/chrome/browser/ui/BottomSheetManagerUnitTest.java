// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.BottomControlsLayer;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.compositor.overlay_panel.OverlayPanelManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.ExpandedSheetHelper;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;

import java.util.function.Supplier;

/** Unit tests for {@link BottomSheetManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.BOTTOM_SHEET_AS_BROWSER_CONTROLS)
public class BottomSheetManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ManagedBottomSheetController mSheetController;
    @Mock private BrowserControlsVisibilityManager mControlsVisibilityManager;
    @Mock private ExpandedSheetHelper mExpandedSheetHelper;
    @Mock private Supplier<OverlayPanelManager> mOverlayManager;
    @Mock private BottomControlsStacker mBottomControlsStacker;
    @Mock private BottomSheetContent mSheetContent;

    private final ActivityTabProvider mTabProvider = new ActivityTabProvider();
    private final org.chromium.base.supplier.SettableMonotonicObservableSupplier<Boolean>
            mOmniboxFocusStateSupplier = ObservableSuppliers.createMonotonic();
    private final SettableNullableObservableSupplier<Tab> mTabObservableSupplier =
            ObservableSuppliers.createNullable();
    private final OneshotSupplierImpl<LayoutStateProvider> mLayoutStateProviderSupplier =
            new OneshotSupplierImpl<>();

    private BottomSheetManager mBottomSheetManager;
    private BottomControlsLayer mLayer;
    private BottomSheetObserver mObserver;

    @Before
    public void setUp() {
        mBottomSheetManager =
                new BottomSheetManager(
                        mSheetController,
                        mTabProvider,
                        mControlsVisibilityManager,
                        mExpandedSheetHelper,
                        mOmniboxFocusStateSupplier,
                        mOverlayManager,
                        mLayoutStateProviderSupplier,
                        mBottomControlsStacker);

        ArgumentCaptor<BottomControlsLayer> captor =
                ArgumentCaptor.forClass(BottomControlsLayer.class);
        verify(mBottomControlsStacker).addLayer(captor.capture());
        mLayer = captor.getValue();
        mObserver = (BottomSheetObserver) mLayer;
    }

    @Test
    public void testLayerDeregistration() {
        mBottomSheetManager.onDestroy();
        verify(mBottomControlsStacker).removeLayer(mLayer);
    }

    @Test
    public void testGetHeight_actsAsBrowserControls() {
        when(mSheetController.getCurrentSheetContent()).thenReturn(mSheetContent);
        when(mSheetContent.actsAsBrowserControls()).thenReturn(true);
        when(mSheetController.getCurrentPeekHeightPx()).thenReturn(100);
        when(mSheetController.getSheetState()).thenReturn(BottomSheetController.SheetState.PEEK);

        mObserver.onSheetStateChanged(BottomSheetController.SheetState.PEEK, 0);
        assertEquals(100, mLayer.getHeight());
        assertEquals(BottomControlsStacker.LayerVisibility.VISIBLE, mLayer.getLayerVisibility());
    }

    @Test
    public void testGetHeight_notActsAsBrowserControls() {
        when(mSheetController.getCurrentSheetContent()).thenReturn(mSheetContent);
        when(mSheetContent.actsAsBrowserControls()).thenReturn(false);
        when(mSheetController.getCurrentPeekHeightPx()).thenReturn(100);
        when(mSheetController.getSheetState()).thenReturn(BottomSheetController.SheetState.PEEK);

        assertEquals(0, mLayer.getHeight());
    }

    @Test
    public void testGetHeight_hiddenState() {
        when(mSheetController.getCurrentSheetContent()).thenReturn(mSheetContent);
        when(mSheetContent.actsAsBrowserControls()).thenReturn(true);
        when(mSheetController.getCurrentPeekHeightPx()).thenReturn(100);
        when(mSheetController.getSheetState()).thenReturn(BottomSheetController.SheetState.HIDDEN);

        assertEquals(0, mLayer.getHeight());
    }

    @Test
    public void testGetHeight_isHiding() {
        when(mSheetController.getCurrentSheetContent()).thenReturn(mSheetContent);
        when(mSheetContent.actsAsBrowserControls()).thenReturn(true);
        when(mSheetController.getCurrentPeekHeightPx()).thenReturn(100);
        when(mSheetController.getSheetState()).thenReturn(BottomSheetController.SheetState.PEEK);
        when(mSheetController.isSheetHiding()).thenReturn(true);

        assertEquals(0, mLayer.getHeight());
    }

    @Test
    public void testOnBrowserControlsOffsetUpdate() {
        mLayer.onBrowserControlsOffsetUpdate(-20);
        verify(mSheetController).setBottomControlsOffset(20);
    }

    @Test
    public void testGetHeight_actsAsBrowserControls_hidden() {
        when(mSheetController.getCurrentSheetContent()).thenReturn(mSheetContent);
        when(mSheetContent.actsAsBrowserControls()).thenReturn(true);
        when(mSheetController.getSheetState()).thenReturn(BottomSheetController.SheetState.HIDDEN);

        mObserver.onSheetStateChanged(BottomSheetController.SheetState.HIDDEN, 0);
        assertEquals(0, mLayer.getHeight());
        assertEquals(BottomControlsStacker.LayerVisibility.HIDDEN, mLayer.getLayerVisibility());
    }

    @Test
    public void testGetHeight_actsAsBrowserControls_hiding() {
        when(mSheetController.getCurrentSheetContent()).thenReturn(mSheetContent);
        when(mSheetContent.actsAsBrowserControls()).thenReturn(true);
        when(mSheetController.getSheetState()).thenReturn(BottomSheetController.SheetState.PEEK);
        when(mSheetController.isSheetHiding()).thenReturn(true);

        mObserver.onSheetStateChanged(BottomSheetController.SheetState.SCROLLING, 0);
        assertEquals(0, mLayer.getHeight());
    }

    @Test
    public void testGetHeight_doesNotActAsBrowserControls() {
        when(mSheetController.getCurrentSheetContent()).thenReturn(mSheetContent);
        when(mSheetContent.actsAsBrowserControls()).thenReturn(false);

        mObserver.onSheetStateChanged(BottomSheetController.SheetState.PEEK, 0);
        assertEquals(0, mLayer.getHeight());
    }

    @Test
    public void testGetHeight_nullContent() {
        when(mSheetController.getCurrentSheetContent()).thenReturn(null);

        mObserver.onSheetStateChanged(BottomSheetController.SheetState.PEEK, 0);
        assertEquals(0, mLayer.getHeight());
    }

    @Test
    public void testGetScrollBehavior_actsAsBrowserControls() {
        when(mSheetController.getCurrentSheetContent()).thenReturn(mSheetContent);
        when(mSheetContent.actsAsBrowserControls()).thenReturn(true);

        assertEquals(
                BottomControlsStacker.LayerScrollBehavior.NEVER_SCROLL_OFF,
                mLayer.getScrollBehavior());
    }

    @Test
    public void testGetScrollBehavior_doesNotActAsBrowserControls() {
        when(mSheetController.getCurrentSheetContent()).thenReturn(mSheetContent);
        when(mSheetContent.actsAsBrowserControls()).thenReturn(false);

        assertEquals(
                BottomControlsStacker.LayerScrollBehavior.NEVER_SCROLL_OFF,
                mLayer.getScrollBehavior());
    }

    @Test
    public void testGetScrollBehavior_nullContent() {
        when(mSheetController.getCurrentSheetContent()).thenReturn(null);

        assertEquals(
                BottomControlsStacker.LayerScrollBehavior.NEVER_SCROLL_OFF,
                mLayer.getScrollBehavior());
    }

    @Test
    public void testOnSheetStateChanged_heightChanged() {
        when(mSheetController.getCurrentSheetContent()).thenReturn(mSheetContent);
        when(mSheetContent.actsAsBrowserControls()).thenReturn(true);
        when(mSheetController.getCurrentPeekHeightPx()).thenReturn(100);
        when(mSheetController.getSheetState()).thenReturn(BottomSheetController.SheetState.PEEK);

        clearInvocations(mBottomControlsStacker);
        mObserver.onSheetStateChanged(BottomSheetController.SheetState.PEEK, 0);
        verify(mBottomControlsStacker).requestLayerUpdate(false);
    }

    @Test
    public void testOnSheetContentChanged_heightChanged() {
        when(mSheetController.getCurrentSheetContent()).thenReturn(mSheetContent);
        when(mSheetContent.actsAsBrowserControls()).thenReturn(true);
        when(mSheetController.getCurrentPeekHeightPx()).thenReturn(100);
        when(mSheetController.getSheetState()).thenReturn(BottomSheetController.SheetState.PEEK);

        clearInvocations(mBottomControlsStacker);
        mObserver.onSheetContentChanged(mSheetContent);
        verify(mBottomControlsStacker).requestLayerUpdate(false);
    }
}
