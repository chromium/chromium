// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;

import androidx.annotation.ColorInt;
import androidx.core.content.ContextCompat;
import androidx.core.graphics.drawable.DrawableCompat;
import androidx.recyclerview.widget.RecyclerView;

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
     * Creates a {@link NtpThemeColorInfo} instance for the given color Id.
     *
     * @param context The activity context.
     * @param colorId The required color Id.
     */
    public static @Nullable NtpThemeColorInfo createNtpThemeColorInfo(
            Context context, @NtpThemeColorId int colorId) {
        // TODO(https://crbug.com/423579377): Updates the colors here and adds the entire list of
        // colors.
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
                        R.color.ntp_color_blue_primary);
            default:
                return null;
        }
    }

    /** Gets the primary color for the theme color id if exists, null otherwise. */
    public static @Nullable @ColorInt Integer getNtpThemePrimaryColor(
            Context context, @NtpThemeColorId int colorId) {
        switch (colorId) {
            case NtpThemeColorId.LIGHT_BLUE:
                return context.getColor(R.color.ntp_color_light_blue_primary);
            case NtpThemeColorId.BLUE:
                return context.getColor(R.color.ntp_color_blue_primary);
            default:
                return null;
        }
    }

    /**
     * Initializes a list of NtpThemeColorInfo and add them to the provided list {@link
     * chromeColorsList}. Returns the index of the info whose primary color matches the given
     * primary color.
     *
     * @param context The Activity context.
     * @param chromeColorsList The list to update.
     * @param primaryColorInfo The primary color to find from the list.
     */
    public static int initColorsListAndFindPrimaryColorIndex(
            Context context,
            List<NtpThemeColorInfo> chromeColorsList,
            @Nullable NtpThemeColorInfo primaryColorInfo) {
        if (!chromeColorsList.isEmpty()) return RecyclerView.NO_POSITION;

        boolean hasPrimaryColor = primaryColorInfo != null;
        int primaryColorIndex = RecyclerView.NO_POSITION;

        for (int i = NtpThemeColorInfo.NtpThemeColorId.DEFAULT + 1;
                i < NtpThemeColorInfo.NtpThemeColorId.NUM_ENTRIES;
                i++) {
            var info = NtpThemeColorUtils.createNtpThemeColorInfo(context, i);
            if (info == null) continue;

            if (hasPrimaryColor && isPrimaryColorMatched(context, primaryColorInfo, info)) {
                primaryColorIndex = i - 1;
            }
            chromeColorsList.add(info);
        }

        // Handles the case of manually inputted primary and background colors. This color doesn't
        // have a prebuilt color id.
        if (primaryColorIndex == RecyclerView.NO_POSITION
                && hasPrimaryColor
                && primaryColorInfo instanceof NtpThemeColorFromHexInfo info
                && info.backgroundColor != NtpThemeColorInfo.COLOR_NOT_SET) {
            chromeColorsList.add(primaryColorInfo);
            return chromeColorsList.size() - 1;
        }

        return primaryColorIndex;
    }

    /**
     * Returns whether the given ntpThemeColorInfo's primary color matches the primary color of the
     * primaryColorInfo.
     *
     * @param context Used to get color.
     * @param primaryColorInfo The ColorInfo for the primary color.
     * @param ntpThemeColorInfo The ColorInfo to compare.
     */
    public static boolean isPrimaryColorMatched(
            Context context,
            @Nullable NtpThemeColorInfo primaryColorInfo,
            @Nullable NtpThemeColorInfo ntpThemeColorInfo) {
        if (primaryColorInfo == null || ntpThemeColorInfo == null) return false;

        if (primaryColorInfo instanceof NtpThemeColorFromHexInfo primaryColorFromHexInfo) {
            if (ntpThemeColorInfo instanceof NtpThemeColorFromHexInfo ntpThemeColorFromHexInfo) {
                return primaryColorFromHexInfo.primaryColor
                        == ntpThemeColorFromHexInfo.primaryColor;
            }

            return primaryColorFromHexInfo.primaryColor
                    == context.getColor(ntpThemeColorInfo.primaryColorResId);
        }

        if (ntpThemeColorInfo instanceof NtpThemeColorFromHexInfo ntpThemeColorFromHexInfo) {
            return context.getColor(primaryColorInfo.primaryColorResId)
                    == ntpThemeColorFromHexInfo.primaryColor;
        }

        return ntpThemeColorInfo.primaryColorResId == primaryColorInfo.primaryColorResId;
    }

    /**
     * Gets the background color from the given colorInfo. Returns the default background color if
     * colorInfo is null.
     *
     * @param context Used to get a color's int value based on the theme.
     * @param colorInfo The NtpThemeColorInfo instance.
     */
    public static @ColorInt int getBackgroundColorFromColorInfo(
            Context context, @Nullable NtpThemeColorInfo colorInfo) {
        if (colorInfo == null) return getDefaultBackgroundColor(context);

        if (colorInfo instanceof NtpThemeColorFromHexInfo) {
            return ((NtpThemeColorFromHexInfo) colorInfo).backgroundColor;
        }
        return context.getColor(colorInfo.backgroundColorResId);
    }

    /**
     * Returns the default background color for NTP. Needs to use the Activity's context rather than
     * the application's context, which isn't themed and will provide a wrong color.
     *
     * @param context The current Activity context. It is themed and can provide the correct color.
     */
    public static @ColorInt int getDefaultBackgroundColor(Context context) {
        return ContextCompat.getColor(context, R.color.home_surface_background_color);
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

    /**
     * Creates a list of predefined theme color info.
     *
     * @param context The Activity context.
     */
    public static List<NtpThemeColorInfo> createThemeColorListForTesting(Context context) {
        List<NtpThemeColorInfo> colorList = new ArrayList<>();
        colorList.add(
                createNtpThemeColorInfo(context, NtpThemeColorInfo.NtpThemeColorId.LIGHT_BLUE));
        colorList.add(createNtpThemeColorInfo(context, NtpThemeColorInfo.NtpThemeColorId.BLUE));
        return colorList;
    }
}
