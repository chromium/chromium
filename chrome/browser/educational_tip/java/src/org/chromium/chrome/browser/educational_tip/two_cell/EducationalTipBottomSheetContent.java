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

    public EducationalTipBottomSheetContent(View contentView) {
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
    public int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public @Nullable String getSheetContentDescription(Context context) {
        // TODO(crbug.com/479597724): Add sheet content description
        return "";
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // TODO(crbug.com/479597724): Add bottom sheet accessibility strings.
        return R.string.ntp_customization_main_bottom_sheet_closed;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        // TODO(crbug.com/479597724): Add bottom sheet accessibility strings.
        return R.string.ntp_customization_main_bottom_sheet_closed;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        // TODO(crbug.com/479597724): Add bottom sheet accessibility strings.
        return R.string.ntp_customization_main_bottom_sheet_closed;
    }
}
