// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.page_info_sheet;

import android.view.View;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** Bottom sheet to provide info about the page that is shared. */
class PageInfoBottomSheetContent implements BottomSheetContent {
    private final View mContentView;

    public PageInfoBottomSheetContent(View contentView) {
        mContentView = contentView;
    }

    /* BottomSheetContent implementation. */
    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {
        // The bottom sheet observer events are used for destruction.
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
        return false;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.share_with_summary_sheet_description;
    }

    @Override
    public boolean hasCustomLifecycle() {
        return false;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // Half-height is disabled so no need for an accessibility string.
        assert false : "This method should not be called";
        return 0;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.share_with_summary_sheet_opened;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.share_with_summary_sheet_closed;
    }
}
