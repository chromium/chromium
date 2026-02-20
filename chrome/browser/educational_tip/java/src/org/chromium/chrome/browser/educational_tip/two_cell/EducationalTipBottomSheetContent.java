// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.two_cell;

import android.content.Context;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.educational_tip.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** Bottom sheet content of the educational tip two-cell layout. */
@NullMarked
public class EducationalTipBottomSheetContent implements BottomSheetContent {
    // TODO(crbug.com/479597724): Implement BottomSheetContent and add relevant tests.
    private final View mContentView;
    private final Context mContext;

    /**
     * @param context Context of the bottom sheet.
     * @param contentView Main view for the bottom sheet.
     */
    public EducationalTipBottomSheetContent(Context context, View contentView) {
        mContext = context;
        mContentView = contentView;
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
        // TODO(crbug.com/479597724): Implement vertical scroll offset.
        return 0;
    }

    @Override
    public void destroy() {}

    @Override
    public float getHalfHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public int getPriority() {
        return ContentPriority.LOW;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public @Nullable String getSheetContentDescription(Context context) {
        return mContext.getString(
                R.string.educational_tip_see_more_bottom_sheet_content_description);
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.educational_tip_see_more_bottom_sheet_accessibility_opened;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.educational_tip_see_more_bottom_sheet_accessibility_opened;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.educational_tip_see_more_bottom_sheet_accessibility_closed;
    }
}
