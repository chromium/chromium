// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** Bottom sheet content of the NTP customization. */
public class NtpCustomizationBottomSheetContent implements BottomSheetContent {
    private final View mContentView;
    private final Runnable mBackPressRunnable;
    private final Runnable mOnDestroyRunnable;
    private ObservableSupplierImpl<Boolean> mBackPressStateChangedSupplier;

    NtpCustomizationBottomSheetContent(
            View contentView, Runnable backPressRunnable, Runnable onDestroy) {
        mContentView = contentView;
        mBackPressRunnable = backPressRunnable;
        mBackPressStateChangedSupplier = new ObservableSupplierImpl<>();
        mOnDestroyRunnable = onDestroy;
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
    public void destroy() {
        mOnDestroyRunnable.run();
    }

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
    public void onBackPressed() {
        mBackPressRunnable.run();
    }

    @Override
    public boolean handleBackPress() {
        mBackPressRunnable.run();
        return true;
    }

    @Override
    public ObservableSupplierImpl<Boolean> getBackPressStateChangedSupplier() {
        return mBackPressStateChangedSupplier;
    }

    @Override
    public String getSheetContentDescription(@NonNull Context context) {
        return context.getString(R.string.ntp_customization_main_bottom_sheet_content_description);
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // Half-height is disabled so no need for an accessibility string.
        assert false : "This method should not be called";
        return Resources.ID_NULL;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.ntp_customization_main_bottom_sheet_opened_full;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.ntp_customization_main_bottom_sheet_closed;
    }

    /** Sets up ObservableSupplierImpl<Boolean> when opening the bottom sheet. */
    void onSheetOpened() {
        // Sets the value in the supplier to true to indicate that back press should be handled by
        // the bottom sheet.
        mBackPressStateChangedSupplier.set(true);
    }

    /** Sets up ObservableSupplierImpl<Boolean> when closing the bottom sheet. */
    void onSheetClosed() {
        // Sets the value in the supplier to false to indicate that back press should not be handled
        // by the bottom sheet.
        mBackPressStateChangedSupplier.set(false);
    }

    void setBackPressStateChangedSupplierForTesting(ObservableSupplierImpl<Boolean> supplier) {
        mBackPressStateChangedSupplier = supplier;
    }
}
