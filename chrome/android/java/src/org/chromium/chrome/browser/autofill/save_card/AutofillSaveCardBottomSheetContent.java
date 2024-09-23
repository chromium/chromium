// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.save_card;

import android.view.View;
import android.widget.ScrollView;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** Implements the content for the autofill save card bottom sheet. */
/*package*/ class AutofillSaveCardBottomSheetContent implements BottomSheetContent {
    private final View mContentView;
    private final ScrollView mScrollView;

    /**
     * Creates the save card contents.
     *
     * @param contentView The bottom sheet content.
     * @param scrollView The view that optionally scrolls the contents within the sheet on smaller
     *     screens.
     */
    /*package*/ AutofillSaveCardBottomSheetContent(View contentView, ScrollView scrollView) {
        mContentView = contentView;
        mScrollView = scrollView;
    }

    // BottomSheetContent implementation follows:
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
        return mScrollView.getScrollY();
    }

    @Override
    public void destroy() {
        // In order to be able to know the reason for this bottom sheet being closed, the
        // BottomSheetObserver interface is used by the lifecycle class instead.
    }

    @Override
    public boolean hasCustomLifecycle() {
        // This bottom sheet should stay open during page navigation. The
        // AutofillSaveCardBottomSheetBridge is responsible for hiding this bottom sheet.
        return true;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public float getHalfHeightRatio() {
        return HeightMode.DISABLED;
    }

    @Override
    public int getPeekHeight() {
        return HeightMode.DISABLED;
    }

    @Override
    public boolean hideOnScroll() {
        return true;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.autofill_save_card_prompt_bottom_sheet_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        assert false : "This method will not be called.";
        return 0;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.autofill_save_card_prompt_bottom_sheet_full_height;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.autofill_save_card_prompt_bottom_sheet_closed;
    }
}
