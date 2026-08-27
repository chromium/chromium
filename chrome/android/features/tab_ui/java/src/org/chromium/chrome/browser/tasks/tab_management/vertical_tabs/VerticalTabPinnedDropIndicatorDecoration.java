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
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalExternalViewDragDropReorderStrategy.DropTargetResult;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.base.LocalizationUtils;

/**
 * An {@link RecyclerView.ItemDecoration} that renders a vertical drop indicator bar between columns
 * in the pinned tabs grid during cross-window or external drag-and-drop operations.
 */
@NullMarked
public class VerticalTabPinnedDropIndicatorDecoration
        extends BaseVerticalTabDropIndicatorDecoration {
    static final float INDICATOR_DIVISOR = 2.0f;

    private final int mItemGap;
    private final int mItemHeight;

    /**
     * @param context The {@link Context} used to retrieve dimension resources and styling.
     */
    public VerticalTabPinnedDropIndicatorDecoration(Context context) {
        super(context);
        Resources res = context.getResources();
        mItemGap = res.getDimensionPixelSize(R.dimen.vertical_tab_pinned_item_gap);
        boolean isTablet = VerticalTabUtils.isTablet(context);
        mItemHeight =
                res.getDimensionPixelSize(
                        isTablet
                                ? R.dimen.vertical_tab_pinned_item_height_tablet
                                : R.dimen.vertical_tab_pinned_item_height);
    }

    @Override
    protected boolean shouldDraw(DropTargetResult result) {
        if (result.targetType == DropTargetResult.TargetType.PINNED_GRID
                && !result.isZeroPinnedState) {
            return true;
        }
        return result.targetType == DropTargetResult.TargetType.MAIN_LIST
                && result.isZeroNormalTabsState;
    }

    @Override
    protected boolean calculateBounds(RectF outRect, RecyclerView parent, DropTargetResult result) {
        if (result.targetType == DropTargetResult.TargetType.MAIN_LIST
                && result.isZeroNormalTabsState) {
            int parentPaddingLeft = parent.getPaddingLeft();
            int parentPaddingRight = parent.getPaddingRight();
            int parentWidth = parent.getWidth();

            float left = parentPaddingLeft;
            float right = parentWidth - parentPaddingRight;
            if (right <= left) return false;

            float centerY = parent.getHeight() - mIndicatorThickness / INDICATOR_DIVISOR;
            float top = centerY - mIndicatorThickness / INDICATOR_DIVISOR;
            float bottom = centerY + mIndicatorThickness / INDICATOR_DIVISOR;

            outRect.set(left, top, right, bottom);
            return true;
        }

        View targetView = getAttachedTargetView(result, parent);

        float itemLeft;
        float itemRight;
        float itemTop;
        float itemBottom;

        if (targetView != null) {
            itemLeft = targetView.getLeft() + targetView.getTranslationX();
            itemRight = targetView.getRight() + targetView.getTranslationX();
            itemTop = targetView.getTop() + targetView.getTranslationY();
            itemBottom = targetView.getBottom() + targetView.getTranslationY();
        } else {
            Rect bounds = result.anchorBounds;
            itemLeft = bounds.left;
            itemRight = bounds.right;
            itemTop = bounds.top;
            itemBottom = bounds.bottom;
        }

        if (itemBottom <= itemTop) {
            itemTop = parent.getPaddingTop();
            itemBottom = itemTop + mItemHeight;
        }

        boolean isRtl = LocalizationUtils.isLayoutRtl();
        boolean physicallyOnLeft = isRtl ? !result.insertBefore : result.insertBefore;
        float centerX =
                physicallyOnLeft ? (itemLeft - mItemGap / 2.0f) : (itemRight + mItemGap / 2.0f);

        float minCenterX = parent.getPaddingLeft() + mIndicatorThickness / 2.0f;
        float maxCenterX =
                parent.getWidth() - parent.getPaddingRight() - mIndicatorThickness / 2.0f;
        if (maxCenterX >= minCenterX) {
            centerX = MathUtils.clamp(centerX, minCenterX, maxCenterX);
        }

        float left = centerX - mIndicatorThickness / 2.0f;
        float right = centerX + mIndicatorThickness / 2.0f;
        float top = itemTop;
        float bottom = itemBottom;

        outRect.set(left, top, right, bottom);
        return true;
    }
}
