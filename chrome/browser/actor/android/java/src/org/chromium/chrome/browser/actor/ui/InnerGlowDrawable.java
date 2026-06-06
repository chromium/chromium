// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.content.Context;
import android.graphics.BlurMaskFilter;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;

import androidx.core.content.ContextCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** A custom drawable that draws an inner glow effect using two layers of inner shadows. */
@NullMarked
public class InnerGlowDrawable extends Drawable {
    private final InnerGlowConstantState mState;
    private final Paint mSoftGlowPaint;
    private final Paint mStrongOutlinePaint;
    private boolean mMutated;

    private static class InnerGlowConstantState extends ConstantState {
        public final int color;
        public final float softBlur;
        public final float softSpread;
        public final float softOpacity;
        public final float strongBlur;
        public final float strongSpread;
        public final float strongOpacity;

        InnerGlowConstantState(
                int color,
                float softBlur,
                float softSpread,
                float softOpacity,
                float strongBlur,
                float strongSpread,
                float strongOpacity) {
            this.color = color;
            this.softBlur = softBlur;
            this.softSpread = softSpread;
            this.softOpacity = softOpacity;
            this.strongBlur = strongBlur;
            this.strongSpread = strongSpread;
            this.strongOpacity = strongOpacity;
        }

        @Override
        public Drawable newDrawable() {
            return new InnerGlowDrawable(this);
        }

        @Override
        public int getChangingConfigurations() {
            return 0;
        }
    }

    /**
     * Creates an InnerGlowDrawable with the specified parameters.
     *
     * @param context The context to get display metrics and resources from.
     * @param color The color of the glow.
     * @param softBlurRes The blur radius of the soft glow.
     * @param softSpreadRes The spread of the soft glow.
     * @param softOpacity The opacity of the soft glow (0.0 to 1.0).
     * @param strongBlurRes The blur radius of the strong outline.
     * @param strongSpreadRes The spread of the strong outline.
     * @param strongOpacity The opacity of the strong outline (0.0 to 1.0).
     */
    public InnerGlowDrawable(
            Context context,
            int color,
            int softBlurRes,
            int softSpreadRes,
            float softOpacity,
            int strongBlurRes,
            int strongSpreadRes,
            float strongOpacity) {
        this(
                new InnerGlowConstantState(
                        color,
                        context.getResources().getDimension(softBlurRes),
                        context.getResources().getDimension(softSpreadRes),
                        softOpacity,
                        context.getResources().getDimension(strongBlurRes),
                        context.getResources().getDimension(strongSpreadRes),
                        strongOpacity));
    }

    private InnerGlowDrawable(InnerGlowConstantState state) {
        mState = state;

        mSoftGlowPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mSoftGlowPaint.setStyle(Paint.Style.STROKE);
        mSoftGlowPaint.setColor(state.color);
        mSoftGlowPaint.setAlpha((int) (255 * state.softOpacity));
        // Spread in spec means the stroke must be 2x wide because the stroke is drawn centered on
        // the line.
        mSoftGlowPaint.setStrokeWidth(state.softSpread * 2);
        mSoftGlowPaint.setMaskFilter(
                new BlurMaskFilter(state.softBlur, BlurMaskFilter.Blur.NORMAL));

        mStrongOutlinePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mStrongOutlinePaint.setStyle(Paint.Style.STROKE);
        mStrongOutlinePaint.setColor(state.color);
        mStrongOutlinePaint.setAlpha((int) (255 * state.strongOpacity));
        mStrongOutlinePaint.setStrokeWidth(state.strongSpread * 2);
        mStrongOutlinePaint.setMaskFilter(
                new BlurMaskFilter(state.strongBlur, BlurMaskFilter.Blur.NORMAL));
    }

    /**
     * Creates an InnerGlowDrawable for the main webpage glow.
     *
     * @param context The context to get display metrics and resources from.
     * @return An InnerGlowDrawable configured for the main webpage.
     */
    public static InnerGlowDrawable createMainWebpageGlow(Context context) {
        return new InnerGlowDrawable(
                context,
                ContextCompat.getColor(context, R.color.baseline_primary_40),
                /* softBlurRes= */ R.dimen.actor_glow_soft_blur,
                /* softSpreadRes= */ R.dimen.actor_glow_soft_spread,
                /* softOpacity= */ 0.4f,
                /* strongBlurRes= */ R.dimen.actor_glow_strong_blur,
                /* strongSpreadRes= */ R.dimen.actor_glow_strong_spread,
                /* strongOpacity= */ 1.0f);
    }

    /**
     * Creates an InnerGlowDrawable for the GTS preview glow.
     *
     * @param context The context to get display metrics and resources from.
     * @return An InnerGlowDrawable configured for the GTS preview.
     */
    public static InnerGlowDrawable createGtsPreviewGlow(Context context) {
        return new InnerGlowDrawable(
                context,
                ContextCompat.getColor(context, R.color.baseline_primary_40),
                /* softBlurRes= */ R.dimen.actor_gts_glow_soft_blur,
                /* softSpreadRes= */ R.dimen.actor_gts_glow_soft_spread,
                /* softOpacity= */ 0.5f,
                /* strongBlurRes= */ R.dimen.actor_gts_glow_strong_blur,
                /* strongSpreadRes= */ R.dimen.actor_gts_glow_strong_spread,
                /* strongOpacity= */ 1.0f);
    }

    @Override
    public void draw(Canvas canvas) {
        RectF rect = new RectF(getBounds());
        // Draw the large soft glow first.
        canvas.drawRect(rect, mSoftGlowPaint);
        // Draw the tighter, stronger outline on top.
        canvas.drawRect(rect, mStrongOutlinePaint);
    }

    @Override
    public void setAlpha(int alpha) {
        // Standard drawable override, though we set alpha in constructor.
    }

    @Override
    public void setColorFilter(@Nullable ColorFilter colorFilter) {
        mSoftGlowPaint.setColorFilter(colorFilter);
        mStrongOutlinePaint.setColorFilter(colorFilter);
    }

    @Override
    public int getOpacity() {
        return PixelFormat.TRANSLUCENT;
    }

    @Override
    public @Nullable ConstantState getConstantState() {
        return mState;
    }

    @Override
    public Drawable mutate() {
        if (!mMutated && super.mutate() == this) {
            mMutated = true;
        }
        return this;
    }
}
