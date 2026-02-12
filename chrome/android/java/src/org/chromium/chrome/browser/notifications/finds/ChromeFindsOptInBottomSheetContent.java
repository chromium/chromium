// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.finds;

import android.content.Context;
import android.view.View;

import androidx.annotation.StringRes;

import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** Content for the Chrome Finds opt-in bottom sheet. */
@NullMarked
class ChromeFindsOptInBottomSheetContent implements BottomSheetContent {
    private final View mContentView;
    private final Runnable mOnBackPress;
    private final NonNullObservableSupplier<Boolean> mBackPressStateChangedSupplier;

    ChromeFindsOptInBottomSheetContent(
            View contentView,
            Runnable onBackPress,
            NonNullObservableSupplier<Boolean> backPressStateChangedSupplier) {
        mContentView = contentView;
        mOnBackPress = onBackPress;
        mBackPressStateChangedSupplier = backPressStateChangedSupplier;
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
    public float getFullHeightRatio() {
        return BottomSheetContent.HeightMode.WRAP_CONTENT;
    }

    @Override
    public boolean handleBackPress() {
        mOnBackPress.run();
        return true;
    }

    @Override
    public NonNullObservableSupplier<Boolean> getBackPressStateChangedSupplier() {
        return mBackPressStateChangedSupplier;
    }

    @Override
    public void onBackPressed() {
        mOnBackPress.run();
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public String getSheetContentDescription(Context context) {
        return context.getString(R.string.chrome_finds_opt_in_bottom_sheet_content_description);
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return R.string.chrome_finds_opt_in_bottom_sheet_closed_content_description;
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        return R.string.chrome_finds_opt_in_bottom_sheet_half_height_content_description;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return R.string.chrome_finds_opt_in_bottom_sheet_full_height_content_description;
    }
}
