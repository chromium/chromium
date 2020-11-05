// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bottomsheet;

import android.view.View;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/**
 * Bookmark bottom sheet's implementation of {@link BottomSheetContent}.
 */
class BookmarkBottomSheetContent implements BottomSheetContent {
    private final View mContentView;
    private final Supplier<Integer> mScrollOffsetSupplier;

    /**
     * Constructs the bookmark bottom sheet view.
     */
    BookmarkBottomSheetContent(View contentView, Supplier<Integer> scrollOffsetSupplier) {
        mContentView = contentView;
        mScrollOffsetSupplier = scrollOffsetSupplier;
    }

    // BottomSheetContent implementation.
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
        return mScrollOffsetSupplier.get();
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
        return false;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        // TODO(xingliu): Add the correct accessibility strings.
        return R.string.reading_list_title;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.reading_list_title;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.reading_list_title;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.reading_list_title;
    }
}
