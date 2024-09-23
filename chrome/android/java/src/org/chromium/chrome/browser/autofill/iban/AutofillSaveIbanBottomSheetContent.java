// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.iban;

import android.view.View;
import android.widget.ScrollView;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** This class is responsible for rendering the content for the Autofill save IBAN bottomsheet. */
/*package*/ class AutofillSaveIbanBottomSheetContent implements BottomSheetContent {
    private final ScrollView mScrollView;
    private final View mContentView;

    AutofillSaveIbanBottomSheetContent(View contentView, ScrollView scrollView) {
        mContentView = contentView;
        mScrollView = scrollView;
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public void destroy() {}

    @Override
    public boolean hasCustomLifecycle() {
        // This bottom sheet should stay open during page navigation.
        return true;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mScrollView.getScrollY();
    }

    @Override
    public int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public int getPeekHeight() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getHalfHeightRatio() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.autofill_save_iban_prompt_bottom_sheet_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        assert false : "This method will not be called.";
        return 0;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.autofill_save_iban_prompt_bottom_sheet_full_height;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.autofill_save_iban_prompt_bottom_sheet_closed;
    }
}
