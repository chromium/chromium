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

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.core.text.TextUtilsCompat;
import androidx.core.view.ViewCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.components.browser_ui.widget.displaystyle.UiConfig.DisplayStyle;

import java.util.Locale;

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

    private final Paint mPaint = new Paint();
    private final boolean mIsTablet;

    private final boolean mIsLeftToRight;

    /** The start margin of the recyclerview in pixel. */
    private int mStartMarginPx;

    /** The value is updated for tablets when displayStyle is changed. */
    private int mItemPerScreen;

    /**
     * @param context The {@link Context} that the application is running.
     * @param startMarginPx The start margin of the first item of the recyclerview.
     */
    public CirclePagerIndicatorDecoration(
            @NonNull Context context,
            int startMarginPx,
            int colorActive,
            int colorInactive,
            boolean isTablet) {
        mStartMarginPx = startMarginPx;
        mColorActive = colorActive;
        mColorInactive = colorInactive;
        mIsTablet = isTablet;

        Resources resources = context.getResources();
        mIndicatorStrokeWidthPx = resources.getDimensionPixelSize(R.dimen.page_indicator_dot_size);
        mPaint.setStrokeWidth(mIndicatorStrokeWidthPx);
        mPaint.setStyle(Style.STROKE);
        mPaint.setAntiAlias(true);
        mItemPerScreen = 1;

        mIndicatorItemPaddingPx =
                (float) resources.getDimensionPixelSize(R.dimen.page_indicator_internal_padding);
        mIndicatorItemDiameterPx = mIndicatorStrokeWidthPx;
        mIndicatorRadiusPx = mIndicatorItemDiameterPx / 2f;
        mIndicatorHeightPx =
                (int) mIndicatorItemDiameterPx
                        + resources.getDimensionPixelSize(R.dimen.page_indicator_top_margin);

        mIsLeftToRight =
                TextUtilsCompat.getLayoutDirectionFromLocale(Locale.getDefault())
                        == ViewCompat.LAYOUT_DIRECTION_LTR;
    }

    @Override
    public void onDrawOver(Canvas canvas, RecyclerView parent, RecyclerView.State state) {
        super.onDrawOver(canvas, parent, state);

        int itemCount = parent.getAdapter().getItemCount();
        // Don't draw a page indicator if all of the items can fit in one screen.
        if (itemCount <= mItemPerScreen) return;

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
        int dotHighlightPosition = activePosition;
        assert activePosition != RecyclerView.NO_POSITION;

        final View activeChild = layoutManager.findViewByPosition(activePosition);
        // It is possible that the activeChild is null, see b/363959953.
        if (activeChild == null) {
            StringBuilder message =
                    new StringBuilder("The activePosition of the RecyclerView is :");
            message.append(activePosition);
            message.append(", the original item count is :");
            message.append(itemCount);
            message.append(", the current item count is:");
            message.append(parent.getAdapter().getItemCount());

            assert false : message.toString();
            return;
        }

        // The left offset of the first visible view. We always track the first visible view to get
        // a consistent offset.
        int left = activeChild.getLeft() - mStartMarginPx;
        if ((left != 0 || activePosition != 0) && isMultiItemPerScreen()) {
            // When multiple items are visible on the screen, the last completely visible view is
            // highlighted, rather than the first visible view unless it is the first one and the
            // recyclerview hasn't been scrolled yet. This allows to highlight the dot of the last
            // view if the recyclerview can't be scrolled any further.
            dotHighlightPosition = layoutManager.findLastCompletelyVisibleItemPosition();
        }

        // When multiple items are shown per screen, we only highlight a dot but don't draw any
        // animation when scrolling.
        boolean showDot = isMultiItemPerScreen() ? true : left == 0;
        drawHighlights(canvas, indicatorStartX, indicatorPosY, dotHighlightPosition, showDot);
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

    /** Draws the active (highlighted) indicator dot and animation. */
    private void drawHighlights(
            Canvas canvas,
            float indicatorStartX,
            float indicatorPosY,
            int highlightPosition,
            boolean drawDot) {
        mPaint.setColor(mColorActive);

        // The width of an indicator dot with padding.
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

    /** Returns whether to show more than a single item per screen. */
    private boolean isMultiItemPerScreen() {
        return mItemPerScreen > 1;
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
        int itemCount = parent.getAdapter().getItemCount();
        // If all of the items can fit in one screen, remove the space for page indicators since
        // they are hidden.
        outRect.bottom = itemCount <= mItemPerScreen ? 0 : mIndicatorHeightPx;

        // If showing one card per screen, the view's width should match the parent recyclerview.
        // Thus, we don't need to add extra padding on the left side of any card.
        if (!mIsTablet || mItemPerScreen == 1 || itemCount == 1) return;

        // On a wide screen, we will show 2 cards instead of 1 per screen. The card's width is
        // calculated and doesn't match parent's width any more. Add a padding on the left side of
        // any card except the first one if it is a left to right language; on the right side if it
        // is a right to left language.
        int padding = parent.getChildAdapterPosition(view) == 0 ? 0 : (int) mIndicatorItemPaddingPx;
        if (mIsLeftToRight) {
            outRect.left = padding;
        } else {
            outRect.right = padding;
        }
    }

    void onDisplayStyleChanged(int startMarginPx, int itemPerScreen) {
        mStartMarginPx = startMarginPx;
        mItemPerScreen = itemPerScreen;
    }

    /** Returns how many items are shown per screen. */
    static int getItemPerScreen(DisplayStyle displayStyle) {
        return displayStyle.isWide() ? 2 : 1;
    }

    void setItemPerScreenForTesting(int itemPerScreen) {
        mItemPerScreen = itemPerScreen;
    }
}
