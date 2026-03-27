// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import androidx.annotation.Px;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.edge_to_edge.EdgeToEdgeSupplier.ChangeObserver;

/**
 * A helper class to help manage some behaviors of {@code SnackbarManager} for a {@code
 * ChromeActivity}.
 */
@NullMarked
public class ChromeActivitySnackbarHelper implements ChangeObserver {
    private final SettableNonNullObservableSupplier<Integer> mSupplier =
            ObservableSuppliers.createNonNull(0);
    private final MonotonicObservableSupplier<EdgeToEdgeController> mEdgeToEdgeControllerSupplier;
    private final Callback<EdgeToEdgeController> mEdgeToEdgeControllerObserver =
            this::onEdgeToEdgeControllerChanged;
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetOffsetChanged(float heightFraction, float offsetPx) {
                    updateMargin();
                }
            };

    private @Nullable EdgeToEdgeController mCurrentEdgeToEdgeController;

    /**
     * Constructs a new ChromeActivitySnackbarHelper.
     *
     * @param edgeToEdgeControllerSupplier The supplier for the EdgeToEdgeController.
     * @param bottomSheetController The {@link BottomSheetController} to observe.
     */
    public ChromeActivitySnackbarHelper(
            MonotonicObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier,
            BottomSheetController bottomSheetController) {
        mEdgeToEdgeControllerSupplier = edgeToEdgeControllerSupplier;
        mBottomSheetController = bottomSheetController;

        edgeToEdgeControllerSupplier.addSyncObserverAndCallIfNonNull(mEdgeToEdgeControllerObserver);
        bottomSheetController.addObserver(mBottomSheetObserver);

        updateMargin();
    }

    /** Returns the supplier for the snackbar bottom margin. */
    public NonNullObservableSupplier<Integer> getBottomMarginSupplier() {
        return mSupplier;
    }

    /** Destroys the supplier, removing observers. */
    public void destroy() {
        if (mCurrentEdgeToEdgeController != null) {
            mCurrentEdgeToEdgeController.unregisterObserver(this);
            mCurrentEdgeToEdgeController = null;
        }
        mEdgeToEdgeControllerSupplier.removeObserver(mEdgeToEdgeControllerObserver);

        mBottomSheetController.removeObserver(mBottomSheetObserver);
    }

    private void onEdgeToEdgeControllerChanged(@Nullable EdgeToEdgeController controller) {
        if (mCurrentEdgeToEdgeController != null) {
            mCurrentEdgeToEdgeController.unregisterObserver(this);
        }
        mCurrentEdgeToEdgeController = controller;
        if (mCurrentEdgeToEdgeController != null) {
            mCurrentEdgeToEdgeController.registerObserver(this);
        }
        updateMargin();
    }

    @Override
    public void onToEdgeChange(
            @Px int bottomInset, boolean isDrawingToEdge, boolean isPageOptInToEdge) {
        updateMargin();
    }

    private void updateMargin() {
        int bottomInset =
                mCurrentEdgeToEdgeController != null
                        ? mCurrentEdgeToEdgeController.getBottomInsetPx()
                        : 0;
        mSupplier.set(bottomInset + mBottomSheetController.getCurrentOffset());
    }
}
