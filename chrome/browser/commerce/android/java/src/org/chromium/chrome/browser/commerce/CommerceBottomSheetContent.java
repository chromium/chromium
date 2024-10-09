// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

public class CommerceBottomSheetContent implements BottomSheetContent {

    private final View mContentView;
    private final int mExpectedContentItemCount;

    public CommerceBottomSheetContent(View contentView, int expectedContentItemCount) {
        mContentView = contentView;
        mExpectedContentItemCount = expectedContentItemCount;
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
    public void destroy() {}

    @Override
    public int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public int getPeekHeight() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getHalfHeightRatio() {
        if (mExpectedContentItemCount > 2) {
            return 0.5f;
        }

        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        if (mExpectedContentItemCount > 2) {
            return 1.0f;
        }

        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.commerce_bottom_sheet_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        if (mExpectedContentItemCount > 2) {
            return R.string.commerce_bottom_sheet_content_opened_half;
        }
        // Half-height is disabled if mExpectedContentItemCount is less than or equal to 2, so no
        // need for an accessibility string.
        assert false : "Half state is not supported with < 2 commerce features";
        return 0;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.commerce_bottom_sheet_content_opened_full;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.commerce_bottom_sheet_content_closed;
    }

    @Override
    public boolean hasCustomScrimLifecycle() {
        // Don't show a scrim (gray overlay on page) when open the bottom sheet.
        return true;
    }
}
