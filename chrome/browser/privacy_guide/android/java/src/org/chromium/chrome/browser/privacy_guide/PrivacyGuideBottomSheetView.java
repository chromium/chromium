// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** Bottom sheet view for displaying privacy guide control explanations */
public class PrivacyGuideBottomSheetView implements BottomSheetContent {
    private final View mContentView;
    private final Runnable mCloseBottomSheetCallback;
    private ObservableSupplierImpl<Boolean> mBackPressStateChangedSupplier =
            new ObservableSupplierImpl<>();
    private final float mHalfHeight;
    private final float mFullHeight;

    PrivacyGuideBottomSheetView(View contentView, Runnable closeBottomSheetCallback) {
        this(contentView, closeBottomSheetCallback, HeightMode.DEFAULT, HeightMode.WRAP_CONTENT);
    }

    PrivacyGuideBottomSheetView(
            View contentView,
            Runnable closeBottomSheetCallback,
            float halfHeight,
            float fullHeight) {
        mContentView = contentView;
        mCloseBottomSheetCallback = closeBottomSheetCallback;
        mBackPressStateChangedSupplier.set(true);
        mHalfHeight = halfHeight;
        mFullHeight = fullHeight;
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
        return mContentView.getScrollY();
    }

    @Override
    public void destroy() {}

    @Override
    public int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public int getPeekHeight() {
        return BottomSheetContent.HeightMode.DISABLED;
    }

    @Override
    public float getHalfHeightRatio() {
        return mHalfHeight;
    }

    @Override
    public float getFullHeightRatio() {
        return mFullHeight;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public boolean handleBackPress() {
        onBackPressed();
        return true;
    }

    @Override
    public void onBackPressed() {
        mCloseBottomSheetCallback.run();
    }

    @Override
    public ObservableSupplierImpl<Boolean> getBackPressStateChangedSupplier() {
        return mBackPressStateChangedSupplier;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.privacy_guide_explanation_content_description;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.privacy_guide_explanation_closed_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.privacy_guide_explanation_opened_half;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.privacy_guide_explanation_opened_full;
    }
}
