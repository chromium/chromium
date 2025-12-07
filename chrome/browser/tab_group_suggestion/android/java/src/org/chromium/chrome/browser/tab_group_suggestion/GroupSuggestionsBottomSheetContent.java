// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion;

import android.content.Context;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestion;
import org.chromium.components.visited_url_ranking.url_grouping.UserResponseMetadata;

/** Bottom sheet content for a GroupSuggestions bottom sheet UI. */
@NullMarked
public class GroupSuggestionsBottomSheetContent implements BottomSheetContent {

    private final @NonNull View mContentView;
    private final @NonNull GroupSuggestion mGroupSuggestion;
    private final @NonNull Callback<UserResponseMetadata> mCallback;

    public GroupSuggestionsBottomSheetContent(
            @NonNull View contentView,
            @NonNull GroupSuggestion suggestion,
            @NonNull Callback<UserResponseMetadata> callback) {
        mContentView = contentView;
        mGroupSuggestion = suggestion;
        mCallback = callback;
    }

    /** Get the {@link GroupSuggestion} showing in this sheet. */
    @NonNull
    public GroupSuggestion getGroupSuggestion() {
        return mGroupSuggestion;
    }

    /** Get the {@link Callback} showing in this sheet. */
    @NonNull
    public Callback<UserResponseMetadata> getUserResponseCallback() {
        return mCallback;
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
