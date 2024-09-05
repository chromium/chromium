// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.cards;

import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.educational_tip.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** Bottom sheet content of the default browser promo card. */
public class DefaultBrowserPromoBottomSheetContent implements BottomSheetContent {
    private View mContentView;

    public DefaultBrowserPromoBottomSheetContent(@NonNull View view) {
        mContentView = view;
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
    public int getSheetContentDescriptionStringId() {
        return R.string.educational_tip_default_browser_title;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.educational_tip_default_browser_title;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.educational_tip_default_browser_title;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.educational_tip_default_browser_title;
    }
}
