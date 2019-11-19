// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.carousel;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Region;
import android.graphics.drawable.Drawable;
import android.support.v7.widget.OrientationHelper;
import android.support.v7.widget.RecyclerView;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.autofill_assistant.R;

/**
 * Decoration added to the actions carousel that add offsets to each action to have the right inner
 * and outer spaces. When the carousel is scrollable, this decoration also:
 * - adds a white-to-transparent gradient to the left of the screen above all but the last action.
 * - adds a shadow to the last action when it's above other actions.
 * - fades actions the more they get close to the left screen.
 */
class AssistantActionsDecoration extends RecyclerView.ItemDecoration {
    // The maximum opacity of the overlays drawn on top of children next to the last child,
    // where 0 means transparent and 1.0 fully opaque.
    private static final float OVERLAYS_MAX_OPACITY = 0.30f;

    // The shadow around the last button is composed of SHADOW_LAYERS layers with decreasing
    // opacity.
    private static final int SHADOW_LAYERS = 4;

    private final AssistantActionsCarouselCoordinator.CustomLayoutManager mLayoutManager;

    // Dimensions in device pixels.
    private final int mOuterSpace;
    private final int mInnerSpace;
    private final int mVerticalSpacing;
    private final int mVerticalInset;
    private final int mGradientWidth;
    private final int mLastChildBorderRadius;
    private final int mShadowColor;
    private final float mShadowLayerWidth;

    private final Drawable mGradientDrawable;
    private final Paint mShadowPaint = new Paint();
    private final Paint mOverlayPaint = new Paint();
    private final Path mLastChildPath = new Path();
    private final RectF mLastChildRect = new RectF();
    private final RectF mChildRect = new RectF();

    public AssistantActionsDecoration(Context context,
            AssistantActionsCarouselCoordinator.CustomLayoutManager layoutManager) {
        mLayoutManager = layoutManager;
        mOuterSpace = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_bottombar_horizontal_spacing);
        mInnerSpace = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_actions_spacing);
        mVerticalInset = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_button_bg_vertical_inset);

        // We remove mVerticalInset from the vertical spacing as that inset will be added above and
        // below the button by ButtonView.
        mVerticalSpacing = context.getResources().getDimensionPixelSize(
                                   R.dimen.autofill_assistant_bottombar_vertical_spacing)
                - mVerticalInset;

        mGradientWidth = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_actions_gradient_width);
        mLastChildBorderRadius = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_button_corner_radius);
        mShadowColor = ApiCompatibilityUtils.getColor(
                context.getResources(), R.color.autofill_assistant_actions_shadow_color);
        mShadowLayerWidth =
                context.getResources().getDimension(R.dimen.autofill_assistant_actions_shadow_width)
                / SHADOW_LAYERS;
        mGradientDrawable =
                context.getResources().getDrawable(R.drawable.autofill_assistant_actions_gradient);

        mShadowPaint.setAntiAlias(true);
        mShadowPaint.setDither(true);
        mShadowPaint.setStyle(Paint.Style.STROKE);
        mShadowPaint.setStrokeWidth(mShadowLayerWidth);

        mOverlayPaint.setColor(
                ApiCompatibilityUtils.getColor(context.getResources(), R.color.sheet_bg_color));
    }

    @Override
    public void onDrawOver(@NonNull Canvas canvas, @NonNull RecyclerView parent,
            @NonNull RecyclerView.State state) {
        if (parent.getChildCount() <= 1) {
            return;
        }

        View lastChild = parent.getChildAt(parent.getChildCount() - 1);
        mLastChildRect.left = lastChild.getLeft() + lastChild.getTranslationX();
        mLastChildRect.top = lastChild.getTop() + lastChild.getTranslationY() + mVerticalInset;
        mLastChildRect.right = lastChild.getRight() + lastChild.getTranslationX();
        mLastChildRect.bottom =
                lastChild.getBottom() + lastChild.getTranslationY() - mVerticalInset;

        canvas.save();

        // Don't draw on the last child.
        mLastChildPath.reset();
        mLastChildPath.addRoundRect(
                mLastChildRect, mLastChildBorderRadius, mLastChildBorderRadius, Path.Direction.CW);
        canvas.clipPath(mLastChildPath, Region.Op.DIFFERENCE);

        // Overlay children close to the last child with a semi-transparent paint.
        OrientationHelper orientationHelper = mLayoutManager.mOrientationHelper;
        float lastChildRight = orientationHelper.getDecoratedEnd(lastChild);
        for (int i = parent.getChildCount() - 2; i >= 0; i--) {
            View child = parent.getChildAt(i);
            int left = Math.round(
                    orientationHelper.getDecoratedStart(child) + lastChild.getTranslationX());
            if (left >= lastChildRight) {
                break;
            }

            int alpha = Math.round(
                    getBoundedLinearValue(left, 0, lastChildRight, OVERLAYS_MAX_OPACITY * 255, 0));
            mOverlayPaint.setColor(getColorWithAlpha(mOverlayPaint.getColor(), alpha));
            mChildRect.left = child.getLeft() + child.getTranslationX();
            mChildRect.right = child.getRight() + child.getTranslationX();
            mChildRect.top = child.getTop() + child.getTranslationY();
            mChildRect.bottom = child.getBottom() + child.getTranslationY();
            canvas.drawRect(mChildRect, mOverlayPaint);
        }

        View beforeLastChild = parent.getChildAt(parent.getChildCount() - 2);

        // Draw a fixed size white-to-transparent linear gradient from left to right.
        mGradientDrawable.setBounds(
                0, mVerticalSpacing, mGradientWidth, parent.getHeight() - mVerticalSpacing);
        mGradientDrawable.setAlpha(Math.round(getBoundedLinearValue(
                beforeLastChild.getLeft(), lastChild.getLeft(), lastChild.getRight(), 255, 0)));
        mGradientDrawable.draw(canvas);

        canvas.restore();

        // Don't draw shadow around the last if it's animated.
        if (orientationHelper.getDecoratedStart(lastChild) != 0
                || lastChild.getTranslationX() != 0) {
            return;
        }

        // Draw shadow composed of 4 layers of colors around the last child. We multiply the
        // original alpha of the shadow color by alphaRatio to hide the shadow when there is no
        // child behind the last child.
        float alphaRatio = getBoundedLinearValue(
                beforeLastChild.getLeft(), lastChild.getLeft(), lastChild.getRight(), 1, 0);

        // originalAlpha is a value between 0 (transparent) and 255 (opaque).
        int originalAlpha = mShadowColor >>> 24;
        for (int i = 0; i < SHADOW_LAYERS; i++) {
            // The higher the layer, the lower its alpha.
            float layerAlphaRatio = ((float) SHADOW_LAYERS - i) / SHADOW_LAYERS;

            int color = getColorWithAlpha(
                    mShadowColor, Math.round(originalAlpha * alphaRatio * layerAlphaRatio));
            mShadowPaint.setColor(color);

            mChildRect.left = mLastChildRect.left - (i + 0.5f) * mShadowLayerWidth;
            mChildRect.right = mLastChildRect.right + (i + 0.5f) * mShadowLayerWidth;
            mChildRect.top = mLastChildRect.top - (i + 0.5f) * mShadowLayerWidth;
            mChildRect.bottom = mLastChildRect.bottom + (i + 0.5f) * mShadowLayerWidth;
            canvas.drawRoundRect(
                    mChildRect, mLastChildBorderRadius, mLastChildBorderRadius, mShadowPaint);
        }
    }

    private int getColorWithAlpha(int color, int alpha) {
        return (color & 0x00FFFFFF) | (alpha << 24);
    }

    @Override
    public void getItemOffsets(@NonNull Rect outRect, @NonNull View view,
            @NonNull RecyclerView parent, @NonNull RecyclerView.State state) {
        // Add vertical spacing above and below the view. This is necessary to display the shadow of
        // the last button correctly.
        outRect.top = mVerticalSpacing;
        outRect.bottom = mVerticalSpacing;

        if (state.getItemCount() <= 1) {
            return;
        }

        int position = parent.getChildAdapterPosition(view);

        // If old position != NO_POSITION, it means the carousel is being animated and we should
        // use that position in our logic.
        RecyclerView.ViewHolder viewHolder = parent.getChildViewHolder(view);
        if (viewHolder != null && viewHolder.getOldPosition() != RecyclerView.NO_POSITION) {
            position = viewHolder.getOldPosition();
        }

        if (position == RecyclerView.NO_POSITION) {
            return;
        }

        outRect.right = position == 0 ? mOuterSpace : mInnerSpace;
        if (position == state.getItemCount() - 1) {
            outRect.left = mOuterSpace;
        }
    }

    /**
     * Get the value f(x) such that:
     *  - f(x) = yMin for all x <= xMin
     *  - f(x) = yMax for all x >= xMax
     *  - (x, f(x)) is a point on the line drawn between (xMin, yMin) and (xMax, yMax)
     */
    private static float getBoundedLinearValue(
            float x, float xMin, float xMax, float yMin, float yMax) {
        if (x <= xMin) {
            return yMin;
        } else if (x >= xMax) {
            return yMax;
        } else {
            return yMin + (x - xMin) / (xMax - xMin) * (yMax - yMin);
        }
    }
}
