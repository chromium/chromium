// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** Bottom sheet content to provide data sharing service to TabGridDialog. */
public class TabGridDialogShareBottomSheetContent implements BottomSheetContent {
    private View mContentView;

    public TabGridDialogShareBottomSheetContent(View view) {
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
        // TODO(haileywang): Add strings for the sheet.
        return R.string.undo_bar_close_all_message;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // TODO(haileywang): Add strings for the sheet.
        return R.string.undo_bar_close_all_message;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        // TODO(haileywang): Add strings for the sheet.
        return R.string.undo_bar_close_all_message;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        // TODO(haileywang): Add strings for the sheet.
        return R.string.undo_bar_close_all_message;
    }
}
