// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Paint.Style;
import android.util.AttributeSet;
import android.view.View;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.base.ViewUtils;

/** Page indicator view displaying dot indicators for the Safety Promo Carousel. */
@NullMarked
public class SafetyPromoPageIndicatorView extends View {
    private final @ColorInt int mColorActive;
    private final @ColorInt int mColorInactive;

    private final int mDotDiameterPx;
    private final float mDotRadiusPx;
    private final int mDotPaddingPx;
    private final int mStepXPx;

    private final Paint mPaint = new Paint();

    private int mItemCount;
    private int mActivePosition;

    private int mTotalDotWidthPx;

    public SafetyPromoPageIndicatorView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);

        mColorActive = SemanticColorUtils.getColorOutline(context);
        mColorInactive = SemanticColorUtils.getColorOutlineVariant(context);

        mDotDiameterPx =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.safety_fre_carousel_page_indicator_dot_size);
        mDotRadiusPx = mDotDiameterPx / 2f;
        mDotPaddingPx =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.safety_fre_carousel_page_indicator_padding);
        mStepXPx = mDotDiameterPx + mDotPaddingPx;

        mPaint.setStyle(Style.FILL);
        mPaint.setAntiAlias(true);
    }

    public void setPageCount(int count) {
        if (mItemCount == count) return;
        mItemCount = count;

        mTotalDotWidthPx =
                mItemCount > 1 ? mItemCount * mDotDiameterPx + (mItemCount - 1) * mDotPaddingPx : 0;

        ViewUtils.requestLayout(this, "SafetyPromoPageIndicatorView.setPageCount");
        invalidate();
    }

    public void setActivePosition(int position) {
        if (mActivePosition == position) return;
        mActivePosition = position;
        invalidate();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (mItemCount <= 1) {
            setMeasuredDimension(0, 0);
            return;
        }

        int desiredWidth = mTotalDotWidthPx + getPaddingLeft() + getPaddingRight();
        int desiredHeight = mDotDiameterPx + getPaddingTop() + getPaddingBottom();

        setMeasuredDimension(
                resolveSizeAndState(desiredWidth, widthMeasureSpec, 0),
                resolveSizeAndState(desiredHeight, heightMeasureSpec, 0));
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        if (mItemCount <= 1) return;

        float startX = (getWidth() - mTotalDotWidthPx) / 2f + mDotRadiusPx;
        float posY = getHeight() / 2f;

        int visualPosition = mActivePosition;
        if (getLayoutDirection() == LAYOUT_DIRECTION_RTL) {
            visualPosition = mItemCount - 1 - mActivePosition;
        }

        for (int i = 0; i < mItemCount; i++) {
            mPaint.setColor(i == visualPosition ? mColorActive : mColorInactive);
            canvas.drawCircle(startX + i * mStepXPx, posY, mDotRadiusPx, mPaint);
        }
    }

    int getPageCountForTesting() {
        return mItemCount;
    }

    int getActivePositionForTesting() {
        return mActivePosition;
    }
}
