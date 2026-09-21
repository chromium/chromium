// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.ViewGroup;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BottomControlsLayer;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.ParentOverrideSlot;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;

@RunWith(BaseRobolectricTestRunner.class)
public class ChromeActivitySnackbarHelperUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Activity mActivity;
    @Mock private EdgeToEdgeController mEdgeToEdgeController1;
    @Mock private EdgeToEdgeController mEdgeToEdgeController2;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private BottomControlsLayer mBottomControlsLayer;
    @Mock private ViewGroup mViewGroup;
    @Mock private BottomSheetContent mBottomSheetContent;
    @Mock private BottomSheetContent mMockContent;
    @Mock private BottomSheetContent mMockContent2;
    @Captor private ArgumentCaptor<BottomSheetObserver> mObserverCaptor;

    private SettableMonotonicObservableSupplier<EdgeToEdgeController> mEdgeToEdgeControllerSupplier;
    private ChromeActivitySnackbarHelper mSnackbarHelper;

    @Before
    public void setUp() {
        mEdgeToEdgeControllerSupplier = ObservableSuppliers.createMonotonic();
        mSnackbarHelper =
                new ChromeActivitySnackbarHelper(
                        mActivity,
                        mEdgeToEdgeControllerSupplier,
                        mBottomSheetController,
                        () -> mBottomControlsLayer);
        mSnackbarHelper.setSnackbarManager(mSnackbarManager);
    }

    @Test
    public void testSupplierValueWithNoController() {
        assertEquals(0, (int) mSnackbarHelper.getBottomMarginSupplier().get());
    }

    @Test
    public void testSupplierValueWithController() {
        when(mEdgeToEdgeController1.getBottomInsetPx()).thenReturn(100);
        mEdgeToEdgeControllerSupplier.set(mEdgeToEdgeController1);

        verify(mEdgeToEdgeController1).registerObserver(mSnackbarHelper);
        assertEquals(100, (int) mSnackbarHelper.getBottomMarginSupplier().get());

        // Test that onToEdgeChange updates the supplier.
        when(mEdgeToEdgeController1.getBottomInsetPx()).thenReturn(200);
        mSnackbarHelper.onToEdgeChange(200, false, false);
        assertEquals(200, (int) mSnackbarHelper.getBottomMarginSupplier().get());
    }

    @Test
    public void testBottomSheetOffset() {
        when(mEdgeToEdgeController1.getBottomInsetPx()).thenReturn(100);
        mEdgeToEdgeControllerSupplier.set(mEdgeToEdgeController1);

        verify(mEdgeToEdgeController1).registerObserver(mSnackbarHelper);
        verify(mBottomSheetController).addObserver(any(BottomSheetObserver.class));
        assertEquals(100, (int) mSnackbarHelper.getBottomMarginSupplier().get());

        // Test that BottomSheetObserver.onSheetOffsetChanged updates the supplier.
        verify(mBottomSheetController).addObserver(mObserverCaptor.capture());
        when(mBottomSheetController.getCurrentOffset()).thenReturn(50);
        mObserverCaptor.getValue().onSheetOffsetChanged(0.5f, 50.0f);

        assertEquals(150, (int) mSnackbarHelper.getBottomMarginSupplier().get());

        // Test that both EdgeToEdgeController and BottomSheetObserver updates stack correctly.
        when(mEdgeToEdgeController1.getBottomInsetPx()).thenReturn(200);
        mSnackbarHelper.onToEdgeChange(200, false, false);
        assertEquals(250, (int) mSnackbarHelper.getBottomMarginSupplier().get());
    }

    @Test
    public void testBottomSheetOffset_ActsAsBrowserControls() {
        reset(mEdgeToEdgeController1);
        when(mEdgeToEdgeController1.getBottomInsetPx()).thenReturn(100);
        mEdgeToEdgeControllerSupplier.set(mEdgeToEdgeController1);

        verify(mEdgeToEdgeController1).registerObserver(mSnackbarHelper);
        verify(mBottomSheetController).addObserver(any(BottomSheetObserver.class));
        assertEquals(100, (int) mSnackbarHelper.getBottomMarginSupplier().get());

        verify(mBottomSheetController).addObserver(mObserverCaptor.capture());

        doReturn(100).when(mBottomSheetController).getCurrentOffset();
        doReturn(50).when(mBottomControlsLayer).getHeight();

        mSnackbarHelper.onToEdgeChange(100, false, false);

        assertEquals(150, (int) mSnackbarHelper.getBottomMarginSupplier().get());

        doReturn(50).when(mBottomSheetController).getCurrentOffset();
        mObserverCaptor.getValue().onSheetOffsetChanged(0.5f, 50.0f);

        assertEquals(100, (int) mSnackbarHelper.getBottomMarginSupplier().get());
    }

    @Test
    public void testBottomSheetOffset_StandardOverlay() {
        reset(mEdgeToEdgeController1);
        when(mEdgeToEdgeController1.getBottomInsetPx()).thenReturn(100);
        mEdgeToEdgeControllerSupplier.set(mEdgeToEdgeController1);

        doReturn(50).when(mBottomSheetController).getCurrentOffset();
        doReturn(0).when(mBottomControlsLayer).getHeight(); // Standard overlay

        mSnackbarHelper.onToEdgeChange(100, false, false);

        // targetPosition = Math.max(100, 50) = 100
        // margin = Math.max(0, 100 - 0) = 100
        assertEquals(150, (int) mSnackbarHelper.getBottomMarginSupplier().get());
    }

    @Test
    public void testBottomSheetOffset_ActsAsBrowserControls_HeightGreaterThanInset() {
        reset(mEdgeToEdgeController1);
        when(mEdgeToEdgeController1.getBottomInsetPx()).thenReturn(100);
        mEdgeToEdgeControllerSupplier.set(mEdgeToEdgeController1);

        doReturn(150).when(mBottomSheetController).getCurrentOffset();
        doReturn(150).when(mBottomControlsLayer).getHeight(); // Greater than inset (100)

        mSnackbarHelper.onToEdgeChange(100, false, false);

        // targetPosition = Math.max(100, 150) = 150
        // margin = Math.max(0, 150 - 150) = 0
        assertEquals(100, (int) mSnackbarHelper.getBottomMarginSupplier().get());
    }

    @Test
    public void testBottomSheetOffset_NullLayer() {
        mSnackbarHelper =
                new ChromeActivitySnackbarHelper(
                        mActivity,
                        mEdgeToEdgeControllerSupplier,
                        mBottomSheetController,
                        SupplierUtils.ofNull());

        reset(mEdgeToEdgeController1);
        when(mEdgeToEdgeController1.getBottomInsetPx()).thenReturn(100);
        mEdgeToEdgeControllerSupplier.set(mEdgeToEdgeController1);

        doReturn(50).when(mBottomSheetController).getCurrentOffset();

        mSnackbarHelper.onToEdgeChange(100, false, false);

        // layer is null -> contributedHeight = 0 -> Standard overlay sheet
        // targetPosition = Math.max(100, 50) = 100
        // margin = Math.max(0, 100 - 0) = 100
        assertEquals(150, (int) mSnackbarHelper.getBottomMarginSupplier().get());
    }

    @Test
    public void testControllerChange() {
        when(mEdgeToEdgeController1.getBottomInsetPx()).thenReturn(100);
        mEdgeToEdgeControllerSupplier.set(mEdgeToEdgeController1);
        assertEquals(100, (int) mSnackbarHelper.getBottomMarginSupplier().get());

        when(mEdgeToEdgeController2.getBottomInsetPx()).thenReturn(150);
        mEdgeToEdgeControllerSupplier.set(mEdgeToEdgeController2);

        verify(mEdgeToEdgeController1).unregisterObserver(mSnackbarHelper);
        verify(mEdgeToEdgeController2).registerObserver(mSnackbarHelper);
        assertEquals(150, (int) mSnackbarHelper.getBottomMarginSupplier().get());
    }

    @Test
    public void testBottomSheetStateChanged() {
        verify(mBottomSheetController).addObserver(mObserverCaptor.capture());
        BottomSheetObserver observer = mObserverCaptor.getValue();

        when(mActivity.findViewById(R.id.bottom_sheet_snackbar_container)).thenReturn(mViewGroup);

        when(mBottomSheetContent.allowInSheetContentSnackbars()).thenReturn(true);
        when(mBottomSheetContent.hasCustomScrimLifecycle()).thenReturn(false);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(mBottomSheetContent);

        // HALF state -> push override
        observer.onSheetStateChanged(BottomSheetController.SheetState.HALF, 0);
        verify(mSnackbarManager)
                .pushParentViewOverride(eq(ParentOverrideSlot.BOTTOM_SHEET), any(), any());

        // FULL state -> should not push again
        observer.onSheetStateChanged(BottomSheetController.SheetState.FULL, 0);
        verify(mSnackbarManager, times(1))
                .pushParentViewOverride(eq(ParentOverrideSlot.BOTTOM_SHEET), any(), any());

        // PEEK state -> pop override
        observer.onSheetStateChanged(BottomSheetController.SheetState.PEEK, 0);
        verify(mSnackbarManager).popParentViewOverride(eq(ParentOverrideSlot.BOTTOM_SHEET));

        // HIDDEN state -> should not pop again
        observer.onSheetStateChanged(BottomSheetController.SheetState.HIDDEN, 0);
        verify(mSnackbarManager, times(1))
                .popParentViewOverride(eq(ParentOverrideSlot.BOTTOM_SHEET));
        verify(mSnackbarManager, times(0)).dismissAllSnackbars();
    }

    @Test
    public void testBottomSheetStateChanged_NotAllowed() {
        verify(mBottomSheetController).addObserver(mObserverCaptor.capture());
        BottomSheetObserver observer = mObserverCaptor.getValue();

        when(mActivity.findViewById(R.id.bottom_sheet_snackbar_container)).thenReturn(mViewGroup);

        when(mBottomSheetContent.allowInSheetContentSnackbars()).thenReturn(false);
        when(mBottomSheetContent.hasCustomScrimLifecycle()).thenReturn(true);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(mBottomSheetContent);

        // HALF state -> should not push override because not allowed
        observer.onSheetStateChanged(BottomSheetController.SheetState.HALF, 0);
        verify(mSnackbarManager, times(0))
                .pushParentViewOverride(eq(ParentOverrideSlot.BOTTOM_SHEET), any(), any());
        verify(mSnackbarManager, times(0)).dismissAllSnackbars();
    }

    @Test
    public void testBottomSheetContentChanged() {
        verify(mBottomSheetController).addObserver(mObserverCaptor.capture());
        BottomSheetObserver observer = mObserverCaptor.getValue();

        when(mActivity.findViewById(R.id.bottom_sheet_snackbar_container)).thenReturn(mViewGroup);

        when(mMockContent.hasCustomScrimLifecycle()).thenReturn(true);
        when(mMockContent.allowInSheetContentSnackbars()).thenReturn(true);
        when(mBottomSheetController.getSheetState())
                .thenReturn(BottomSheetController.SheetState.HALF);

        observer.onSheetContentChanged(mMockContent);
        verify(mSnackbarManager)
                .pushParentViewOverride(eq(ParentOverrideSlot.BOTTOM_SHEET), any(), any());

        // Switch to not allowed content -> pop override
        when(mMockContent2.hasCustomScrimLifecycle()).thenReturn(true);
        when(mMockContent2.allowInSheetContentSnackbars()).thenReturn(false);
        observer.onSheetContentChanged(mMockContent2);
        verify(mSnackbarManager).popParentViewOverride(eq(ParentOverrideSlot.BOTTOM_SHEET));
        verify(mSnackbarManager, times(0)).dismissAllSnackbars();
    }

    @Test
    public void testDestroy() {
        mEdgeToEdgeControllerSupplier.set(mEdgeToEdgeController1);
        assertTrue(mEdgeToEdgeControllerSupplier.hasObservers());
        mSnackbarHelper.destroy();

        verify(mEdgeToEdgeController1).unregisterObserver(mSnackbarHelper);
        verify(mBottomSheetController).removeObserver(any(BottomSheetObserver.class));
        assertFalse(mEdgeToEdgeControllerSupplier.hasObservers());
    }
}
