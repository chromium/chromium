// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.app.Activity;
import android.view.ViewGroup;

import androidx.annotation.Px;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BottomControlsLayer;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.ParentOverrideSlot;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.edge_to_edge.EdgeToEdgeSupplier.ChangeObserver;

import java.util.function.Supplier;

/**
 * A helper class to help manage some behaviors of {@code SnackbarManager} for a {@code
 * ChromeActivity}.
 */
@NullMarked
public class ChromeActivitySnackbarHelper implements ChangeObserver {
    private final SettableNonNullObservableSupplier<Integer> mSupplier =
            ObservableSuppliers.createNonNull(0);
    private final NonNullObservableSupplier<Integer> mZeroBottomMarginSupplier =
            ObservableSuppliers.alwaysZero();
    private final MonotonicObservableSupplier<EdgeToEdgeController> mEdgeToEdgeControllerSupplier;
    private final Callback<EdgeToEdgeController> mEdgeToEdgeControllerObserver =
            this::onEdgeToEdgeControllerChanged;
    private final Activity mActivity;
    private final BottomSheetController mBottomSheetController;
    private final Supplier<BottomControlsLayer> mBottomControlsLayerSupplier;
    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetOffsetChanged(float heightFraction, float offsetPx) {
                    updateMargin();
                }

                @Override
                public void onSheetContentChanged(@Nullable BottomSheetContent newContent) {
                    updateSnackbarForBottomSheet(
                            newContent, mBottomSheetController.getSheetState());
                }

                @Override
                public void onSheetStateChanged(int newState, int reason) {
                    updateSnackbarForBottomSheet(
                            mBottomSheetController.getCurrentSheetContent(), newState);
                }

                private void updateSnackbarForBottomSheet(
                        @Nullable BottomSheetContent content, int newState) {
                    if (mSnackbarManager == null) return;

                    boolean allowInSheetSnackbars =
                            content != null && content.allowInSheetContentSnackbars();
                    assert content == null
                                    || content.allowInSheetContentSnackbars()
                                    || content.hasCustomScrimLifecycle()
                            : "BottomSheetContent can only prevent out-of-sheet snackbars if it has"
                                    + " a custom scrim lifecycle.";

                    boolean shouldOverride =
                            (newState == SheetState.HALF || newState == SheetState.FULL)
                                    && allowInSheetSnackbars;
                    boolean shouldPop =
                            newState == SheetState.HIDDEN
                                    || newState == SheetState.PEEK
                                    || !allowInSheetSnackbars;

                    if (shouldOverride && !mHasSnackbarOverride) {
                        if (mBottomSheetSnackbarContainer == null) {
                            mBottomSheetSnackbarContainer =
                                    mActivity.findViewById(R.id.bottom_sheet_snackbar_container);
                        }
                        assert mBottomSheetSnackbarContainer != null;
                        // TODO(499164555): Determine if we should call dismissAllSnackbars here.

                        mHasSnackbarOverride = true;
                        mSnackbarManager.pushParentViewOverride(
                                ParentOverrideSlot.BOTTOM_SHEET,
                                mBottomSheetSnackbarContainer,
                                mZeroBottomMarginSupplier);
                    } else if (shouldPop && mHasSnackbarOverride) {
                        // TODO(499164555): Determine if we should call dismissAllSnackbars here.
                        // Keeping the active snackbar alive might cause a slight jump in the UI
                        // when switching containers, but this is needed to match api expectations
                        // prior to crrev.com/c/7712352. Namely snackbars shown as a sheet is
                        // closing need to remain visible long enough for a user to interact with
                        // them.

                        mSnackbarManager.popParentViewOverride(ParentOverrideSlot.BOTTOM_SHEET);
                        mHasSnackbarOverride = false;
                    }
                }
            };

    private @Nullable ViewGroup mBottomSheetSnackbarContainer;
    private @Nullable EdgeToEdgeController mCurrentEdgeToEdgeController;
    private @Nullable SnackbarManager mSnackbarManager;
    private boolean mHasSnackbarOverride;

    /**
     * Constructs a new ChromeActivitySnackbarHelper.
     *
     * @param activity The activity.
     * @param edgeToEdgeControllerSupplier The supplier for the EdgeToEdgeController.
     * @param bottomSheetController The {@link BottomSheetController} to observe.
     * @param bottomControlsLayerSupplier The supplier for the {@link BottomControlsLayer}.
     */
    public ChromeActivitySnackbarHelper(
            Activity activity,
            MonotonicObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier,
            BottomSheetController bottomSheetController,
            Supplier<BottomControlsLayer> bottomControlsLayerSupplier) {
        mActivity = activity;
        mEdgeToEdgeControllerSupplier = edgeToEdgeControllerSupplier;
        mBottomSheetController = bottomSheetController;
        mBottomControlsLayerSupplier = bottomControlsLayerSupplier;

        edgeToEdgeControllerSupplier.addSyncObserverAndCallIfNonNull(mEdgeToEdgeControllerObserver);
        bottomSheetController.addObserver(mBottomSheetObserver);

        updateMargin();
    }

    /**
     * Sets the {@link SnackbarManager} instance.
     *
     * @param snackbarManager The SnackbarManager.
     */
    public void setSnackbarManager(SnackbarManager snackbarManager) {
        mSnackbarManager = snackbarManager;
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

        int sheetOffset = mBottomSheetController.getCurrentOffset();
        BottomControlsLayer layer = mBottomControlsLayerSupplier.get();
        int contributedHeight = layer != null ? layer.getHeight() : 0;

        // The snackbar's parent view (e.g. BottomContainer) is typically adjusted by browser
        // controls. When the sheet acts as browser controls, its contributed height already
        // pushes the content view (and thus the snackbar) up.
        // We only need to apply additional margin if the required position to clear the
        // navbar or the sheet exceeds what the browser controls are already providing.
        int margin = bottomInset + Math.max(sheetOffset - contributedHeight, 0);
        mSupplier.set(margin);
    }
}
