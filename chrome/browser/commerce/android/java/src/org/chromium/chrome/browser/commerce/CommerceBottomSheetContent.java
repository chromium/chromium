// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

import android.content.Context;
import android.view.View;
import android.view.View.MeasureSpec;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

public class CommerceBottomSheetContent implements BottomSheetContent {

    private final View mContentView;
    private final int mExpectedContentItemCount;
    private final BottomSheetController mBottomSheetController;

    public CommerceBottomSheetContent(
            View contentView,
            int expectedContentItemCount,
            BottomSheetController bottomSheetController) {
        mContentView = contentView;
        mExpectedContentItemCount = expectedContentItemCount;
        mBottomSheetController = bottomSheetController;
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
        float containerHeight = mBottomSheetController.getContainerHeight();
        if (containerHeight == 0) {
            return HeightMode.DISABLED;
        }
        float contentRatio = getContentHeight() / containerHeight;
        if (contentRatio > 0.5) {
            return 0.5f;
        }

        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        float containerHeight = mBottomSheetController.getContainerHeight();
        if (containerHeight == 0) {
            return HeightMode.DISABLED;
        }
        float contentRatio = getContentHeight() / containerHeight;
        if (contentRatio > 0.5) {
            return contentRatio;
        }

        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public @NonNull String getSheetContentDescription(Context context) {
        return context.getString(R.string.commerce_bottom_sheet_content_description);
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        if (mExpectedContentItemCount > 2) {
            return R.string.commerce_bottom_sheet_content_opened_half;
        }
        // Half-height is disabled if mExpectedContentItemCount is less than or equal to 2, so no
        // need for an accessibility string.
        assert false : "Half state is not supported with < 2 commerce features";
        return 0;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return R.string.commerce_bottom_sheet_content_opened_full;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return R.string.commerce_bottom_sheet_content_closed;
    }

    @Override
    public boolean hasCustomScrimLifecycle() {
        // Don't show a scrim (gray overlay on page) when open the bottom sheet.
        return true;
    }

    private int getContentHeight() {
        mContentView.measure(
                MeasureSpec.makeMeasureSpec(
                        mBottomSheetController.getContainerWidth(), MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(
                        mBottomSheetController.getContainerHeight(), MeasureSpec.AT_MOST));
        return mContentView.getMeasuredHeight();
    }
}
