// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * A view for the Shared Tab Group Notice Bottom Sheet. Inform the user that changes made to a
 * shared tab group will be visible to everyone in the group.
 */
@NullMarked
public class TabGroupListBottomSheetView extends LinearLayout implements BottomSheetContent {
    private final RecyclerView mRecyclerView;
    private final ViewGroup mContentView;

    TabGroupListBottomSheetView(Context context) {
        super(context);
        mContentView =
                (ViewGroup)
                        LayoutInflater.from(context)
                                .inflate(R.layout.tab_group_list_bottom_sheet, /* root= */ null);
        mRecyclerView = mContentView.findViewById(R.id.tab_group_parity_recycler_view);
        mRecyclerView.setLayoutManager(new LinearLayoutManager(context));
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
    }

    void setRecyclerViewAdapter(SimpleRecyclerViewAdapter adapter) {
        mRecyclerView.setAdapter(adapter);
    }

    // BottomSheetContent implementation follows:
    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    @Nullable
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mRecyclerView.getScrollY();
    }

    @Override
    public void destroy() {}

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public int getPriority() {
        return ContentPriority.LOW;
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
    public boolean hideOnScroll() {
        return true;
    }

    @Override
    public @NonNull String getSheetContentDescription(Context context) {
        return context.getString(R.string.tab_group_list_bottom_sheet_content_description);
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.tab_group_list_bottom_sheet_half_height;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.tab_group_list_bottom_sheet_full_height;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.tab_group_list_bottom_sheet_closed;
    }
}
