// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion;

import android.content.Context;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** Bottom sheet content for a GroupSuggestions bottom sheet UI. */
@NullMarked
public class GroupSuggestionsBottomSheetContent implements BottomSheetContent {

    private final @NonNull View mContentView;

    public GroupSuggestionsBottomSheetContent(@NonNull View contentView) {
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
        return 0;
    }

    @Override
    public void destroy() {}

    @Override
    public int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public int getPeekHeight() {
        return HeightMode.DISABLED;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public String getSheetContentDescription(Context context) {
        return "";
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return 0;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        // TODO(397221723): Placeholder string needed for bottom sheet to work. Replace with correct
        // string later.
        return R.string.commerce_bottom_sheet_content_opened_full;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        // TODO(397221723): Placeholder string needed for bottom sheet to work. Replace with correct
        // string later.
        return R.string.commerce_bottom_sheet_content_closed;
    }
}
