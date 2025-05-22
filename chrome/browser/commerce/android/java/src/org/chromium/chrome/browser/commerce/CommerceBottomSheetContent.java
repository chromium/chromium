// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

import android.content.Context;
import android.view.View;
import android.view.View.MeasureSpec;

import androidx.annotation.StringRes;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

@NullMarked
public class CommerceBottomSheetContent implements BottomSheetContent {

    private final View mContentView;
    private final RecyclerView mRecyclerView;
    private final BottomSheetController mBottomSheetController;
    private boolean mIsHalfHeightDisabled;

    public CommerceBottomSheetContent(
            View contentView, BottomSheetController bottomSheetController) {
        mContentView = contentView;
        mRecyclerView = mContentView.findViewById(R.id.commerce_content_recycler_view);
        mBottomSheetController = bottomSheetController;
        mIsHalfHeightDisabled = false;
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
        if (mRecyclerView != null) {
            return mRecyclerView.computeVerticalScrollOffset();
        }
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
    public float getHalfHeightRatio() {
        float containerHeight = mBottomSheetController.getContainerHeight();
        if (containerHeight == 0 || mIsHalfHeightDisabled) {
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
    public String getSheetContentDescription(Context context) {
        return context.getString(R.string.commerce_bottom_sheet_content_description);
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        return R.string.commerce_bottom_sheet_content_opened_half;
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

    /** Set whether to disable the bottom sheet content half height. */
    public void setIsHalfHeightDisabled(boolean isHalfHeightDisabled) {
        mIsHalfHeightDisabled = isHalfHeightDisabled;
    }

    private int getContentHeight() {
        mContentView.measure(
                MeasureSpec.makeMeasureSpec(
                        mBottomSheetController.getMaxSheetWidth(), MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(
                        mBottomSheetController.getContainerHeight(), MeasureSpec.AT_MOST));
        return mContentView.getMeasuredHeight();
    }
}
