// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.BlurMaskFilter;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.View;
import android.widget.FrameLayout;

import androidx.core.content.ContextCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Container view for the Actor handoff ("Take over task") button that renders a custom glow shadow
 * behind the button.
 */
@NullMarked
public class ActorHandoffButtonView extends FrameLayout {
    private final Paint mSoftGlowPaint;
    private final Paint mStrongGlowPaint;
    private final RectF mGlowRect;

    private final float mSoftSpreadX;
    private final float mSoftSpreadY;
    private final float mStrongSpreadX;
    private final float mVOffset;
    private final float mCornerRadius;

    private @Nullable View mButton;

    public ActorHandoffButtonView(Context context, AttributeSet attrs) {
        super(context, attrs);

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

        mSoftSpreadX = res.getDimension(R.dimen.actor_button_glow_soft_spread_x);
        mSoftSpreadY = res.getDimension(R.dimen.actor_button_glow_soft_spread_y);
        mStrongSpreadX = res.getDimension(R.dimen.actor_button_glow_strong_spread_x);
        mVOffset = res.getDimension(R.dimen.actor_button_glow_v_offset);
        mCornerRadius = res.getDimension(R.dimen.actor_overlay_button_corner_radius);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mButton = findViewById(R.id.take_over_task_button);
        if (mButton != null) {
            mButton.setPointerIcon(PointerIcon.getSystemIcon(getContext(), PointerIcon.TYPE_HAND));
        }
    }

    /** Returns the take over task button. */
    public @Nullable View getButton() {
        return mButton;
    }

    @Override
    @SuppressLint("ClickableViewAccessibility")
    public boolean onTouchEvent(MotionEvent event) {
        // Do not consume touches outside the button so underlying views can receive them.
        return false;
    }

    @Override
    protected void dispatchDraw(Canvas canvas) {
        if (mButton != null && mButton.getVisibility() == View.VISIBLE) {
            float cornerRadius = Math.min(mButton.getHeight() / 2f, mCornerRadius);
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
                    left - mStrongSpreadX, topWithOffset, right + mStrongSpreadX, bottomWithOffset);
            canvas.drawRoundRect(
                    mGlowRect, cornerRadius + mStrongSpreadX, cornerRadius, mStrongGlowPaint);
        }
        super.dispatchDraw(canvas);
    }
}
