// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/**
 * The bottom sheet content for the Restore Tabs promo.
 */
public class RestoreTabsPromoSheetContent implements BottomSheetContent {
    private final View mContentView;

    public RestoreTabsPromoSheetContent(View contentView) {
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
        return R.string.restore_tabs_content_description;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.restore_tabs_sheet_closed;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.restore_tabs_content_description;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.restore_tabs_content_description;
    }
}
