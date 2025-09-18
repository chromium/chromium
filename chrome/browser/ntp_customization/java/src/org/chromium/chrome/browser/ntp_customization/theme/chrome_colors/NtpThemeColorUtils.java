// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;

import androidx.core.graphics.drawable.DrawableCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;

import java.util.ArrayList;
import java.util.List;

/** Utility class for Chrome NTP's theme colors. */
@NullMarked
public class NtpThemeColorUtils {
    /**
     * Creates a list of predefined theme color info.
     *
     * @param context The Activity context.
     */
    public static List<NtpThemeColorInfo> createThemeColorList(Context context) {
        List<NtpThemeColorInfo> colorList = new ArrayList<>();
        // TODO(https://crbug.com/423579377): Updates the colors here and adds the entire list of
        // colors.
        colorList.add(
                createNtpThemeColorInfo(context, NtpThemeColorInfo.NtpThemeColorId.LIGHT_BLUE));
        colorList.add(createNtpThemeColorInfo(context, NtpThemeColorInfo.NtpThemeColorId.BLUE));
        return colorList;
    }

    /**
     * Creates a {@link NtpThemeColorInfo} instance for the given color Id.
     *
     * @param context The activity context.
     * @param colorId The required color Id.
     */
    public static @Nullable NtpThemeColorInfo createNtpThemeColorInfo(
            Context context, @NtpThemeColorId int colorId) {
        switch (colorId) {
            case NtpThemeColorId.LIGHT_BLUE:
                return new NtpThemeColorInfo(
                        context,
                        NtpThemeColorInfo.NtpThemeColorId.LIGHT_BLUE,
                        R.color.ntp_color_light_blue_background,
                        R.color.ntp_color_light_blue_primary);
            case NtpThemeColorId.BLUE:
                return new NtpThemeColorInfo(
                        context,
                        NtpThemeColorInfo.NtpThemeColorId.BLUE,
                        R.color.ntp_color_blue_background,
                        R.color.ntp_color_blue_background);
            default:
                return null;
        }
    }

    /** Creates a colored circle drawable based on provides three colors. */
    static LayerDrawable createColoredCircle(
            Context context, int topColor, int bottomLeftColor, int bottomRightColor) {
        // 1. Loads each drawable layer.
        Drawable iconTopHalf =
                assumeNonNull(context.getDrawable(R.drawable.chrome_color_icon_top_half));
        Drawable iconBottomLeft =
                assumeNonNull(context.getDrawable(R.drawable.chrome_color_icon_bottom_left));
        Drawable iconBottomRight =
                assumeNonNull(context.getDrawable(R.drawable.chrome_color_icon_bottom_right));

        // 2. Mutates each drawable to have its own state.
        // Without this, tinting one instance would tint all instances of the drawable.
        Drawable tintedIconTopHalf = DrawableCompat.wrap(iconTopHalf).mutate();
        Drawable tintedIconBottomLeft = DrawableCompat.wrap(iconBottomLeft).mutate();
        Drawable tintedIconBottomRight = DrawableCompat.wrap(iconBottomRight).mutate();

        // 3. Applies the specific colors (tints).
        DrawableCompat.setTint(tintedIconTopHalf, topColor);
        DrawableCompat.setTint(tintedIconBottomLeft, bottomLeftColor);
        DrawableCompat.setTint(tintedIconBottomRight, bottomRightColor);

        // 4. Combines them into a LayerDrawable.
        Drawable[] layers =
                new Drawable[] {tintedIconTopHalf, tintedIconBottomLeft, tintedIconBottomRight};
        return new LayerDrawable(layers);
    }
}
