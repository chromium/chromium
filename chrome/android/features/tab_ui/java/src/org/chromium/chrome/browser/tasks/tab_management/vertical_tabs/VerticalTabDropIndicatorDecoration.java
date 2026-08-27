// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.graphics.RectF;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.MathUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalExternalViewDragDropReorderStrategy.DropTargetResult;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.base.LocalizationUtils;

/**
 * An {@link RecyclerView.ItemDecoration} that renders a horizontal drop indicator line for vertical
 * tab list items during cross-window or external drag-and-drop operations.
 */
@NullMarked
public class VerticalTabDropIndicatorDecoration extends BaseVerticalTabDropIndicatorDecoration {
    private final int mMarginBottom;
    private final int mNestingMargin;

    /**
     * @param context The {@link Context} used to retrieve dimension resources and styling.
     */
    public VerticalTabDropIndicatorDecoration(Context context) {
        super(context);
        Resources res = context.getResources();
        boolean isTablet = VerticalTabUtils.isTablet(context);
        mMarginBottom =
                res.getDimensionPixelSize(
                        isTablet
                                ? R.dimen.vertical_tab_item_margin_bottom_tablet
                                : R.dimen.vertical_tab_item_margin_bottom);
        mNestingMargin = res.getDimensionPixelSize(R.dimen.vertical_tab_child_nesting_margin);
    }

    @Override
    protected boolean shouldDraw(DropTargetResult result) {
        return (result.targetType == DropTargetResult.TargetType.MAIN_LIST
                        && !result.isZeroNormalTabsState)
                || result.isZeroPinnedState;
    }

    @Override
    protected boolean calculateBounds(RectF outRect, RecyclerView parent, DropTargetResult result) {
        int parentPaddingLeft = parent.getPaddingLeft();
        int parentPaddingRight = parent.getPaddingRight();
        int parentWidth = parent.getWidth();
        boolean isRtl = LocalizationUtils.isLayoutRtl();

        float left;
        float right;
        if (result.isZeroPinnedState
                || result.isGroupTopOrBottomBoundary
                || result.destGroupTabId == TabList.INVALID_TAB_INDEX) {
            left = parentPaddingLeft;
            right = parentWidth - parentPaddingRight;
        } else {
            // Target is an inner group insertion slot -> indent to respect tab group spine
            if (isRtl) {
                left = parentPaddingLeft;
                right = parentWidth - parentPaddingRight - mNestingMargin;
            } else {
                left = parentPaddingLeft + mNestingMargin;
                right = parentWidth - parentPaddingRight;
            }
        }

        if (right <= left) return false;

        float centerY;
        if (result.isZeroPinnedState) {
            centerY = parent.getPaddingTop() + mIndicatorThickness / 2.0f;
        } else {
            View targetView = getAttachedTargetView(result, parent);
            if (targetView != null) {
                float viewTop = targetView.getTop() + targetView.getTranslationY();
                float viewBottom = targetView.getBottom() + targetView.getTranslationY();
                if (result.insertBefore) {
                    centerY = viewTop - mMarginBottom / 2.0f;
                } else {
                    centerY = viewBottom + mMarginBottom / 2.0f;
                }
            } else {
                Rect bounds = result.anchorBounds;
                if (bounds.isEmpty() || bounds.height() == 0) {
                    centerY = parent.getPaddingTop() + mIndicatorThickness / 2.0f;
                } else if (result.insertBefore) {
                    centerY = bounds.top - mMarginBottom / 2.0f;
                } else {
                    centerY = bounds.bottom + mMarginBottom / 2.0f;
                }
            }
        }

        // Clamp centerY to ensure indicator is visible within RecyclerView bounds
        float minCenterY = parent.getPaddingTop() + mIndicatorThickness / 2.0f;
        float maxCenterY =
                parent.getHeight() - parent.getPaddingBottom() - mIndicatorThickness / 2.0f;
        if (maxCenterY >= minCenterY) {
            centerY = MathUtils.clamp(centerY, minCenterY, maxCenterY);
        }

        float top = centerY - mIndicatorThickness / 2.0f;
        float bottom = centerY + mIndicatorThickness / 2.0f;

        outRect.set(left, top, right, bottom);
        return true;
    }
}
