// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.TimeAnimator;
import android.animation.ValueAnimator;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.drawable.Animatable;
import android.graphics.drawable.Drawable;

import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.interpolators.Interpolators;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Random;

/**
 * Illustration for the animated illustration arm of the Safety FRE Promo. Draws the Chrome logo in
 * the center with safety icons around it that blink between full and dim opacity.
 *
 * <p>A single {@link TimeAnimator} drives all icons on a fixed time step. Each step fades the icons
 * from their state at the start of the step to their state at the end of the step, then holds them
 * still. On each step, every icon is dim at the end of the step with some probability, as long as
 * not too many icons are dim, so an icon can stay dim for consecutive steps.
 */
@NullMarked
class SafetyPromoAnimatedIllustrationDrawable extends Drawable implements Animatable {
    private static final int GRID_CELLS_PER_SIDE = 4;

    private static final float BRIGHT_ICON_ALPHA = 1f;
    @VisibleForTesting static final float DIM_ICON_ALPHA = 0.2f;
    @VisibleForTesting static final long FADE_DURATION_MS = 800;
    private static final long HOLD_DURATION_MS = 800;
    @VisibleForTesting static final long STEP_DURATION_MS = FADE_DURATION_MS + HOLD_DURATION_MS;
    private static final float DIM_PROBABILITY = 0.3f;
    @VisibleForTesting static final int MAX_DIM_ICONS = 3;
    // Fixed seed for determinism, so the illustration blinks the same way on every run.
    private static final long SEED = 42;

    private static @Nullable Boolean sAreAnimatorsEnabledForTesting;

    private static final class Icon {
        public final Drawable drawable;
        public final int column;
        public final int row;

        /** Whether the icon is dim at the start of the current step. */
        public boolean wasDim;

        /** Whether the icon is dim at the end of the current step. */
        public boolean isDim;

        Icon(Context context, @DrawableRes int drawableId, int column, int row) {
            this.drawable = loadDrawable(context, drawableId);
            this.column = column;
            this.row = row;
        }
    }

    private final Drawable mLogo;
    private final List<Icon> mIcons;
    private final Random mRandom = new Random(SEED);
    private final int mWidthPx;
    private final int mHeightPx;
    private final int mIconSizePx;
    private final int mLogoSizePx;
    private final float mColumnSpacingPx;
    private final float mRowSpacingPx;

    private long mLastStep;
    private @Nullable TimeAnimator mAnimator;

    static SafetyPromoAnimatedIllustrationDrawable create(Context context) {
        @DrawableRes int password = R.drawable.safety_promo_password_manager_illustration;
        @DrawableRes int safeBrowsing = R.drawable.safety_promo_enhanced_safe_browsing_illustration;
        @DrawableRes int history = R.drawable.safety_promo_history_quick_delete_illustration;
        @DrawableRes int incognito = R.drawable.safety_promo_incognito_illustration;
        @Nullable Integer[][] grid = {
            {safeBrowsing, incognito, history, password},
            {password, null, null, history},
            {incognito, null, null, safeBrowsing},
            {password, safeBrowsing, incognito, history},
        };
        List<Icon> icons = new ArrayList<>();
        for (int row = 0; row < GRID_CELLS_PER_SIDE; row++) {
            for (int column = 0; column < GRID_CELLS_PER_SIDE; column++) {
                Integer drawableId = grid[row][column];
                if (drawableId == null) continue;
                icons.add(new Icon(context, drawableId, column, row));
            }
        }
        Resources res = context.getResources();
        return new SafetyPromoAnimatedIllustrationDrawable(
                loadDrawable(context, R.drawable.fre_product_logo),
                icons,
                res.getDimensionPixelSize(R.dimen.safety_fre_animated_illustration_width),
                res.getDimensionPixelSize(R.dimen.safety_fre_animated_illustration_height),
                res.getDimensionPixelSize(R.dimen.safety_fre_animated_illustration_icon_size),
                res.getDimensionPixelSize(R.dimen.safety_fre_animated_illustration_logo_size));
    }

    private static Drawable loadDrawable(Context context, @DrawableRes int resId) {
        return assumeNonNull(context.getDrawable(resId)).mutate();
    }

    private SafetyPromoAnimatedIllustrationDrawable(
            Drawable logo,
            List<Icon> icons,
            int widthPx,
            int heightPx,
            int iconSizePx,
            int logoSizePx) {
        mLogo = logo;
        mIcons = icons;
        mWidthPx = widthPx;
        mHeightPx = heightPx;
        mIconSizePx = iconSizePx;
        mLogoSizePx = logoSizePx;
        mColumnSpacingPx = (float) (widthPx - iconSizePx) / (GRID_CELLS_PER_SIDE - 1);
        mRowSpacingPx = (float) (heightPx - iconSizePx) / (GRID_CELLS_PER_SIDE - 1);
    }

    @Override
    public void start() {
        if (isRunning()) return;

        resetIcons();
        if (!areAnimatorsEnabled()) return;

        mAnimator = new TimeAnimator();
        mAnimator.setTimeListener((_, totalTime, _) -> onTimeUpdate(totalTime));
        mAnimator.start();
    }

    @Override
    public void stop() {
        if (mAnimator != null) {
            mAnimator.setTimeListener(null);
            mAnimator.cancel();
            mAnimator = null;
        }
        resetIcons();
    }

    @Override
    public boolean isRunning() {
        return mAnimator != null;
    }

    private static boolean areAnimatorsEnabled() {
        if (sAreAnimatorsEnabledForTesting != null) return sAreAnimatorsEnabledForTesting;

        return ValueAnimator.areAnimatorsEnabled();
    }

    private void resetIcons() {
        mRandom.setSeed(SEED);
        mLastStep = 0;
        for (Icon icon : mIcons) {
            icon.wasDim = false;
            icon.isDim = false;
        }
        updateIconAlphas(/* progress= */ 0f);
    }

    @VisibleForTesting
    void onTimeUpdate(long totalTimeMs) {
        long step = totalTimeMs / STEP_DURATION_MS;
        while (mLastStep < step) {
            mLastStep++;
            advanceStep();
        }
        long timeInStepMs = totalTimeMs % STEP_DURATION_MS;
        float fadeFraction = Math.min(1f, (float) timeInStepMs / FADE_DURATION_MS);
        updateIconAlphas(
                Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR.getInterpolation(fadeFraction));
    }

    private void advanceStep() {
        for (Icon icon : mIcons) {
            icon.wasDim = icon.isDim;
            icon.isDim = false;
        }
        List<Icon> dimCandidates = new ArrayList<>(mIcons);
        int slots = MAX_DIM_ICONS;
        // Shuffle so that no icon is favored when the number of dim icons is capped.
        Collections.shuffle(dimCandidates, mRandom);
        // Try bright icons first, so an icon only stays dim when there are slots left.
        dimCandidates.sort((a, b) -> Boolean.compare(a.wasDim, b.wasDim));
        for (Icon icon : dimCandidates) {
            if (slots == 0) break;

            if (mRandom.nextFloat() < DIM_PROBABILITY) {
                icon.isDim = true;
                slots--;
            }
        }
    }

    private void updateIconAlphas(float progress) {
        boolean changed = false;
        for (Icon icon : mIcons) {
            float from = icon.wasDim ? DIM_ICON_ALPHA : BRIGHT_ICON_ALPHA;
            float to = icon.isDim ? DIM_ICON_ALPHA : BRIGHT_ICON_ALPHA;
            int alpha = Math.round((from + (to - from) * progress) * 255);
            if (icon.drawable.getAlpha() == alpha) continue;
            icon.drawable.setAlpha(alpha);
            changed = true;
        }
        if (changed) invalidateSelf();
    }

    @Override
    public boolean setVisible(boolean visible, boolean restart) {
        boolean changed = super.setVisible(visible, restart);
        if (!visible) {
            stop();
        } else if (changed || restart) {
            stop();
            start();
        }
        return changed;
    }

    @Override
    protected void onBoundsChange(Rect bounds) {
        super.onBoundsChange(bounds);
        float left = bounds.exactCenterX() - mWidthPx / 2f;
        float top = bounds.exactCenterY() - mHeightPx / 2f;
        setCenteredBounds(mLogo, bounds.exactCenterX(), bounds.exactCenterY(), mLogoSizePx);
        for (Icon icon : mIcons) {
            float centerX = left + mIconSizePx / 2f + icon.column * mColumnSpacingPx;
            float centerY = top + mIconSizePx / 2f + icon.row * mRowSpacingPx;
            setCenteredBounds(icon.drawable, centerX, centerY, mIconSizePx);
        }
    }

    @Override
    public void draw(Canvas canvas) {
        mLogo.draw(canvas);
        for (Icon icon : mIcons) {
            icon.drawable.draw(canvas);
        }
    }

    @Override
    public int getIntrinsicWidth() {
        return mWidthPx;
    }

    @Override
    public int getIntrinsicHeight() {
        return mHeightPx;
    }

    @Override
    public void setAlpha(int alpha) {}

    @Override
    public void setColorFilter(@Nullable ColorFilter colorFilter) {}

    @Override
    public int getOpacity() {
        return PixelFormat.TRANSLUCENT;
    }

    private static void setCenteredBounds(
            Drawable drawable, float centerXPx, float centerYPx, float sizePx) {
        float halfPx = sizePx / 2f;
        drawable.setBounds(
                Math.round(centerXPx - halfPx),
                Math.round(centerYPx - halfPx),
                Math.round(centerXPx + halfPx),
                Math.round(centerYPx + halfPx));
    }

    static void setAreAnimatorsEnabledForTesting(boolean enabled) {
        sAreAnimatorsEnabledForTesting = enabled;
        ResettersForTesting.register(() -> sAreAnimatorsEnabledForTesting = null);
    }

    Drawable getLogoForTesting() {
        return mLogo;
    }

    List<Drawable> getIconsForTesting() {
        List<Drawable> icons = new ArrayList<>(mIcons.size());
        for (Icon icon : mIcons) {
            icons.add(icon.drawable);
        }
        return icons;
    }
}
