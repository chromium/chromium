// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_insights;

import android.view.View;
import android.widget.ScrollView;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** An implementation of {@link BottomSheetContent} for the price insights bottom sheet content. */
public class PriceInsightsBottomSheetContent implements BottomSheetContent {
    private final View mContentView;
    private final ScrollView mScrollView;

    public PriceInsightsBottomSheetContent(View contentView, ScrollView scrollView) {
        mContentView = contentView;
        mScrollView = scrollView;
    }

    /* BottomSheetContent implementation. */
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
        if (mScrollView != null) {
            return mScrollView.getScrollY();
        }
        return 0;
    }

    @Override
    public int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public int getPeekHeight() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getHalfHeightRatio() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public void destroy() {}

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.price_insights_bottom_sheet_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // Half-height is disabled so no need for an accessibility string.
        assert false : "This method should not be called";
        return 0;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.price_insights_bottom_sheet_content_opened_full;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.price_insights_bottom_sheet_content_closed;
    }

    @Override
    public boolean hasCustomScrimLifecycle() {
        // Don't show a scrim (gray overlay on page) when open the bottom sheet.
        return true;
    }
}
