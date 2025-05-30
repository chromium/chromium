// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.hub;

import android.content.Context;
import android.graphics.Point;
import android.graphics.Rect;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;
import androidx.core.content.ContextCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.theme.ThemeModuleUtils;
import org.chromium.components.browser_ui.styles.ChromeColors;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Utilities related to new tab animations. */
@NullMarked
public class NewTabAnimationUtils {
    @IntDef({
        RectStart.TOP,
        RectStart.TOP_TOOLBAR,
        RectStart.BOTTOM,
        RectStart.BOTTOM_TOOLBAR,
        RectStart.CENTER
    })
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    public @interface RectStart {
        int TOP = 0;
        int TOP_TOOLBAR = 1;
        int BOTTOM = 2;
        int BOTTOM_TOOLBAR = 3;
        int CENTER = 4;
    }

    private static final float INITIAL_SCALE = 0.2f;
    private static final float FINAL_SCALE = 1.1f;

    /** Returns whether to use new tab animations. */
    public static boolean isNewTabAnimationEnabled() {
        return ThemeModuleUtils.isForceEnableDependencies()
                || ChromeFeatureList.sShowNewTabAnimations.isEnabled();
    }

    /**
     * Returns the tab color for the new tab foreground animation.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The {@link ColorInt} for the new tab animation background.
     */
    public static @ColorInt int getBackgroundColor(Context context, boolean isIncognito) {
        // See crbug.com/1507124 for Surface Background Color.
        return isIncognito
                ? ChromeColors.getPrimaryBackgroundColor(context, isIncognito)
                : ContextCompat.getColor(context, R.color.home_surface_background_color);
    }

    /**
     * Updates two {@link Rect} objects for the new tab foreground animation. The initial {@link
     * Rect} will be updated and scaled based on {@code finalRect} * {@link #INITIAL_SCALE} and the
     * final {@link Rect} will be scaled by {@link #FINAL_SCALE}. This method takes into account RTL
     * and LTR layout directions.
     *
     * @param rectStart Origin point where the animation starts.
     * @param isRtl Whether the Layout direction is RTL or LTR.
     * @param initialRect The initial {@link Rect}. Its values will be overwritten by {@code
     *     finalRect} * {@link #INITIAL_SCALE}.
     * @param finalRect The final {@link Rect} in which the {@code initialRect} will be based on.
     *     Its values will be multiplied by {@link #FINAL_SCALE}.
     */
    public static void updateRects(
            @RectStart int rectStart, boolean isRtl, Rect initialRect, Rect finalRect) {
        int initialWidth = Math.round(finalRect.width() * INITIAL_SCALE);
        int initialHeight = Math.round(finalRect.height() * INITIAL_SCALE);
        int finalWidth = Math.round(finalRect.width() * FINAL_SCALE);
        int finalHeight = Math.round(finalRect.height() * FINAL_SCALE);

        if (rectStart == RectStart.CENTER) {
            Point center = new Point(finalRect.centerX(), finalRect.centerY());
            updateCenterRect(center, initialWidth, initialHeight, initialRect);
            updateCenterRect(center, finalWidth, finalHeight, finalRect);
        } else {
            int x;
            if (isRtl) {
                x = finalRect.right;
                initialRect.left = x - initialWidth;
                initialRect.right = x;
                finalRect.left = x - finalWidth;
            } else {
                x = finalRect.left;
                initialRect.left = x;
                initialRect.right = x + initialWidth;
                finalRect.right = x + finalWidth;
            }

            int y;
            if (rectStart == RectStart.TOP || rectStart == RectStart.TOP_TOOLBAR) {
                y = finalRect.top;
                initialRect.top = y;
                initialRect.bottom = y + initialHeight;
                finalRect.bottom = y + finalHeight;
            } else {
                y = finalRect.bottom;
                initialRect.top = y - initialHeight;
                initialRect.bottom = y;
                finalRect.top = y - finalHeight;
            }
        }
    }

    private static void updateCenterRect(Point center, int width, int height, Rect rect) {
        rect.left = center.x - width / 2;
        rect.right = rect.left + width;
        rect.top = center.y - height / 2;
        rect.bottom = rect.top + height;
    }
}
