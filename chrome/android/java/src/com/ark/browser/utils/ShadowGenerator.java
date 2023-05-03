package com.ark.browser.utils;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.BlurMaskFilter;
import android.graphics.BlurMaskFilter.Blur;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.RectF;
import androidx.core.graphics.ColorUtils;

import com.android.launcher3.LauncherAppState;

/**
 * Utility class to add shadows to bitmaps.
 */
public class ShadowGenerator {

    // Percent of actual icon size
    private static final float HALF_DISTANCE = 0.5f;
    public static final float BLUR_FACTOR = 3f / 48; // 0.5f

    // Percent of actual icon size
    public static final float KEY_SHADOW_DISTANCE = 1f / 48;
    private static final int KEY_SHADOW_ALPHA = 61;

    private static final int AMBIENT_SHADOW_ALPHA = 30;

    private final int mIconSize;

    private final Paint mBlurPaint;
    private final Paint mDrawPaint;
    private final BlurMaskFilter mDefaultBlurMaskFilter;

    public ShadowGenerator(Context context) {
        mIconSize = (int) LauncherAppState.getIDP(context).iconSize;
        mBlurPaint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
        mDrawPaint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
        mDefaultBlurMaskFilter = new BlurMaskFilter(mIconSize * BLUR_FACTOR, Blur.NORMAL);
    }

    public synchronized void recreateIcon(Bitmap icon, Canvas out) {
        recreateIcon(icon, mDefaultBlurMaskFilter, AMBIENT_SHADOW_ALPHA, KEY_SHADOW_ALPHA, out);
    }

    public synchronized void recreateIcon(Bitmap icon, BlurMaskFilter blurMaskFilter,
                                          int ambientAlpha, int keyAlpha, Canvas out) {
        int[] offset = new int[2];
        mBlurPaint.setMaskFilter(blurMaskFilter);
        Bitmap shadow = icon.extractAlpha(mBlurPaint, offset);

        // Draw ambient shadow
        mDrawPaint.setAlpha(ambientAlpha);
        out.drawBitmap(shadow, offset[0], offset[1], mDrawPaint);

        // Draw key shadow
        mDrawPaint.setAlpha(keyAlpha);
        out.drawBitmap(shadow, offset[0], offset[1] + KEY_SHADOW_DISTANCE * mIconSize, mDrawPaint);

        // Draw the icon
        mDrawPaint.setAlpha(255);
        out.drawBitmap(icon, 0, 0, mDrawPaint);
    }

    /**
     * Returns the minimum amount by which an icon with {@param bounds} should be scaled
     * so that the shadows do not get clipped.
     */
    public static float getScaleForBounds(RectF bounds) {
        float scale = 1;

        // For top, left & right, we need same space.
        float minSide = Math.min(Math.min(bounds.left, bounds.right), bounds.top);
        if (minSide < BLUR_FACTOR) {
            scale = (HALF_DISTANCE - BLUR_FACTOR) / (HALF_DISTANCE - minSide);
        }

        float bottomSpace = BLUR_FACTOR + KEY_SHADOW_DISTANCE;
        if (bounds.bottom < bottomSpace) {
            scale = Math.min(scale, (HALF_DISTANCE - bottomSpace) / (HALF_DISTANCE - bounds.bottom));
        }
        return scale;
    }

    public static class Builder {

        public final RectF bounds = new RectF();
        public final int color;

        public int ambientShadowAlpha = AMBIENT_SHADOW_ALPHA;

        public float shadowBlur;

        public float keyShadowDistance;
        public int keyShadowAlpha = KEY_SHADOW_ALPHA;
        public float radius;

        public Builder(int color) {
            this.color = color;
        }

        public Builder setupBlurForSize(int height) {
            shadowBlur = height * 1f / 32;
            keyShadowDistance = height * 1f / 16;
            return this;
        }

        public Bitmap createPill(int width, int height) {
            radius = height / 2;

            int centerX = Math.round(width / 2 + shadowBlur);
            int centerY = Math.round(radius + shadowBlur + keyShadowDistance);
            int center = Math.max(centerX, centerY);
            bounds.set(0, 0, width, height);
            bounds.offsetTo(center - width / 2, center - height / 2);

            int size = center * 2;
            Bitmap result = Bitmap.createBitmap(size, size, Config.ARGB_8888);
            drawShadow(new Canvas(result));
            return result;
        }

        public void drawShadow(Canvas c) {
            Paint p = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
            p.setColor(color);

            // Key shadow
            p.setShadowLayer(shadowBlur, 0, keyShadowDistance,
                    ColorUtils.setAlphaComponent(Color.BLACK, keyShadowAlpha));
            c.drawRoundRect(bounds, radius, radius, p);

            // Ambient shadow
            p.setShadowLayer(shadowBlur, 0, 0,
                    ColorUtils.setAlphaComponent(Color.BLACK, ambientShadowAlpha));
            c.drawRoundRect(bounds, radius, radius, p);

            if (Color.alpha(color) < 255) {
                // Clear any content inside the pill-rect for translucent fill.
                p.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.CLEAR));
                p.clearShadowLayer();
                p.setColor(Color.BLACK);
                c.drawRoundRect(bounds, radius, radius, p);

                p.setXfermode(null);
                p.setColor(color);
                c.drawRoundRect(bounds, radius, radius, p);
            }
        }
    }
}

