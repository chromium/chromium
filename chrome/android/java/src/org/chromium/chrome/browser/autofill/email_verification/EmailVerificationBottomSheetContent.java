// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.email_verification;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;
import android.widget.ScrollView;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** Implements the content for the email verification bottom sheet. */
@NullMarked
/*package*/ class EmailVerificationBottomSheetContent implements BottomSheetContent {
    private final View mContentView;
    private final ScrollView mScrollView;

    /**
     * Creates the email verification contents.
     *
     * @param contentView The bottom sheet content view.
     * @param scrollView The view that optionally scrolls the contents within the sheet on smaller
     *     screens.
     */
    /*package*/ EmailVerificationBottomSheetContent(View contentView, ScrollView scrollView) {
        mContentView = contentView;
        mScrollView = scrollView;
    }

    // BottomSheetContent implementation follows:
    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    public @Nullable View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mScrollView.getScrollY();
    }

    @Override
    public void destroy() {}

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
    public boolean hideOnScroll() {
        return true;
    }

    @Override
    public String getSheetContentDescription(Context context) {
        return context.getString(R.string.autofill_email_verifier_prompt_title);
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        assert false : "This method will not be called.";
        return Resources.ID_NULL;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return R.string.autofill_email_verification_sheet_full_height;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return R.string.autofill_email_verification_sheet_closed;
    }
}
