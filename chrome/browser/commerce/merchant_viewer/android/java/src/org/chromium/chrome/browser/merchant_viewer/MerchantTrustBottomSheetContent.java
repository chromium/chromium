// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import android.view.View;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/**
 * An implementation of {@link BottomSheetContent} for the merchant trust bottom sheet experience.
 */
public class MerchantTrustBottomSheetContent implements BottomSheetContent {
    /** Ratio of the height when in half mode. */
    private static final float HALF_HEIGHT_RATIO = 0.6f;

    /** Ratio of the height when in full mode. Used in half-open variation. */
    public static final float FULL_HEIGHT_RATIO = 0.9f;

    private final View mToolbarView;
    private final View mContentView;
    private final Supplier<Integer> mVerticalScrollOffset;
    private final Runnable mBackPressCallback;
    private final ObservableSupplierImpl<Boolean> mBackPressStateChangedSupplier =
            new ObservableSupplierImpl<>();

    /** Creates a new instance. */
    public MerchantTrustBottomSheetContent(
            View toolbarView,
            View contentView,
            Supplier<Integer> verticalScrollOffset,
            Runnable backPressHandler) {
        mToolbarView = toolbarView;
        mContentView = contentView;
        mVerticalScrollOffset = verticalScrollOffset;
        mBackPressCallback = backPressHandler;
        mBackPressStateChangedSupplier.set(true);
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    public View getToolbarView() {
        return mToolbarView;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mVerticalScrollOffset.get();
    }

    @Override
    public void destroy() {}

    @Override
    public int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public int getPeekHeight() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getHalfHeightRatio() {
        return HALF_HEIGHT_RATIO;
    }

    @Override
    public float getFullHeightRatio() {
        return FULL_HEIGHT_RATIO;
    }

    @Override
    public boolean handleBackPress() {
        mBackPressCallback.run();
        return true;
    }

    @Override
    public ObservableSupplierImpl<Boolean> getBackPressStateChangedSupplier() {
        return mBackPressStateChangedSupplier;
    }

    @Override
    public void onBackPressed() {
        mBackPressCallback.run();
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.merchant_viewer_preview_sheet_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.merchant_viewer_preview_sheet_opened_half;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.merchant_viewer_preview_sheet_opened_full;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.merchant_viewer_preview_sheet_closed;
    }
}
