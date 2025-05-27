// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.MAIN;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** Bottom sheet content of the NTP customization. */
@NullMarked
public class NtpCustomizationBottomSheetContent implements BottomSheetContent {
    private final View mContentView;
    private final Runnable mBackPressRunnable;
    private final Runnable mOnDestroyRunnable;
    private ObservableSupplierImpl<Boolean> mBackPressStateChangedSupplier;
    private Supplier<Integer> mCurrentBottomSheetTypeSupplier;

    NtpCustomizationBottomSheetContent(
            View contentView,
            Runnable backPressRunnable,
            Runnable onDestroy,
            Supplier<Integer> currentBottomSheetTypeSupplier) {
        mContentView = contentView;
        mBackPressRunnable = backPressRunnable;
        mBackPressStateChangedSupplier = new ObservableSupplierImpl<>();
        mOnDestroyRunnable = onDestroy;
        mCurrentBottomSheetTypeSupplier = currentBottomSheetTypeSupplier;
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
        return mContentView.findViewById(R.id.bottom_sheet_container).getScrollY();
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
    public @Nullable String getSheetContentDescription(Context context) {
        // Returns null when the current sheet is the main bottom sheet. This ensures TalkBack reads
        // the full content of the main bottom sheet in a top-to-bottom, left-to-right order.
        if (mCurrentBottomSheetTypeSupplier.get() == MAIN) {
            return null;
        }
        return context.getString(
                NtpCustomizationUtils.getSheetContentDescription(
                        mCurrentBottomSheetTypeSupplier.get()));
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // Half-height is disabled so no need for an accessibility string.
        assert false : "This method should not be called";
        return Resources.ID_NULL;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return NtpCustomizationUtils.getSheetFullHeightAccessibilityStringId(
                mCurrentBottomSheetTypeSupplier.get());
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

    void setCurrentBottomSheetTypeSupplierForTesting(Supplier<Integer> supplier) {
        mCurrentBottomSheetTypeSupplier = supplier;
    }
}
