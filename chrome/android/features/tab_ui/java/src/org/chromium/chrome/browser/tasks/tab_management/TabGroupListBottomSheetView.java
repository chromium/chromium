// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.view.View.LAYOUT_DIRECTION_LTR;
import static android.view.View.LAYOUT_DIRECTION_RTL;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.ui.base.LocalizationUtils.isLayoutRtl;

import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;

import androidx.annotation.Px;
import androidx.annotation.StringRes;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetRecyclerScrollListener;
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
        mRecyclerView.addOnScrollListener(
                new BottomSheetRecyclerScrollListener(bottomSheetController));
        mBottomsheetController = bottomSheetController;
        mShowNewGroupRow = showNewGroupRow;
    }

    void setRecyclerViewAdapter(SimpleRecyclerViewAdapter adapter) {
        mRecyclerView.setAdapter(adapter);
        invalidateContentHeight();
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
        return mRecyclerView.computeVerticalScrollOffset();
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
        float maxAvailable = getAvailableSheetHeight();
        return maxAvailable <= 0
                ? 0f
                : Math.min(getSheetContentHeight(), maxAvailable) / maxAvailable;
    }

    private @Px int getAvailableSheetHeight() {
        int maxHeight = mBottomsheetController.getMaxSheetHeight();
        return maxHeight > 0 ? maxHeight : mBottomsheetController.getContainerHeight();
    }

    @Override
    public float getHalfHeightRatio() {
        return Math.min(getFullHeightRatio(), 0.5f);
    }

    @Override
    public String getSheetContentDescription(Context context) {
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

    public void addBottomPadding() {
        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams) assumeNonNull(mRecyclerView).getLayoutParams();

        Resources resources = mRecyclerView.getContext().getResources();
        @Px int rowMargin = resources.getDimensionPixelSize(R.dimen.default_list_row_padding);
        if (params.bottomMargin != rowMargin) {
            params.bottomMargin = rowMargin;
            mRecyclerView.setLayoutParams(params);
            invalidateContentHeight();
        }
    }

    private int mCachedSheetHeightPx;

    public void invalidateContentHeight() {
        mCachedSheetHeightPx = 0;
    }

    private float getSheetContentHeight() {
        if (mCachedSheetHeightPx > 0) {
            return mCachedSheetHeightPx;
        }

        mContentView.measure(
                MeasureSpec.makeMeasureSpec(
                        mBottomsheetController.getMaxSheetWidth(), MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(getAvailableSheetHeight(), MeasureSpec.AT_MOST));
        int measuredHeight = mContentView.getMeasuredHeight();

        int adapterCount =
                mRecyclerView.getAdapter() != null ? mRecyclerView.getAdapter().getItemCount() : 0;
        int childCount = mRecyclerView.getChildCount();

        // When the model list is updated dynamically right before showing the sheet,
        // RecyclerView may not have completed creating and laying out its item views yet,
        // causing mContentView.measure() to under-report the total content height.
        // Estimate the full list height based on adapter item count to ensure accurate ratios.
        if (adapterCount > 0 && childCount < adapterCount) {
            View dragHandlebar =
                    mContentView.findViewById(R.id.tab_group_list_bottom_sheet_drag_handlebar);
            View titleText =
                    mContentView.findViewById(R.id.tab_group_parity_bottom_sheet_title_text);

            int nonListHeight = 0;
            if (dragHandlebar != null && dragHandlebar.getVisibility() != View.GONE) {
                ViewGroup.MarginLayoutParams lp =
                        (ViewGroup.MarginLayoutParams) dragHandlebar.getLayoutParams();
                nonListHeight += dragHandlebar.getMeasuredHeight() + lp.topMargin + lp.bottomMargin;
            }
            if (titleText != null) {
                ViewGroup.MarginLayoutParams lp =
                        (ViewGroup.MarginLayoutParams) titleText.getLayoutParams();
                nonListHeight += titleText.getMeasuredHeight() + lp.topMargin + lp.bottomMargin;
            }

            Resources resources = mRecyclerView.getContext().getResources();
            int rowHeight =
                    resources.getDimensionPixelSize(
                            ChromeFeatureList.sTabGroupListContainment.getValue()
                                    ? R.dimen.tab_group_row_height_containment
                                    : R.dimen.tab_group_row_height);
            ViewGroup.MarginLayoutParams recyclerLp =
                    (ViewGroup.MarginLayoutParams) mRecyclerView.getLayoutParams();
            int recyclerMargins =
                    recyclerLp.topMargin
                            + recyclerLp.bottomMargin
                            + mRecyclerView.getPaddingTop()
                            + mRecyclerView.getPaddingBottom();

            int estimatedHeight = nonListHeight + (adapterCount * rowHeight) + recyclerMargins;
            measuredHeight = Math.max(measuredHeight, estimatedHeight);
        }

        mCachedSheetHeightPx = measuredHeight;
        return measuredHeight;
    }
}
