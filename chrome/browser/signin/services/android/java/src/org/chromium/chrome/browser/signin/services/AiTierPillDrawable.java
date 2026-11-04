// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BlurMaskFilter;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.drawable.Drawable;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

/**
 * Background of the AI tier pill: a fully rounded fill with a blurred wave at the bottom and an
 * inner shadow. {@link SubscriptionTierBrandingDelegate} provides the wave and the shadow color;
 * without them, only the fill is drawn.
 */
@NullMarked
public class AiTierPillDrawable extends Drawable {
    private static final float WAVE_BLUR_RADIUS_DP = 3f;
    private static final float SHADOW_BLUR_RADIUS_DP = 5f;
    private static final float SHADOW_DX_DP = 0.6f;
    private static final float SHADOW_DY_DP = -0.6f;
    // The inner shadow is the shadow of a frame around the pill. The frame must be wider than the
    // shadow's blur.
    private static final float SHADOW_FRAME_WIDTH_DP = 20f;

    private final SubscriptionTierBrandingDelegate mBrandingDelegate;
    private final float mDensity;
    private final Paint mFillPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mWavePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mShadowPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mBitmapPaint = new Paint();

    // Cached in a bitmap to avoid redrawing the blurred wave and inner shadow on every frame.
    private @Nullable Bitmap mBitmap;

    /**
     * @param context The context to get the fill color and the display density from.
     * @param brandingDelegate Provides the wave and the shadow color.
     */
    public AiTierPillDrawable(Context context, SubscriptionTierBrandingDelegate brandingDelegate) {
        mBrandingDelegate = brandingDelegate;
        mDensity = context.getResources().getDisplayMetrics().density;
        mFillPaint.setColor(SemanticColorUtils.getColorSurfaceContainerLowest(context));
        mWavePaint.setMaskFilter(
                new BlurMaskFilter(
                        Math.max(1f, WAVE_BLUR_RADIUS_DP * mDensity), BlurMaskFilter.Blur.NORMAL));
        // Only the frame's shadow is drawn, not the frame.
        mShadowPaint.setColor(Color.TRANSPARENT);
    }

    @Override
    protected void onBoundsChange(Rect bounds) {
        super.onBoundsChange(bounds);
        if (bounds.isEmpty()) {
            mBitmap = null;
            return;
        }
        // The pill only depends on the size; draw() applies the position.
        if (mBitmap != null
                && mBitmap.getWidth() == bounds.width()
                && mBitmap.getHeight() == bounds.height()) {
            return;
        }
        // Not recycled: a frame that is being drawn may still use the old bitmap.
        mBitmap = drawPill(bounds.width(), bounds.height());
    }

    private Bitmap drawPill(int width, int height) {
        RectF pillBounds = new RectF(0, 0, width, height);
        // Half the height, so that the pill stays fully rounded when it grows with the font size.
        float radius = height / 2f;
        Path pillPath = new Path();
        pillPath.addRoundRect(pillBounds, radius, radius, Path.Direction.CW);

        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        canvas.clipPath(pillPath);
        canvas.drawPath(pillPath, mFillPaint);

        Shader waveShader = mBrandingDelegate.getTierPillWaveShader(pillBounds);
        Path wavePath = mBrandingDelegate.getTierPillWavePath(pillBounds);
        if (waveShader != null && wavePath != null) {
            mWavePaint.setShader(waveShader);
            canvas.drawPath(wavePath, mWavePaint);
        }

        @ColorInt int shadowColor = mBrandingDelegate.getTierPillShadowColor();
        if (Color.alpha(shadowColor) != 0) {
            float frameWidth = SHADOW_FRAME_WIDTH_DP * mDensity;
            RectF frameBounds = new RectF(pillBounds);
            frameBounds.inset(-frameWidth, -frameWidth);
            Path framePath = new Path();
            framePath.addRect(frameBounds, Path.Direction.CW);
            framePath.op(pillPath, Path.Op.DIFFERENCE);
            mShadowPaint.setShadowLayer(
                    SHADOW_BLUR_RADIUS_DP * mDensity,
                    SHADOW_DX_DP * mDensity,
                    SHADOW_DY_DP * mDensity,
                    shadowColor);
            canvas.drawPath(framePath, mShadowPaint);
        }
        return bitmap;
    }

    @Override
    public void draw(Canvas canvas) {
        if (mBitmap == null) return;
        Rect bounds = getBounds();
        canvas.drawBitmap(mBitmap, bounds.left, bounds.top, mBitmapPaint);
    }

    @Override
    public void setAlpha(int alpha) {
        mBitmapPaint.setAlpha(alpha);
        invalidateSelf();
    }

    @Override
    public int getAlpha() {
        return mBitmapPaint.getAlpha();
    }

    @Override
    public void setColorFilter(@Nullable ColorFilter colorFilter) {
        mBitmapPaint.setColorFilter(colorFilter);
        invalidateSelf();
    }

    @Override
    public @Nullable ColorFilter getColorFilter() {
        return mBitmapPaint.getColorFilter();
    }

    @Override
    public int getOpacity() {
        return PixelFormat.TRANSLUCENT;
    }
}
