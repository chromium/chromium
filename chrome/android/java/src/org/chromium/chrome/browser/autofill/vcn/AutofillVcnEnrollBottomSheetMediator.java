// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

/** The mediator controller for the virtual card enrollment bottom sheet. */
/*package*/ class AutofillVcnEnrollBottomSheetMediator implements BottomSheetContent {
    private final View mContentView;
    private final Runnable mOnDismiss;
    private BottomSheetController mBottomSheetController;

    /**
     * Constructs the mediator controller for the virtual card enrollment bottom sheet.
     *
     * @param contentView The bottom sheet content.
     * @param onDismiss The callback to invoke when the user dismisses the bottom sheet.
     */
    /*package*/ AutofillVcnEnrollBottomSheetMediator(View contentView, Runnable onDismiss) {
        mContentView = contentView;
        mOnDismiss = onDismiss;
    }

    /**
     * Requests to show the bottom sheet.
     *
     * @param window The window where the bottom sheet should be shown.
     *
     * @return True if shown.
     */
    /*package*/ boolean requestShowContent(WindowAndroid window) {
        mBottomSheetController = BottomSheetControllerProvider.from(window);
        if (mBottomSheetController == null) return false;

        return mBottomSheetController.requestShowContent(this, /*animate=*/true);
    }

    /** Hides the bottom sheet, if present. */
    /*package*/ void hide() {
        if (mBottomSheetController == null) return;
        mBottomSheetController.hideContent(this, /*animate=*/true,
                BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Nullable
    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {
        mOnDismiss.run();
    }

    @Override
    public int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.autofill_virtual_card_enroll_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        assert false : "Half height is disabled for virtual card enrollment bottom sheet";
        return R.string.autofill_virtual_card_enroll_content_description;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.autofill_virtual_card_enroll_full_height_content_description;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.autofill_virtual_card_enroll_closed_description;
    }

    @Override
    public int getPeekHeight() {
        return BottomSheetContent.HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return BottomSheetContent.HeightMode.WRAP_CONTENT;
    }
}
