// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** Bottom sheet view for displaying privacy guide control explanations */
public class PrivacyGuideBottomSheetView implements BottomSheetContent {
    private final View mContentView;

    PrivacyGuideBottomSheetView(View contentView) {
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
    public float getFullHeightRatio() {
        return BottomSheetContent.HeightMode.WRAP_CONTENT;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
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
