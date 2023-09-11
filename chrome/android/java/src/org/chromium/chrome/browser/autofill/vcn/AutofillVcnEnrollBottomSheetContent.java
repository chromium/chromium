// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import android.view.View;
import android.widget.ScrollView;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** The contents for the virtual card number (VCN) enrollment bottom sheet. */
/*package*/ class AutofillVcnEnrollBottomSheetContent implements BottomSheetContent {
    private final View mContentView;
    private final ScrollView mScrollView;
    private final Runnable mOnDismiss;

    /**
     * Constructs the VCN enrollment bottom sheet contents.
     *
     * @param contentView The bottom sheet content.
     * @param scrollView The view that optionally scrolls the contents within the sheet on smaller
     *                   screens.
     * @param onDismiss The callback to invoke when the user dismisses the bottom sheet.
     */
    AutofillVcnEnrollBottomSheetContent(
            View contentView, ScrollView scrollView, Runnable onDismiss) {
        mContentView = contentView;
        mScrollView = scrollView;
        mOnDismiss = onDismiss;
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
        return mScrollView != null ? mScrollView.getScrollY() : 0;
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
    public boolean hasCustomLifecycle() {
        return true;
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
