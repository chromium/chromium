// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.finds;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.widget.ButtonCompat;

/** Coordinator for the Chrome Finds opt-in bottom sheet. */
@NullMarked
public class ChromeFindsOptInCoordinator {
    private final BottomSheetController mBottomSheetController;
    private final ChromeFindsOptInBottomSheetContent mSheetContent;
    private final BottomSheetObserver mBottomSheetObserver;
    private final SettableNonNullObservableSupplier<Boolean> mBackPressStateChangedSupplier =
            ObservableSuppliers.createNonNull(false);

    /**
     * @param context The Android {@link Context}.
     * @param bottomSheetController The system {@link BottomSheetController}.
     */
    public ChromeFindsOptInCoordinator(
            Context context, BottomSheetController bottomSheetController) {
        mBottomSheetController = bottomSheetController;

        View contentView =
                LayoutInflater.from(context)
                        .inflate(R.layout.chrome_finds_opt_in_bottom_sheet, /* root= */ null);
        mSheetContent =
                new ChromeFindsOptInBottomSheetContent(
                        contentView, this::onBackPressed, mBackPressStateChangedSupplier);

        // TODO(crbug.com/483037430): Add opt in functionality and metrics to these button clicks.
        ButtonCompat positiveButtonView = contentView.findViewById(R.id.opt_in_positive_button);
        positiveButtonView.setOnClickListener((view) -> dismiss());

        ButtonCompat negativeButtonView = contentView.findViewById(R.id.opt_in_negative_button);
        negativeButtonView.setOnClickListener((view) -> dismiss());

        mBottomSheetObserver =
                new EmptyBottomSheetObserver() {
                    @Override
                    public void onSheetOpened(@StateChangeReason int reason) {
                        super.onSheetOpened(reason);
                        mBackPressStateChangedSupplier.set(true);
                    }

                    @Override
                    public void onSheetClosed(@StateChangeReason int reason) {
                        super.onSheetClosed(reason);
                        mBackPressStateChangedSupplier.set(false);
                    }
                };
        mBottomSheetController.addObserver(mBottomSheetObserver);
    }

    /** Cleans up the coordinator. */
    public void destroy() {
        mBottomSheetController.removeObserver(mBottomSheetObserver);
    }

    /**
     * Shows the opt-in bottom sheet. Tab helper will trigger this method when conditions are met to
     * show the opt-in bottom sheet.
     */
    public void showBottomSheet() {
        mBottomSheetController.requestShowContent(mSheetContent, /* animate= */ true);
    }

    private void onBackPressed() {
        dismiss();
    }

    private void dismiss() {
        mBottomSheetController.hideContent(mSheetContent, /* animate= */ true);
    }
}
