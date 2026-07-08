// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.BlurMaskFilter;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.graphics.drawable.StateListDrawable;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import androidx.core.content.ContextCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * The root view for the Actor Overlay. Displays the overlay content on top of the browser content.
 */
@NullMarked
public class ActorOverlayView extends FrameLayout {
    private final Paint mSoftGlowPaint;
    private final Paint mStrongGlowPaint;
    private final RectF mGlowRect;

    // Pixel dimensions for the take over button's shadow
    private final float mSoftSpreadX;
    private final float mSoftSpreadY;
    private final float mStrongSpreadX;
    private final float mVOffset;

    private @Nullable View mButton;

    public ActorOverlayView(Context context, AttributeSet attrs) {
        super(context, attrs);
        setClickable(true);
        setFocusable(true);

        InnerGlowDrawable normalDrawable = InnerGlowDrawable.createMainWebpageGlow(context);
        InnerGlowDrawable normalDrawableForHover = InnerGlowDrawable.createMainWebpageGlow(context);

        int hoverColor = ContextCompat.getColor(context, R.color.actor_overlay_hover_color);
        Drawable hoverHighlight = new ColorDrawable(hoverColor);
        Drawable hoverDrawable =
                new LayerDrawable(new Drawable[] {hoverHighlight, normalDrawableForHover});

        StateListDrawable stateListDrawable = new StateListDrawable();
        stateListDrawable.addState(new int[] {android.R.attr.state_hovered}, hoverDrawable);
        stateListDrawable.addState(new int[] {}, normalDrawable);

        setBackground(stateListDrawable);

        Resources res = context.getResources();

        mSoftGlowPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mSoftGlowPaint.setColor(ContextCompat.getColor(context, R.color.actor_button_glow_soft));
        mSoftGlowPaint.setStyle(Paint.Style.FILL);
        float softBlur = res.getDimension(R.dimen.actor_button_glow_soft_blur);
        mSoftGlowPaint.setMaskFilter(new BlurMaskFilter(softBlur, BlurMaskFilter.Blur.NORMAL));

        mStrongGlowPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mStrongGlowPaint.setColor(
                ContextCompat.getColor(context, R.color.actor_button_glow_strong));
        mStrongGlowPaint.setStyle(Paint.Style.FILL);
        float strongBlur = res.getDimension(R.dimen.actor_button_glow_strong_blur);
        mStrongGlowPaint.setMaskFilter(new BlurMaskFilter(strongBlur, BlurMaskFilter.Blur.NORMAL));

        mGlowRect = new RectF();

        // Resolve dimensions for the take over button's shadow
        // (automatically handles density scaling).
        mSoftSpreadX = res.getDimension(R.dimen.actor_button_glow_soft_spread_x);
        mSoftSpreadY = res.getDimension(R.dimen.actor_button_glow_soft_spread_y);
        mStrongSpreadX = res.getDimension(R.dimen.actor_button_glow_strong_spread_x);
        mVOffset = res.getDimension(R.dimen.actor_button_glow_v_offset);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mButton = getTakeOverButton();
    }

    /**
     * Sets the margins of the view.
     *
     * @param left The left margin in pixels.
     * @param top The top margin in pixels.
     * @param right The right margin in pixels.
     * @param bottom The bottom margin in pixels.
     */
    public void setMargins(int left, int top, int right, int bottom) {
        MarginLayoutParams params = (MarginLayoutParams) getLayoutParams();
        if (params == null) return;

        if (params.leftMargin != left
                || params.topMargin != top
                || params.rightMargin != right
                || params.bottomMargin != bottom) {
            params.leftMargin = left;
            params.topMargin = top;
            params.rightMargin = right;
            params.bottomMargin = bottom;
            setLayoutParams(params);
        }
    }

    /** Returns the take over task button. */
    public View getTakeOverButton() {
        return findViewById(R.id.take_over_task_button);
    }

    @Override
    protected void dispatchDraw(Canvas canvas) {
        if (mButton != null && mButton.getVisibility() == View.VISIBLE) {
            float cornerRadius = mButton.getHeight() / 2f;
            int left = mButton.getLeft();
            int right = mButton.getRight();
            float topWithOffset = mButton.getTop() + mVOffset;
            float bottomWithOffset = mButton.getBottom() + mVOffset;

            mGlowRect.set(
                    left - mSoftSpreadX,
                    topWithOffset - mSoftSpreadY,
                    right + mSoftSpreadX,
                    bottomWithOffset + mSoftSpreadY);
            canvas.drawRoundRect(
                    mGlowRect,
                    cornerRadius + mSoftSpreadX,
                    cornerRadius + mSoftSpreadY,
                    mSoftGlowPaint);

            mGlowRect.set(
                    left - mStrongSpreadX,
                    topWithOffset,
                    right + mStrongSpreadX,
                    bottomWithOffset);
            canvas.drawRoundRect(
                    mGlowRect, cornerRadius + mStrongSpreadX, cornerRadius, mStrongGlowPaint);
        }
        super.dispatchDraw(canvas);
    }
}
