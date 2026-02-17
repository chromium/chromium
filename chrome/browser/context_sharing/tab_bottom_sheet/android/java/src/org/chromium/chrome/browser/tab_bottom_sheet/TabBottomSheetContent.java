// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.content.Context;
import android.view.View;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.context_sharing.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** The bottom sheet content for the tab bottom sheet. */
@NullMarked
public class TabBottomSheetContent implements BottomSheetContent {
    private final View mContentView;

    /**
     * Constructor.
     *
     * @param contentView The inflated view for the bottom sheet.
     */
    public TabBottomSheetContent(View contentView) {
        mContentView = contentView;
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
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean hasCustomLifecycle() {
        return true;
    }

    @Override
    public void onBackPressed() {
        handleBackPress();
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public int getPeekHeight() {
        return (int) (mContentView.getHeight() * 0.1);
    }

    @Override
    public float getHalfHeightRatio() {
        return 0.4f;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.DEFAULT;
    }

    @Override
    public String getSheetContentDescription(Context context) {
        return "";
    }

    @Override
    public boolean skipHalfStateOnScrollingDown() {
        return false;
    }

    @Override
    public boolean hideOnScroll() {
        return false;
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        return R.string.tab_bottom_sheet_half_height;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return R.string.tab_bottom_sheet_full_height;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return R.string.tab_bottom_sheet_closed;
    }

    @Override
    public boolean canSuppressInAnyState() {
        return false;
    }
}
