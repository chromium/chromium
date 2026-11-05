// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;
import android.view.Gravity;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.appcompat.content.res.AppCompatResources;

import com.google.android.material.color.MaterialColors;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Builds the colored, circular row icons used on the top-level settings page. Each icon is a filled
 * circle with a glyph tinted to contrast with it.
 */
@NullMarked
public final class SettingsIconFactory {
    private static final String TAG = "SettingsIconFactory";

    /**
     * The hue of an icon. Each hue has a circle (container) color and a glyph (on container) color,
     * with light and dark variants, harmonized with the theme's primary color. {@link #NEUTRAL}
     * follows the theme's surface colors.
     */
    @IntDef({
        IconColor.BLUE,
        IconColor.PURPLE,
        IconColor.PINK,
        IconColor.ORANGE,
        IconColor.CYAN,
        IconColor.NEUTRAL
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface IconColor {
        int BLUE = 0;
        int PURPLE = 1;
        int PINK = 2;
        int ORANGE = 3;
        int CYAN = 4;
        int NEUTRAL = 5;
    }

    private SettingsIconFactory() {}

    /**
     * Creates a row icon.
     *
     * @param context The context used to resolve the glyph and colors.
     * @param glyphRes The glyph drawable drawn on top of the circle.
     * @param iconColor The {@link IconColor} of the icon.
     * @return A new {@link Drawable}, sized to {@code R.dimen.settings_row_icon_size}.
     */
    public static Drawable create(
            Context context, @DrawableRes int glyphRes, @IconColor int iconColor) {
        Resources res = context.getResources();
        int circleSize = res.getDimensionPixelSize(R.dimen.settings_row_icon_size);
        int glyphSize = res.getDimensionPixelSize(R.dimen.settings_row_icon_glyph_size);

        GradientDrawable circle = new GradientDrawable();
        circle.setShape(GradientDrawable.OVAL);
        circle.setColor(getContainerColor(context, iconColor));

        Drawable glyph = AppCompatResources.getDrawable(context, glyphRes);
        assert glyph != null : "Missing glyph drawable";
        glyph = glyph.mutate();
        glyph.setTint(getOnContainerColor(context, iconColor));

        LayerDrawable icon = new LayerDrawable(new Drawable[] {circle, glyph});
        icon.setLayerSize(0, circleSize, circleSize);
        icon.setLayerSize(1, glyphSize, glyphSize);
        icon.setLayerGravity(1, Gravity.CENTER);
        return icon;
    }

    /** Returns the circle color of {@code iconColor}. */
    static @ColorInt int getContainerColor(Context context, @IconColor int iconColor) {
        switch (iconColor) {
            case IconColor.BLUE:
                return harmonize(context, R.color.settings_icon_blue_container);
            case IconColor.PURPLE:
                return harmonize(context, R.color.settings_icon_purple_container);
            case IconColor.PINK:
                return harmonize(context, R.color.settings_icon_pink_container);
            case IconColor.ORANGE:
                return harmonize(context, R.color.settings_icon_orange_container);
            case IconColor.CYAN:
                return harmonize(context, R.color.settings_icon_cyan_container);
            case IconColor.NEUTRAL:
            default:
                return MaterialColors.getColor(context, R.attr.colorSurfaceContainerHighest, TAG);
        }
    }

    /** Returns the glyph color of {@code iconColor}. */
    static @ColorInt int getOnContainerColor(Context context, @IconColor int iconColor) {
        switch (iconColor) {
            case IconColor.BLUE:
                return harmonize(context, R.color.settings_icon_on_blue_container);
            case IconColor.PURPLE:
                return harmonize(context, R.color.settings_icon_on_purple_container);
            case IconColor.PINK:
                return harmonize(context, R.color.settings_icon_on_pink_container);
            case IconColor.ORANGE:
                return harmonize(context, R.color.settings_icon_on_orange_container);
            case IconColor.CYAN:
                return harmonize(context, R.color.settings_icon_on_cyan_container);
            case IconColor.NEUTRAL:
            default:
                return MaterialColors.getColor(context, R.attr.colorOnSurfaceVariant, TAG);
        }
    }

    /**
     * Returns {@code colorRes} shifted slightly toward the theme's primary color, so the fixed hues
     * follow dynamic color.
     */
    private static @ColorInt int harmonize(Context context, @ColorRes int colorRes) {
        return MaterialColors.harmonizeWithPrimary(context, context.getColor(colorRes));
    }
}
