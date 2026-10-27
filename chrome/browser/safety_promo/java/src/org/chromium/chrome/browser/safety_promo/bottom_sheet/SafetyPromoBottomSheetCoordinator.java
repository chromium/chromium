// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo.bottom_sheet;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.safety_promo.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/**
 * Coordinator for the Safety Promo "Show me how" bottom sheet. A new coordinator is created each
 * time the sheet is opened.
 */
@NullMarked
public class SafetyPromoBottomSheetCoordinator {
    private final BottomSheetController mBottomSheetController;
    private SafetyPromoBottomSheetContent mBottomSheetContent;

    /**
     * @param context The {@link Context} used for inflating the sheet.
     * @param bottomSheetController The {@link BottomSheetController} that shows the sheet.
     * @param onDestroy Called after the sheet is closed for any reason and its content destroyed.
     */
    public SafetyPromoBottomSheetCoordinator(
            Context context, BottomSheetController bottomSheetController, Runnable onDestroy) {
        mBottomSheetController = bottomSheetController;

        View view =
                LayoutInflater.from(context)
                        .inflate(R.layout.safety_promo_show_me_how_bottom_sheet, /* root= */ null);
        mBottomSheetContent = new SafetyPromoBottomSheetContent(view, onDestroy);
    }

    /** Opens the "Show me how" bottom sheet. */
    public void showBottomSheet() {
        mBottomSheetController.requestShowContent(mBottomSheetContent, /* animate= */ true);
    }

    /** Closes the "Show me how" bottom sheet. */
    public void dismissBottomSheet(boolean animate) {
        mBottomSheetController.hideContent(mBottomSheetContent, animate);
    }

    void setBottomSheetContentForTesting(SafetyPromoBottomSheetContent bottomSheetContent) {
        mBottomSheetContent = bottomSheetContent;
    }
}
