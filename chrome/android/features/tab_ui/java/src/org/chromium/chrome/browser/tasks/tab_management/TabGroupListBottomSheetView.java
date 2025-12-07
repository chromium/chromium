// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.view.View.LAYOUT_DIRECTION_LTR;
import static android.view.View.LAYOUT_DIRECTION_RTL;

import static org.chromium.ui.base.LocalizationUtils.isLayoutRtl;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * A view for the Shared Tab Group Notice Bottom Sheet. Inform the user that changes made to a
 * shared tab group will be visible to everyone in the group.
 */
@NullMarked
public class TabGroupListBottomSheetView implements BottomSheetContent {
    private final RecyclerView mRecyclerView;
    private final ViewGroup mContentView;
    private final BottomSheetController mBottomsheetController;
    private final boolean mShowNewGroupRow;

    /**
     * @param context The {@link Context} to attach the bottom sheet to.
     * @param bottomSheetController The {@link BottomSheetController} that will be used to display
     *     this view. This is used to measure content (see {@link
     *     BottomSheetController#getMaxSheetWidth()}).
     * @param showNewGroupRow Whether the 'New Tab Group' row should be displayed.
     */
    TabGroupListBottomSheetView(
            Context context, BottomSheetController bottomSheetController, boolean showNewGroupRow) {
        mContentView =
                (ViewGroup)
                        LayoutInflater.from(context)
                                .inflate(R.layout.tab_group_list_bottom_sheet, /* root= */ null);
        mContentView.setLayoutDirection(
                isLayoutRtl() ? LAYOUT_DIRECTION_RTL : LAYOUT_DIRECTION_LTR);

        mRecyclerView = mContentView.findViewById(R.id.tab_group_parity_recycler_view);
        mRecyclerView.setLayoutManager(new LinearLayoutManager(context));
        mBottomsheetController = bottomSheetController;
        mShowNewGroupRow = showNewGroupRow;
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
        // Since this is the result of a user action, this needs to be able to override
        // more persistent bottom sheets.
        return ContentPriority.HIGH;
    }

    @Override
    public float getFullHeightRatio() {
        return Math.min(getSheetContentHeight(), mBottomsheetController.getContainerHeight())
                / (float) mBottomsheetController.getContainerHeight();
    }

    @Override
    public float getHalfHeightRatio() {
        return Math.min(getFullHeightRatio(), 0.5f);
    }

    @Override
    public boolean hideOnScroll() {
        return true;
    }

    @Override
    public @NonNull String getSheetContentDescription(Context context) {
        return mShowNewGroupRow
                ? context.getString(
                        R.string.tab_group_list_with_add_button_bottom_sheet_content_description)
                : context.getString(R.string.tab_group_list_bottom_sheet_content_description);
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        return R.string.tab_group_list_bottom_sheet_half_height;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return R.string.tab_group_list_bottom_sheet_full_height;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return R.string.tab_group_list_bottom_sheet_closed;
    }

    private float getSheetContentHeight() {
        mContentView.measure(
                MeasureSpec.makeMeasureSpec(
                        mBottomsheetController.getContainerWidth(), MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(
                        mBottomsheetController.getContainerHeight(), MeasureSpec.AT_MOST));
        return mContentView.getMeasuredHeight();
    }
}
