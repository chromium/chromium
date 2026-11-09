// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;

import androidx.annotation.DrawableRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.List;

/**
 * Illustration for the animated illustration arm of the Safety FRE Promo. Draws the Chrome logo in
 * the center with safety icons around it.
 */
@NullMarked
class SafetyPromoAnimatedIllustrationDrawable extends Drawable {
    // TODO(crbug.com/483539670): Blink the icons between full and dim opacity.

    private static final int GRID_CELLS_PER_SIDE = 4;

    private static final class Icon {
        public final Drawable drawable;
        public final int column;
        public final int row;

        Icon(Context context, @DrawableRes int drawableId, int column, int row) {
            this.drawable = loadDrawable(context, drawableId);
            this.column = column;
            this.row = row;
        }
    }

    private final Drawable mLogo;
    private final List<Icon> mIcons;
    private final int mWidthPx;
    private final int mHeightPx;
    private final int mIconSizePx;
    private final int mLogoSizePx;
    private final float mColumnSpacingPx;
    private final float mRowSpacingPx;

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
        return assumeNonNull(context.getDrawable(resId));
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
