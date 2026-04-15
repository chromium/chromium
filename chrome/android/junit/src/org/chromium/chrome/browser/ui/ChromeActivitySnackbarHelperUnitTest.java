// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
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
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.ParentOverrideSlot;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChromeActivitySnackbarHelperUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private final SettableMonotonicObservableSupplier<EdgeToEdgeController>
            mEdgeToEdgeControllerSupplier = ObservableSuppliers.createMonotonic();

    @Mock private Activity mActivity;
    @Mock private EdgeToEdgeController mEdgeToEdgeController1;
    @Mock private EdgeToEdgeController mEdgeToEdgeController2;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private SnackbarManager mSnackbarManager;

    private ChromeActivitySnackbarHelper mSnackbarHelper;

    @Before
    public void setUp() {
        mSnackbarHelper =
                new ChromeActivitySnackbarHelper(
                        mActivity, mEdgeToEdgeControllerSupplier, mBottomSheetController);
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
        ArgumentCaptor<BottomSheetObserver> observerCaptor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mBottomSheetController).addObserver(observerCaptor.capture());
        when(mBottomSheetController.getCurrentOffset()).thenReturn(50);
        observerCaptor.getValue().onSheetOffsetChanged(0.5f, 50.0f);

        assertEquals(150, (int) mSnackbarHelper.getBottomMarginSupplier().get());

        // Test that both EdgeToEdgeController and BottomSheetObserver updates stack correctly.
        when(mEdgeToEdgeController1.getBottomInsetPx()).thenReturn(200);
        mSnackbarHelper.onToEdgeChange(200, false, false);
        assertEquals(250, (int) mSnackbarHelper.getBottomMarginSupplier().get());
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
        ArgumentCaptor<BottomSheetObserver> observerCaptor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mBottomSheetController).addObserver(observerCaptor.capture());
        BottomSheetObserver observer = observerCaptor.getValue();

        ViewGroup mockContainer = mock(ViewGroup.class);
        when(mActivity.findViewById(R.id.bottom_sheet_snackbar_container))
                .thenReturn(mockContainer);

        BottomSheetContent mockContent = mock(BottomSheetContent.class);
        when(mockContent.allowInSheetContentSnackbars()).thenReturn(true);
        when(mockContent.hasCustomScrimLifecycle()).thenReturn(false);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(mockContent);

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
        ArgumentCaptor<BottomSheetObserver> observerCaptor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mBottomSheetController).addObserver(observerCaptor.capture());
        BottomSheetObserver observer = observerCaptor.getValue();

        ViewGroup mockContainer = mock(ViewGroup.class);
        when(mActivity.findViewById(R.id.bottom_sheet_snackbar_container))
                .thenReturn(mockContainer);

        BottomSheetContent mockContent = mock(BottomSheetContent.class);
        when(mockContent.allowInSheetContentSnackbars()).thenReturn(false);
        when(mockContent.hasCustomScrimLifecycle()).thenReturn(true);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(mockContent);

        // HALF state -> should not push override because not allowed
        observer.onSheetStateChanged(BottomSheetController.SheetState.HALF, 0);
        verify(mSnackbarManager, times(0))
                .pushParentViewOverride(eq(ParentOverrideSlot.BOTTOM_SHEET), any(), any());
        verify(mSnackbarManager, times(0)).dismissAllSnackbars();
    }

    @Test
    public void testBottomSheetContentChanged() {
        ArgumentCaptor<BottomSheetObserver> observerCaptor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mBottomSheetController).addObserver(observerCaptor.capture());
        BottomSheetObserver observer = observerCaptor.getValue();

        ViewGroup mockContainer = mock(ViewGroup.class);
        when(mActivity.findViewById(R.id.bottom_sheet_snackbar_container))
                .thenReturn(mockContainer);

        BottomSheetContent mockContent = mock(BottomSheetContent.class);
        when(mockContent.hasCustomScrimLifecycle()).thenReturn(true);
        when(mockContent.allowInSheetContentSnackbars()).thenReturn(true);
        when(mBottomSheetController.getSheetState())
                .thenReturn(BottomSheetController.SheetState.HALF);

        observer.onSheetContentChanged(mockContent);
        verify(mSnackbarManager)
                .pushParentViewOverride(eq(ParentOverrideSlot.BOTTOM_SHEET), any(), any());

        // Switch to not allowed content -> pop override
        BottomSheetContent mockContent2 = mock(BottomSheetContent.class);
        when(mockContent2.hasCustomScrimLifecycle()).thenReturn(true);
        when(mockContent2.allowInSheetContentSnackbars()).thenReturn(false);
        observer.onSheetContentChanged(mockContent2);
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
