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
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Map;

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

    /** The glyph and color of a top-level row's icon. */
    private static final class RowIcon {
        final @DrawableRes int mGlyphRes;
        final @IconColor int mIconColor;

        RowIcon(@DrawableRes int glyphRes, @IconColor int iconColor) {
            mGlyphRes = glyphRes;
            mIconColor = iconColor;
        }
    }

    /** Icons for {@link MainSettings} rows, keyed by preference key. */
    private static final Map<String, RowIcon> ROW_ICONS =
            Map.ofEntries(
                    Map.entry(
                            MainSettings.PREF_SEARCH_ENGINE,
                            new RowIcon(R.drawable.settings_search_24dp, IconColor.PURPLE)),
                    Map.entry(
                            MainSettings.PREF_PRIVACY,
                            new RowIcon(R.drawable.settings_security_24dp, IconColor.PURPLE)),
                    Map.entry(
                            MainSettings.PREF_NOTIFICATIONS,
                            new RowIcon(R.drawable.settings_notifications_24dp, IconColor.PINK)),
                    Map.entry(
                            MainSettings.PREF_APPEARANCE,
                            new RowIcon(R.drawable.settings_palette_24dp, IconColor.ORANGE)),
                    Map.entry(
                            MainSettings.PREF_TABS,
                            new RowIcon(R.drawable.settings_tab_group_24dp, IconColor.ORANGE)),
                    Map.entry(
                            MainSettings.PREF_AUTOFILL_AND_PASSWORDS,
                            new RowIcon(R.drawable.settings_password_manager_24dp, IconColor.CYAN)),
                    Map.entry(
                            MainSettings.PREF_LANGUAGES,
                            new RowIcon(R.drawable.settings_translate_24dp, IconColor.NEUTRAL)),
                    Map.entry(
                            MainSettings.PREF_DOWNLOADS,
                            new RowIcon(R.drawable.settings_download_24dp, IconColor.NEUTRAL)),
                    Map.entry(
                            MainSettings.PREF_ACCESSIBILITY,
                            new RowIcon(R.drawable.settings_accessibility_24dp, IconColor.NEUTRAL)),
                    Map.entry(
                            MainSettings.PREF_ABOUT_CHROME,
                            new RowIcon(
                                    R.drawable.settings_chrome_product_24dp, IconColor.NEUTRAL)),
                    // TODO(crbug.com/581503964): Replace these placeholder glyphs and colors once
                    // the design covers these rows.
                    Map.entry(
                            MainSettings.PREF_SAFETY_HUB,
                            new RowIcon(R.drawable.ic_security_grey, IconColor.NEUTRAL)),
                    Map.entry(
                            MainSettings.PREF_CONTENT_SETTINGS,
                            new RowIcon(R.drawable.ic_globe_24dp, IconColor.NEUTRAL)),
                    Map.entry(
                            MainSettings.PREF_ADDRESS_BAR,
                            new RowIcon(R.drawable.ic_toolbar_24dp, IconColor.NEUTRAL)),
                    Map.entry(
                            MainSettings.PREF_HOMEPAGE,
                            new RowIcon(R.drawable.ic_home_24dp, IconColor.NEUTRAL)),
                    Map.entry(
                            MainSettings.PREF_DEFAULT_BROWSER,
                            new RowIcon(R.drawable.ic_open_in_browser, IconColor.NEUTRAL)),
                    Map.entry(
                            MainSettings.PREF_ON_STARTUP,
                            new RowIcon(R.drawable.ic_open_in_new_20dp, IconColor.NEUTRAL)),
                    Map.entry(
                            MainSettings.PREF_GLIC,
                            new RowIcon(R.drawable.ic_spark_outlined_24dp, IconColor.NEUTRAL)),
                    Map.entry(
                            MainSettings.PREF_DEVELOPER,
                            new RowIcon(
                                    R.drawable.ic_settings_applications_24dp, IconColor.NEUTRAL)));

    private SettingsIconFactory() {}

    /**
     * Creates the icon for a {@link MainSettings} row, using the row's default color.
     *
     * @param context The context used to resolve the glyph and colors.
     * @param key The row's preference key.
     * @return The row's icon, or null if the row has no icon mapping.
     */
    public static @Nullable Drawable createForKey(Context context, String key) {
        RowIcon rowIcon = ROW_ICONS.get(key);
        if (rowIcon == null) return null;
        return create(context, rowIcon.mGlyphRes, rowIcon.mIconColor);
    }

    /**
     * Creates the icon for a {@link MainSettings} row with a caller-chosen color, e.g. when the
     * row's state changes. Call {@link androidx.preference.Preference#setIcon} with the result to
     * update a row that is already shown.
     *
     * @param context The context used to resolve the glyph and colors.
     * @param key The row's preference key.
     * @param iconColor The {@link IconColor}, overriding the row's default.
     * @return The row's icon, or null if the row has no icon mapping.
     */
    public static @Nullable Drawable createForKey(
            Context context, String key, @IconColor int iconColor) {
        RowIcon rowIcon = ROW_ICONS.get(key);
        if (rowIcon == null) return null;
        return create(context, rowIcon.mGlyphRes, iconColor);
    }

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
