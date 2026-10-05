// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Color;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
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
@EnableFeatures(ChromeFeatureList.BOTTOM_SHEET_AS_BROWSER_CONTROLS)
public class BottomSheetManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ManagedBottomSheetController mSheetController;
    @Mock private BrowserControlsVisibilityManager mControlsVisibilityManager;
    @Mock private ExpandedSheetHelper mExpandedSheetHelper;
    @Mock private Supplier<OverlayPanelManager> mOverlayManager;
    @Mock private BottomControlsStacker mBottomControlsStacker;
    @Mock private BottomSheetContent mSheetContent;
    @Mock private Tab mTab;

    private final ActivityTabProvider mTabProvider = new ActivityTabProvider();
    private final SettableMonotonicObservableSupplier<Boolean> mOmniboxFocusStateSupplier =
            ObservableSuppliers.createMonotonic();
    private final SettableNullableObservableSupplier<Tab> mTabObservableSupplier =
            ObservableSuppliers.createNullable();
    private final OneshotSupplierImpl<LayoutStateProvider> mLayoutStateProviderSupplier =
            new OneshotSupplierImpl<>();

    private BottomSheetManager mBottomSheetManager;
    private BottomControlsLayer mLayer;
    private BottomSheetObserver mObserver;

    @Before
    public void setUp() {
        when(mTab.isUserInteractable()).thenReturn(true);
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.setForTesting(mTab);

        mBottomSheetManager =
                new BottomSheetManager(
                        mSheetController,
                        mTabProvider,
                        mControlsVisibilityManager,
                        mExpandedSheetHelper,
                        mOmniboxFocusStateSupplier,
                        mOverlayManager,
                        mLayoutStateProviderSupplier,
                        mBottomControlsStacker,
                        /* isBottomSheetAsBrowserControlsEnabled= */ true);

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
    public void testLayerRegistration_disabled() {
        clearInvocations(mBottomControlsStacker);
        new BottomSheetManager(
                mSheetController,
                mTabProvider,
                mControlsVisibilityManager,
                mExpandedSheetHelper,
                mOmniboxFocusStateSupplier,
                mOverlayManager,
                mLayoutStateProviderSupplier,
                mBottomControlsStacker,
                /* isBottomSheetAsBrowserControlsEnabled= */ false);
        verify(mBottomControlsStacker, never()).addLayer(any());
        verify(mControlsVisibilityManager).addObserver(any());
    }

    @Test
    public void testGetHeight_actsAsBrowserControls() {
        when(mSheetController.getCurrentSheetContent()).thenReturn(mSheetContent);
        when(mSheetContent.actsAsBrowserControls()).thenReturn(true);
        when(mSheetController.getCurrentPeekHeightPx()).thenReturn(100);
        when(mSheetController.getSheetState()).thenReturn(BottomSheetController.SheetState.PEEK);
        when(mSheetController.isFullWidth()).thenReturn(true);

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
        when(mSheetController.isFullWidth()).thenReturn(true);

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

    @Test
    public void testCalculateContributedHeight_notFullWidth() {
        when(mSheetController.getCurrentSheetContent()).thenReturn(mSheetContent);
        when(mSheetContent.actsAsBrowserControls()).thenReturn(true);
        when(mSheetController.isFullWidth()).thenReturn(false);
        when(mSheetController.getCurrentPeekHeightPx()).thenReturn(100);
        when(mSheetController.getSheetState()).thenReturn(BottomSheetController.SheetState.PEEK);

        assertEquals(0, mLayer.getHeight());
    }

    @Test
    public void testOnContainerSizeChanged_heightChanged() {
        when(mSheetController.getCurrentSheetContent()).thenReturn(mSheetContent);
        when(mSheetContent.actsAsBrowserControls()).thenReturn(true);
        when(mSheetController.isFullWidth()).thenReturn(true);
        when(mSheetController.getCurrentPeekHeightPx()).thenReturn(100);
        when(mSheetController.getSheetState()).thenReturn(BottomSheetController.SheetState.PEEK);

        clearInvocations(mBottomControlsStacker);
        mObserver.onContainerSizeChanged(200, 400);
        verify(mBottomControlsStacker).requestLayerUpdate(false);
    }

    @Test
    public void testGetBackgroundColor() {
        when(mSheetController.getCurrentSheetContent()).thenReturn(mSheetContent);
        when(mSheetContent.actsAsBrowserControls()).thenReturn(true);
        when(mSheetController.isFullWidth()).thenReturn(true);
        when(mSheetController.getSheetBackgroundColor()).thenReturn(Color.RED);
        assertEquals(Color.RED, (int) mLayer.getBackgroundColor());
    }

    @Test
    public void testOnSheetHidden_topControlsScrolledOff_animates() {
        showSheetInPeekState();

        clearInvocations(mBottomControlsStacker);
        when(mControlsVisibilityManager.getTopControlsHeight()).thenReturn(100);
        when(mControlsVisibilityManager.getTopControlOffset()).thenReturn(-50);
        hideSheetAndAssertHidden();
        verify(mBottomControlsStacker).requestLayerUpdate(/* animate= */ true);
    }

    @Test
    public void testOnSheetHidden_topControlsShown_doesNotAnimate() {
        showSheetInPeekState();

        clearInvocations(mBottomControlsStacker);
        when(mControlsVisibilityManager.getTopControlsHeight()).thenReturn(100);
        when(mControlsVisibilityManager.getTopControlOffset()).thenReturn(0);
        hideSheetAndAssertHidden();
        verify(mBottomControlsStacker).requestLayerUpdate(/* animate= */ false);
        verify(mBottomControlsStacker, never()).requestLayerUpdate(/* animate= */ true);
    }

    @Test
    public void testOnSheetHidden_noTopControls_doesNotAnimate() {
        showSheetInPeekState();

        clearInvocations(mBottomControlsStacker);
        when(mControlsVisibilityManager.getTopControlsHeight()).thenReturn(0);
        when(mControlsVisibilityManager.getTopControlOffset()).thenReturn(0);
        hideSheetAndAssertHidden();
        verify(mBottomControlsStacker).requestLayerUpdate(/* animate= */ false);
        verify(mBottomControlsStacker, never()).requestLayerUpdate(/* animate= */ true);
    }

    @Test
    public void testOnSheetHidden_heightChanged_doesNotAnimate() {
        when(mSheetContent.actsAsBrowserControls()).thenReturn(true);
        when(mSheetController.isFullWidth()).thenReturn(true);
        when(mSheetController.getCurrentPeekHeightPx()).thenReturn(100);
        showSheetInPeekState();
        assertEquals(100, mLayer.getHeight());

        clearInvocations(mBottomControlsStacker);
        when(mControlsVisibilityManager.getTopControlsHeight()).thenReturn(100);
        when(mControlsVisibilityManager.getTopControlOffset()).thenReturn(-50);
        hideSheetAndAssertHidden();
        assertEquals(0, mLayer.getHeight());
        verify(mBottomControlsStacker).requestLayerUpdate(/* animate= */ false);
        verify(mBottomControlsStacker, never()).requestLayerUpdate(/* animate= */ true);
    }

    @Test
    public void testInitialUpdate_hidden_doesNotRequestUpdate() {
        // The initial HIDDEN/0 snapshot already matches a hidden sheet, so no update is needed.
        clearInvocations(mBottomControlsStacker);
        when(mControlsVisibilityManager.getTopControlsHeight()).thenReturn(100);
        when(mControlsVisibilityManager.getTopControlOffset()).thenReturn(-100);
        when(mSheetController.getSheetState()).thenReturn(BottomSheetController.SheetState.HIDDEN);
        BottomControlsLayer layer = createManagerAndCaptureLayer();
        assertEquals(BottomControlsStacker.LayerVisibility.HIDDEN, layer.getLayerVisibility());
        assertEquals(0, layer.getHeight());
        verify(mBottomControlsStacker, never()).requestLayerUpdate(anyBoolean());
    }

    @Test
    public void testInitialUpdate_visibleZeroHeight_requestsUpdate() {
        clearInvocations(mBottomControlsStacker);
        when(mSheetController.getCurrentSheetContent()).thenReturn(mSheetContent);
        when(mSheetContent.actsAsBrowserControls()).thenReturn(false);
        when(mSheetController.getSheetState()).thenReturn(BottomSheetController.SheetState.PEEK);
        BottomControlsLayer layer = createManagerAndCaptureLayer();
        assertEquals(BottomControlsStacker.LayerVisibility.VISIBLE, layer.getLayerVisibility());
        verify(mBottomControlsStacker).requestLayerUpdate(/* animate= */ false);
        verify(mBottomControlsStacker, never()).requestLayerUpdate(/* animate= */ true);
    }

    @Test
    public void testInitialUpdate_hidingTopControlsShown_hidden() {
        // Created mid-hide, the initial HIDDEN snapshot latches the layer hidden since the sheet
        // is going away, so no update is needed.
        clearInvocations(mBottomControlsStacker);
        when(mSheetController.getCurrentSheetContent()).thenReturn(mSheetContent);
        when(mSheetController.getSheetState())
                .thenReturn(BottomSheetController.SheetState.SCROLLING);
        when(mSheetController.isSheetHiding()).thenReturn(true);
        when(mControlsVisibilityManager.getTopControlsHeight()).thenReturn(100);
        when(mControlsVisibilityManager.getTopControlOffset()).thenReturn(0);
        BottomControlsLayer layer = createManagerAndCaptureLayer();
        assertEquals(BottomControlsStacker.LayerVisibility.HIDDEN, layer.getLayerVisibility());
        verify(mBottomControlsStacker, never()).requestLayerUpdate(anyBoolean());
    }

    @Test
    public void testOnSheetHidden_topControlsScrolledOff_nativePage_doesNotAnimate() {
        showSheetInPeekState();

        clearInvocations(mBottomControlsStacker);
        when(mTab.isNativePage()).thenReturn(true);
        when(mControlsVisibilityManager.getTopControlsHeight()).thenReturn(100);
        when(mControlsVisibilityManager.getTopControlOffset()).thenReturn(-50);
        hideSheetAndAssertHidden();
        verify(mBottomControlsStacker).requestLayerUpdate(/* animate= */ false);
        verify(mBottomControlsStacker, never()).requestLayerUpdate(/* animate= */ true);
    }

    @Test
    public void testOnSheetHidden_topControlsScrolledOff_nullTab_doesNotAnimate() {
        showSheetInPeekState();

        clearInvocations(mBottomControlsStacker);
        mTabProvider.setForTesting(null);
        when(mControlsVisibilityManager.getTopControlsHeight()).thenReturn(100);
        when(mControlsVisibilityManager.getTopControlOffset()).thenReturn(-50);
        hideSheetAndAssertHidden();
        verify(mBottomControlsStacker).requestLayerUpdate(/* animate= */ false);
        verify(mBottomControlsStacker, never()).requestLayerUpdate(/* animate= */ true);
    }

    @Test
    public void testOnSheetHidden_topControlsScrolledOff_notInteractable_doesNotAnimate() {
        showSheetInPeekState();

        clearInvocations(mBottomControlsStacker);
        when(mTab.isUserInteractable()).thenReturn(false);
        when(mControlsVisibilityManager.getTopControlsHeight()).thenReturn(100);
        when(mControlsVisibilityManager.getTopControlOffset()).thenReturn(-50);
        hideSheetAndAssertHidden();
        verify(mBottomControlsStacker).requestLayerUpdate(/* animate= */ false);
        verify(mBottomControlsStacker, never()).requestLayerUpdate(/* animate= */ true);
    }

    @Test
    public void testGetLayerVisibility_hiding_topControlsScrolledOff_hidden() {
        showSheetInPeekState();

        when(mControlsVisibilityManager.getTopControlsHeight()).thenReturn(100);
        when(mControlsVisibilityManager.getTopControlOffset()).thenReturn(-50);
        startHidingSheet();
        assertEquals(BottomControlsStacker.LayerVisibility.HIDDEN, mLayer.getLayerVisibility());
    }

    @Test
    public void testGetLayerVisibility_hiding_topControlsShown_visible() {
        showSheetInPeekState();

        when(mControlsVisibilityManager.getTopControlsHeight()).thenReturn(100);
        when(mControlsVisibilityManager.getTopControlOffset()).thenReturn(0);
        startHidingSheet();
        assertEquals(BottomControlsStacker.LayerVisibility.VISIBLE, mLayer.getLayerVisibility());
    }

    @Test
    public void testGetLayerVisibility_hiding_noTopControls_visible() {
        showSheetInPeekState();

        when(mControlsVisibilityManager.getTopControlsHeight()).thenReturn(0);
        when(mControlsVisibilityManager.getTopControlOffset()).thenReturn(0);
        startHidingSheet();
        assertEquals(BottomControlsStacker.LayerVisibility.VISIBLE, mLayer.getLayerVisibility());
    }

    @Test
    public void testGetLayerVisibility_hiding_topControlsShownMidHide_staysHidden() {
        showSheetInPeekState();

        clearInvocations(mBottomControlsStacker);
        when(mControlsVisibilityManager.getTopControlsHeight()).thenReturn(100);
        when(mControlsVisibilityManager.getTopControlOffset()).thenReturn(-50);
        startHidingSheet();
        assertEquals(BottomControlsStacker.LayerVisibility.HIDDEN, mLayer.getLayerVisibility());
        verify(mBottomControlsStacker).requestLayerUpdate(/* animate= */ true);

        // The top controls return mid-hide. A stacker pull triggered by a sibling layer must
        // still see HIDDEN, and further sheet callbacks must not re-pin the bottom controls.
        clearInvocations(mBottomControlsStacker);
        when(mControlsVisibilityManager.getTopControlOffset()).thenReturn(0);
        assertEquals(BottomControlsStacker.LayerVisibility.HIDDEN, mLayer.getLayerVisibility());
        mObserver.onContainerSizeChanged(200, 400);
        assertEquals(BottomControlsStacker.LayerVisibility.HIDDEN, mLayer.getLayerVisibility());
        verify(mBottomControlsStacker, never()).requestLayerUpdate(anyBoolean());

        hideSheetAndAssertHidden();
        verify(mBottomControlsStacker, never()).requestLayerUpdate(anyBoolean());
    }

    @Test
    public void testGetLayerVisibility_hideCancelledMidHide_visible() {
        showSheetInPeekState();

        when(mControlsVisibilityManager.getTopControlsHeight()).thenReturn(100);
        when(mControlsVisibilityManager.getTopControlOffset()).thenReturn(-50);
        startHidingSheet();
        assertEquals(BottomControlsStacker.LayerVisibility.HIDDEN, mLayer.getLayerVisibility());

        // The hide is cancelled while the sheet is still scrolling.
        clearInvocations(mBottomControlsStacker);
        when(mSheetController.isSheetHiding()).thenReturn(false);
        mObserver.onContainerSizeChanged(200, 400);
        assertEquals(BottomControlsStacker.LayerVisibility.VISIBLE, mLayer.getLayerVisibility());
        verify(mBottomControlsStacker).requestLayerUpdate(/* animate= */ false);
        verify(mBottomControlsStacker, never()).requestLayerUpdate(/* animate= */ true);
    }

    @Test
    public void testGetLayerVisibility_secondHideTopControlsShown_latchDoesNotCarryOver() {
        showSheetInPeekState();

        when(mControlsVisibilityManager.getTopControlsHeight()).thenReturn(100);
        when(mControlsVisibilityManager.getTopControlOffset()).thenReturn(-50);
        startHidingSheet();
        assertEquals(BottomControlsStacker.LayerVisibility.HIDDEN, mLayer.getLayerVisibility());
        hideSheetAndAssertHidden();

        // Re-show, then start a second hide with the top controls shown.
        showSheetInPeekState();
        when(mControlsVisibilityManager.getTopControlOffset()).thenReturn(0);
        startHidingSheet();
        assertEquals(BottomControlsStacker.LayerVisibility.VISIBLE, mLayer.getLayerVisibility());
    }

    @Test
    public void testGetLayerVisibility_hidingWithoutStateChange_returnsCommittedSnapshot() {
        showSheetInPeekState();

        // A drag dismiss moves SCROLLING -> hiding without an onSheetStateChanged callback. A
        // stacker pull must see the last committed visibility rather than live sheet state.
        clearInvocations(mBottomControlsStacker);
        when(mControlsVisibilityManager.getTopControlsHeight()).thenReturn(100);
        when(mControlsVisibilityManager.getTopControlOffset()).thenReturn(-50);
        when(mSheetController.getSheetState())
                .thenReturn(BottomSheetController.SheetState.SCROLLING);
        when(mSheetController.isSheetHiding()).thenReturn(true);
        assertEquals(BottomControlsStacker.LayerVisibility.VISIBLE, mLayer.getLayerVisibility());
        verify(mBottomControlsStacker, never()).requestLayerUpdate(anyBoolean());

        // The animated update is requested once the sheet reaches HIDDEN.
        hideSheetAndAssertHidden();
        verify(mBottomControlsStacker).requestLayerUpdate(/* animate= */ true);
    }

    @Test
    public void testOnSheetStartedHiding_topControlsScrolledOff_animates() {
        showSheetInPeekState();

        clearInvocations(mBottomControlsStacker);
        when(mControlsVisibilityManager.getTopControlsHeight()).thenReturn(100);
        when(mControlsVisibilityManager.getTopControlOffset()).thenReturn(-50);
        startHidingSheet();
        assertEquals(BottomControlsStacker.LayerVisibility.HIDDEN, mLayer.getLayerVisibility());
        verify(mBottomControlsStacker).requestLayerUpdate(/* animate= */ true);

        // Visibility is already HIDDEN, so the end of the hide animation is a no-op.
        clearInvocations(mBottomControlsStacker);
        hideSheetAndAssertHidden();
        verify(mBottomControlsStacker, never()).requestLayerUpdate(anyBoolean());
    }

    private void showSheetInPeekState() {
        when(mSheetController.getCurrentSheetContent()).thenReturn(mSheetContent);
        when(mSheetController.getSheetState()).thenReturn(BottomSheetController.SheetState.PEEK);
        mObserver.onSheetStateChanged(
                BottomSheetController.SheetState.PEEK,
                BottomSheetController.StateChangeReason.NONE);
        assertEquals(BottomControlsStacker.LayerVisibility.VISIBLE, mLayer.getLayerVisibility());
    }

    private void startHidingSheet() {
        when(mSheetController.isSheetHiding()).thenReturn(true);
        when(mSheetController.getSheetState())
                .thenReturn(BottomSheetController.SheetState.SCROLLING);
        mObserver.onSheetStateChanged(
                BottomSheetController.SheetState.SCROLLING,
                BottomSheetController.StateChangeReason.NONE);
    }

    private void hideSheetAndAssertHidden() {
        when(mSheetController.isSheetHiding()).thenReturn(false);
        when(mSheetController.getSheetState()).thenReturn(BottomSheetController.SheetState.HIDDEN);
        mObserver.onSheetStateChanged(
                BottomSheetController.SheetState.HIDDEN,
                BottomSheetController.StateChangeReason.NONE);
        assertEquals(BottomControlsStacker.LayerVisibility.HIDDEN, mLayer.getLayerVisibility());
    }

    private BottomControlsLayer createManagerAndCaptureLayer() {
        new BottomSheetManager(
                mSheetController,
                mTabProvider,
                mControlsVisibilityManager,
                mExpandedSheetHelper,
                mOmniboxFocusStateSupplier,
                mOverlayManager,
                mLayoutStateProviderSupplier,
                mBottomControlsStacker,
                /* isBottomSheetAsBrowserControlsEnabled= */ true);
        ArgumentCaptor<BottomControlsLayer> captor =
                ArgumentCaptor.forClass(BottomControlsLayer.class);
        verify(mBottomControlsStacker).addLayer(captor.capture());
        return captor.getValue();
    }
}
