// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Paint.Style;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;

/** Circle pager indicator for recyclerview. */
public class CirclePagerIndicatorDecoration extends RecyclerView.ItemDecoration {
    private final @ColorInt int mColorActive;
    private final @ColorInt int mColorInactive;

    /** Height of the space that the indicator takes up at the bottom of the view in pixel. */
    private final int mIndicatorHeightPx;

    /** Indicator stroke width in pixel. */
    private final float mIndicatorStrokeWidthPx;

    /** The width (diameter) of the indicator dot in pixel. */
    private final float mIndicatorItemDiameterPx;

    /** The radius of the indicator dot in pixel. */
    private final float mIndicatorRadiusPx;

    /** Padding between indicators in pixel. */
    private final float mIndicatorItemPaddingPx;

    /** The start margin of the recyclerview in pixel. */
    private final int mStartMarginPx;

    private final Paint mPaint = new Paint();
    private final UiConfig mUiConfig;
    private final boolean mIsTablet;

    /**
     * @param context The {@link Context} that the application is running.
     * @param uiConfig The instance of {@link UiConfig}.
     * @param startMarginPx The start margin of the first item of the recyclerview.
     */
    public CirclePagerIndicatorDecoration(
            @NonNull Context context,
            @Nullable UiConfig uiConfig,
            int startMarginPx,
            int colorActive,
            int colorInactive,
            boolean isTablet) {
        mUiConfig = uiConfig;
        mStartMarginPx = startMarginPx;
        mColorActive = colorActive;
        mColorInactive = colorInactive;
        mIsTablet = isTablet;

        Resources resources = context.getResources();
        mIndicatorStrokeWidthPx = resources.getDimensionPixelSize(R.dimen.page_indicator_dot_size);
        mPaint.setStrokeWidth(mIndicatorStrokeWidthPx);
        mPaint.setStyle(Style.STROKE);
        mPaint.setAntiAlias(true);

        mIndicatorItemPaddingPx =
                (float) resources.getDimensionPixelSize(R.dimen.page_indicator_internal_padding);
        mIndicatorItemDiameterPx = mIndicatorStrokeWidthPx;
        mIndicatorRadiusPx = mIndicatorItemDiameterPx / 2f;
        mIndicatorHeightPx =
                (int) mIndicatorItemDiameterPx
                        + resources.getDimensionPixelSize(R.dimen.page_indicator_top_margin);
    }

    @Override
    public void onDrawOver(Canvas canvas, RecyclerView parent, RecyclerView.State state) {
        super.onDrawOver(canvas, parent, state);

        int itemCount = parent.getAdapter().getItemCount();
        // Don't draw page indicator if there is only one item.
        if (itemCount <= 1) return;

        // The page indicators are center-horizontal. Calculates the total width and subtracts half
        // from the center.
        float dotsTotalLength = mIndicatorItemDiameterPx * itemCount;
        float paddingBetweenItems = Math.max(0, itemCount - 1) * mIndicatorItemPaddingPx;
        float indicatorTotalWidth = dotsTotalLength + paddingBetweenItems;
        float indicatorStartX = (parent.getWidth() - indicatorTotalWidth) / 2f;

        // The page indicators are center-vertical in the allotted space.
        float indicatorPosY = parent.getHeight() - mIndicatorItemDiameterPx;

        drawInactiveIndicators(canvas, indicatorStartX, indicatorPosY, itemCount);

        // Finds the active page which should be highlighted.
        LinearLayoutManager layoutManager = (LinearLayoutManager) parent.getLayoutManager();
        int activePosition = layoutManager.findFirstVisibleItemPosition();
        assert activePosition != RecyclerView.NO_POSITION;

        final View activeChild = layoutManager.findViewByPosition(activePosition);
        int left = activeChild.getLeft() - mStartMarginPx;

        drawHighlights(canvas, indicatorStartX, indicatorPosY, activePosition, left == 0);
    }

    /** Draws the inactive indicator dots. */
    private void drawInactiveIndicators(
            Canvas canvas, float indicatorStartX, float indicatorPosY, int itemCount) {
        mPaint.setColor(mColorInactive);

        // The width of item indicator including padding.
        final float itemWidth = mIndicatorItemDiameterPx + mIndicatorItemPaddingPx;
        float startX = indicatorStartX;
        for (int i = 0; i < itemCount; i++) {
            // Draws the inactive indicator dot.
            canvas.drawCircle(startX, indicatorPosY, mIndicatorRadiusPx, mPaint);
            startX += itemWidth;
        }
    }

    /**
     * Draws the active (highlighted) indicator dot and animation. TODO(https://crbug.com/1512962):
     * Make drawing highlights work on wider screen which could show two items.
     */
    private void drawHighlights(
            Canvas canvas,
            float indicatorStartX,
            float indicatorPosY,
            int highlightPosition,
            boolean drawDot) {
        mPaint.setColor(mColorActive);

        // The width of item indicator including padding.
        final float itemWidth = mIndicatorItemDiameterPx + mIndicatorItemPaddingPx;
        float highlightStart = indicatorStartX + itemWidth * highlightPosition;
        if (drawDot) {
            // No swipe, draw a normal indicator.
            canvas.drawCircle(highlightStart, indicatorPosY, mIndicatorRadiusPx, mPaint);
        } else {
            // Draws a rounded rectangle which starts from the position of the active dot, and ends
            // on the next dot.
            canvas.drawRoundRect(
                    highlightStart - mIndicatorRadiusPx,
                    indicatorPosY - mIndicatorRadiusPx,
                    highlightStart + itemWidth + mIndicatorRadiusPx,
                    indicatorPosY + mIndicatorRadiusPx,
                    mIndicatorRadiusPx,
                    mIndicatorRadiusPx,
                    mPaint);
        }
    }

    @Override
    public void getItemOffsets(
            Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
        super.getItemOffsets(outRect, view, parent, state);

        getItemOffsetsImpl(outRect, view, parent, state);
    }

    @VisibleForTesting
    void getItemOffsetsImpl(
            Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
        outRect.bottom = mIndicatorHeightPx;

        if (!mIsTablet
                || mUiConfig != null && mUiConfig.getCurrentDisplayStyle().isSmall()
                || parent.getAdapter().getItemCount() < 2) return;

        // On wide screen, we will show 2 cards instead of 1 on the magic stack.
        int position = parent.getChildAdapterPosition(view);
        boolean isFirstPosition = position == 0;

        // Updates the width of the card.
        outRect.left = isFirstPosition ? 0 : (int) mIndicatorItemPaddingPx;
        MarginLayoutParams marginLayoutParams = (MarginLayoutParams) view.getLayoutParams();
        int width = (int) (parent.getMeasuredWidth() / 2 - mIndicatorItemPaddingPx);
        marginLayoutParams.width = width;
        view.setMinimumWidth(width);
    }
}
