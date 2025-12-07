// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import android.content.Context;
import android.view.View;

import androidx.annotation.StringRes;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** Bottom sheet view for displaying privacy guide control explanations */
@NullMarked
public class PrivacyGuideBottomSheetView implements BottomSheetContent {
    private final View mContentView;
    private final Runnable mCloseBottomSheetCallback;
    private final ObservableSupplierImpl<Boolean> mBackPressStateChangedSupplier =
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

    @Override
    public @Nullable View getToolbarView() {
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
    public String getSheetContentDescription(Context context) {
        return context.getString(R.string.privacy_guide_explanation_content_description);
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return R.string.privacy_guide_explanation_closed_description;
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        return R.string.privacy_guide_explanation_opened_half;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return R.string.privacy_guide_explanation_opened_full;
    }
}
