// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import android.animation.Animator;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.LinearGradient;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Shader;
import android.graphics.drawable.Drawable;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.animation.Interpolator;

import androidx.annotation.ColorInt;
import androidx.annotation.FloatRange;
import androidx.annotation.IntRange;
import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;
import androidx.core.view.animation.PathInterpolatorCompat;

import com.google.android.material.color.MaterialColors;

import org.chromium.build.annotations.UsedByReflection;
import org.chromium.chrome.browser.night_mode.GlobalNightModeStateProviderHolder;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.util.ColorUtils;

/** The {@link Drawable} that will be used to run animations for Quick Delete. */
public class QuickDeleteAnimationGradientDrawable extends Drawable {
    private static final String TAG = "QuickDeleteAnimationGradientDrawable";

    /**
     * This multiplier will be used to calculate the height of the gradient relative to tab grid
     * height. The tab grid height changes depending on the screen size, orientation, multi-window
     * mode...
     */
    @FloatRange(from = 0.0F, to = 1.0F)
    private static final float QUICK_DELETE_WIPE_GRADIENT_HEIGHT_MULTIPLIER = 0.35F;

    /**
     * This multiplier will be used to calculate the intersection point between the wipe and fade
     * animation relative to tab grid height. The tab grid height changes depending on the screen
     * size, orientation, multi-window mode...
     */
    @FloatRange(from = 0.0F, to = 1.0F)
    private static final float QUICK_DELETE_ANIMATION_INTERSECTION_MULTIPLIER = 0.5F;

    @IntRange(from = 0L, to = 255L)
    private static final int QUICK_DELETE_GRADIENT_DARK_MODE_MAX_ALPHA = 77;

    @IntRange(from = 0L, to = 255L)
    private static final int QUICK_DELETE_GRADIENT_LIGHT_MODE_MAX_ALPHA = 64;

    private static final int QUICK_DELETE_GRADIENT_EASING_POINTS_NUM = 20;
    private static final Interpolator QUICK_DELETE_WIPE_ANIMATION_INTERPOLATOR =
            PathInterpolatorCompat.create(0.25F, 0F, 0.15F, 1F);
    private static final int QUICK_DELETE_WIPE_ANIMATION_TIME_MS = 1200;
    private static final int QUICK_DELETE_FADE_ANIMATION_TIME_MS = 230;
    private final @NonNull Paint mPaint;
    private final @NonNull LinearGradient mShader;

    /* The value that will be used to translate the gradient across the bounds of a view. */
    private float mTranslationY;

    /**
     * Creates a new {@link QuickDeleteAnimationGradientDrawable} for the wipe animation.
     *
     * @param context The associated {@link Context}.
     * @param tabGridHeight The height of the tab grid. This will be used to determine the height of
     *     the gradient.
     * @param isIncognito Whether the surface is in incognito mode.
     */
    public static QuickDeleteAnimationGradientDrawable createQuickDeleteWipeAnimationDrawable(
            @NonNull Context context, int tabGridHeight, boolean isIncognito) {
        int gradientColor =
                isIncognito
                        ? ContextCompat.getColor(context, R.color.baseline_primary_80)
                        : MaterialColors.getColor(context, R.attr.colorPrimary, TAG);
        boolean useDarkTheme =
                isIncognito || GlobalNightModeStateProviderHolder.getInstance().isInNightMode();

        int h = QUICK_DELETE_GRADIENT_EASING_POINTS_NUM;
        int k =
                useDarkTheme
                        ? QUICK_DELETE_GRADIENT_DARK_MODE_MAX_ALPHA
                        : QUICK_DELETE_GRADIENT_LIGHT_MODE_MAX_ALPHA;
        int[] colors = new int[h + 1];
        for (int i = 0; i <= h; ++i) {
            // Quadratic equation to calculate the alpha value at each easing point to achieve a
            // smoother transition between the colors of the gradient. This should map to an
            // inverted parabola where the vertical shift corresponds to the maximum alpha value and
            // the horizontal shift corresponds to the number of easing points (number of colors) of
            // the gradient, while maintaining an intersection point at (0,0).
            float alphaValue = -4F * k / (h * h) * (i - h / 2F) * (i - h / 2F) + k;
            colors[i] = ColorUtils.setAlphaComponent(gradientColor, Math.round(alphaValue));
        }

        int gradientHeight = (int) (tabGridHeight * QUICK_DELETE_WIPE_GRADIENT_HEIGHT_MULTIPLIER);

        return new QuickDeleteAnimationGradientDrawable(
                context, colors, /* positions= */ null, gradientHeight);
    }

    /**
     * Creates a new {@link QuickDeleteAnimationGradientDrawable} for the fade animation.
     *
     * @param context The associated {@link Context}.
     * @param tabHeight The height of the tab in the tab grid. This will be used to determine the
     *     height of the gradient.
     * @param isIncognito Whether the surface is in incognito mode.
     */
    public static QuickDeleteAnimationGradientDrawable createQuickDeleteFadeAnimationDrawable(
            @NonNull Context context, int tabHeight, boolean isIncognito) {
        // The color of the background behind the tab.
        int backgroundColor = ChromeColors.getPrimaryBackgroundColor(context, isIncognito);

        int[] colors = new int[] {Color.TRANSPARENT, backgroundColor, backgroundColor};

        return new QuickDeleteAnimationGradientDrawable(
                context, colors, /* positions= */ null, tabHeight);
    }

    /**
     * Creates an instance of QuickDeleteAnimationGradientDrawable with specified height and colors.
     *
     * @param context The associated {@link Context}.
     * @param colors The sRGB colors to be distributed along the gradient line.
     * @param positions May be null. The relative positions [0..1] of each corresponding color in
     *     the colors array. If this is null, the the colors are distributed evenly along the
     *     gradient line.
     * @param gradientHeight The height of the gradient.
     */
    public QuickDeleteAnimationGradientDrawable(
            @NonNull Context context,
            @NonNull @ColorInt int[] colors,
            @Nullable float[] positions,
            int gradientHeight) {
        super();
        assert positions == null || colors.length == positions.length;
        float gradientHeightInPixels = dpToPixels(context, gradientHeight);

        mShader =
                new LinearGradient(
                        0.0F,
                        0.0F,
                        0.0F,
                        gradientHeightInPixels,
                        colors,
                        positions,
                        Shader.TileMode.CLAMP);
        mPaint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.DITHER_FLAG | Paint.FILTER_BITMAP_FLAG);
        mPaint.setStyle(Paint.Style.FILL);
        mPaint.setShader(mShader);
    }

    /**
     * @param parentViewHeight The height of the view that the animation should run on.
     * @return The {@link Animator} that can be used to start the wipe animation.
     */
    public ObjectAnimator createWipeAnimator(int parentViewHeight) {
        ObjectAnimator animator = createAnimator(parentViewHeight, -parentViewHeight);
        animator.setInterpolator(QUICK_DELETE_WIPE_ANIMATION_INTERPOLATOR);
        animator.setDuration(QUICK_DELETE_WIPE_ANIMATION_TIME_MS);
        return animator;
    }

    /**
     * @param parentViewHeight The height of the view that the animation should run on.
     * @return The {@link Animator} that can be used to start the fade animation.
     */
    public ObjectAnimator createFadeAnimator(int parentViewHeight) {
        ObjectAnimator animator = createAnimator(parentViewHeight, -parentViewHeight * 2);
        animator.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);
        animator.setDuration(QUICK_DELETE_FADE_ANIMATION_TIME_MS);
        return animator;
    }

    public static int getAnimationsIntersectionHeight(int parentViewHeight) {
        int gradientHeight =
                (int) (parentViewHeight * QUICK_DELETE_WIPE_GRADIENT_HEIGHT_MULTIPLIER);
        return (int) (gradientHeight * QUICK_DELETE_ANIMATION_INTERSECTION_MULTIPLIER);
    }

    /** Returns the current translationY value of the {@link LinearGradient} shader. */
    @UsedByReflection("QuickDeleteAnimationGradientDrawable.java")
    public float getTranslationY() {
        return mTranslationY;
    }

    /**
     * Sets the current translationY value of the {@link LinearGradient} shader. Generally this
     * method should only be called by the running animation (but must be accessible for {@link
     * android.animation.ObjectAnimator}).
     */
    @UsedByReflection("QuickDeleteAnimationGradientDrawable.java")
    @Keep
    public void setTranslationY(float value) {
        mTranslationY = value;
        invalidateSelf();
    }

    @Override
    public void draw(@NonNull Canvas canvas) {
        Matrix matrix = new Matrix();
        mShader.getLocalMatrix(matrix);
        matrix.setTranslate(0, mTranslationY);

        mShader.setLocalMatrix(matrix);
        canvas.drawPaint(mPaint);
    }

    @Override
    public void setAlpha(int alpha) {
        mPaint.setAlpha(alpha);
    }

    @Override
    public void setColorFilter(@Nullable ColorFilter colorFilter) {
        mPaint.setColorFilter(colorFilter);
    }

    @Override
    public int getOpacity() {
        return PixelFormat.TRANSLUCENT;
    }

    private ObjectAnimator createAnimator(float startValue, float endValue) {
        return ObjectAnimator.ofFloat(this, "translationY", startValue, endValue);
    }

    private float dpToPixels(@NonNull Context context, float dp) {
        Resources res = context.getResources();
        DisplayMetrics metrics = res.getDisplayMetrics();
        return TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, dp, metrics);
    }
}
