// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
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
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChromeActivitySnackbarHelperUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private final SettableMonotonicObservableSupplier<EdgeToEdgeController>
            mEdgeToEdgeControllerSupplier = ObservableSuppliers.createMonotonic();

    @Mock private EdgeToEdgeController mEdgeToEdgeController1;
    @Mock private EdgeToEdgeController mEdgeToEdgeController2;
    @Mock private BottomSheetController mBottomSheetController;

    private ChromeActivitySnackbarHelper mSnackbarHelper;

    @Before
    public void setUp() {
        mSnackbarHelper =
                new ChromeActivitySnackbarHelper(
                        mEdgeToEdgeControllerSupplier, mBottomSheetController);
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
    public void testDestroy() {
        mEdgeToEdgeControllerSupplier.set(mEdgeToEdgeController1);
        assertTrue(mEdgeToEdgeControllerSupplier.hasObservers());
        mSnackbarHelper.destroy();

        verify(mEdgeToEdgeController1).unregisterObserver(mSnackbarHelper);
        verify(mBottomSheetController).removeObserver(any(BottomSheetObserver.class));
        assertFalse(mEdgeToEdgeControllerSupplier.hasObservers());
    }
}
