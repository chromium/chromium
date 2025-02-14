// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.hub;

import android.content.Context;
import android.graphics.Rect;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.styles.ChromeColors;

/** Utilities related to new tab animations. */
@NullMarked
public class NewTabAnimationUtils {
    private static final float INITIAL_SCALE = 0.2f;
    private static final float FINAL_SCALE = 1.1f;

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
                : ChromeColors.getSurfaceColor(
                        context, R.dimen.home_surface_background_color_elevation);
    }

    /**
     * Updates two {@link Rect} objects for the new tab foreground animation. The initial {@link
     * Rect} will be updated and scaled based on {@code finalRect} * {@link #INITIAL_SCALE} and the
     * final {@link Rect} will be scaled by {@link #FINAL_SCALE}. This method takes into account RTL
     * and LTR layout directions.
     *
     * @param initialRect The initial {@link Rect}. Its values will be overwritten by {@code
     *     finalRect} * {@link #INITIAL_SCALE}.
     * @param finalRect The final {@link Rect} in which the {@code initialRect} will be based on.
     *     Its values will be multiplied by {@link #FINAL_SCALE}.
     * @param isRtl Whether the Layout direction is RTL or LTR.
     * @param isTopAligned Whether the {@code initialRect} starts from the top or from the bottom of
     *     {@code finalRect}.
     */
    public static void updateRects(
            Rect initialRect, Rect finalRect, boolean isRtl, boolean isTopAligned) {
        int initialWidth = Math.round(finalRect.width() * INITIAL_SCALE);
        int initialHeight = Math.round(finalRect.height() * INITIAL_SCALE);
        int finalWidth = Math.round(finalRect.width() * FINAL_SCALE);
        int finalHeight = Math.round(finalRect.height() * FINAL_SCALE);

        int x;
        if (isRtl) {
            x = finalRect.right;
            initialRect.left = x - initialWidth;
            initialRect.right = x;
            finalRect.left = finalRect.right - finalWidth;
        } else {
            x = finalRect.left;
            initialRect.left = x;
            initialRect.right = x + initialWidth;
            finalRect.right = x + finalWidth;
        }

        int y;
        if (isTopAligned) {
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
