// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** Bottom sheet content of the new tab page customization. */
public class NtpCustomizationMainBottomSheetContent implements BottomSheetContent {
    private final View mContentView;

    public NtpCustomizationMainBottomSheetContent(@NonNull View view) {
        mContentView = view;
    }

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
        return 0;
    }

    @Override
    public void destroy() {}

    @Override
    public int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public int getPeekHeight() {
        return BottomSheetContent.HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return BottomSheetContent.HeightMode.WRAP_CONTENT;
    }

    @Override
    public String getSheetContentDescription(@NonNull Context context) {
        return context.getString(R.string.ntp_customization_main_bottom_sheet_content_description);
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        // Half-height is disabled so no need for an accessibility string.
        assert false : "This method should not be called";
        return Resources.ID_NULL;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return R.string.ntp_customization_main_bottom_sheet_opened_full;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return R.string.ntp_customization_main_bottom_sheet_closed;
    }
}
