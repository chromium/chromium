// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility_annotator.first_run;

import android.content.Context;
import android.view.View;
import android.widget.ScrollView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** Implements the content for the Accessibility Annotator first-run bottom sheet. */
@NullMarked
/*package*/ class AccessibilityAnnotatorFirstRunBottomSheetContent implements BottomSheetContent {
    private final View mContentView;
    private final ScrollView mScrollView;

    AccessibilityAnnotatorFirstRunBottomSheetContent(View contentView, ScrollView scrollView) {
        mContentView = contentView;
        mScrollView = scrollView;
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
        return mScrollView.getScrollY();
    }

    @Override
    public void destroy() {}

    @Override
    public boolean hasCustomLifecycle() {
        return false;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public float getHalfHeightRatio() {
        return HeightMode.DISABLED;
    }

    @Override
    public boolean hideOnScroll() {
        return true;
    }

    @Override
    public @Nullable String getSheetContentDescription(Context context) {
        // TODO(crbug.com/498909675): Replace with specific string id once implemented.
        return context.getString(R.string.accessibility_partial_custom_tab_bottom_sheet);
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // TODO(crbug.com/498909675): Replace with specific string id once implemented.
        return R.string.accessibility_partial_custom_tab_bottom_sheet;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        // TODO(crbug.com/498909675): Replace with specific string id once implemented.
        return R.string.accessibility_partial_custom_tab_bottom_sheet;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        // TODO(crbug.com/498909675): Replace with specific string id once implemented.
        return R.string.accessibility_partial_custom_tab_bottom_sheet;
    }
}
