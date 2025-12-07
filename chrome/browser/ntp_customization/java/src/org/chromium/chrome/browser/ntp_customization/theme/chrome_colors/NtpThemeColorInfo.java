// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.util.ColorUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Objects;

/** The data class for NTP's theme color. */
@NullMarked
public class NtpThemeColorInfo {
    // LINT.IfChange(NtpThemeColorId)
    @IntDef({
        NtpThemeColorId.DEFAULT,
        NtpThemeColorId.NTP_COLORS_BLUE,
        NtpThemeColorId.NTP_COLORS_AQUA,
        NtpThemeColorId.NTP_COLORS_GREEN,
        NtpThemeColorId.NTP_COLORS_VIRIDIAN,
        NtpThemeColorId.NTP_COLORS_CITRON,
        NtpThemeColorId.NTP_COLORS_ORANGE,
        NtpThemeColorId.NTP_COLORS_ROSE,
        NtpThemeColorId.NTP_COLORS_FUCHSIA,
        NtpThemeColorId.NTP_COLORS_VIOLET,
        NtpThemeColorId.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface NtpThemeColorId {
        int DEFAULT = 0;
        int NTP_COLORS_BLUE = 1;
        int NTP_COLORS_AQUA = 2;
        int NTP_COLORS_GREEN = 3;
        int NTP_COLORS_VIRIDIAN = 4;
        int NTP_COLORS_CITRON = 5;
        int NTP_COLORS_ORANGE = 6;
        int NTP_COLORS_ROSE = 7;
        int NTP_COLORS_FUCHSIA = 8;
        int NTP_COLORS_VIOLET = 9;
        int NUM_ENTRIES = 10;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/new_tab_page/enums.xml:NtpThemeColorId)

    public static final int COLOR_NOT_SET = -1;

    public @NtpThemeColorId int id;
    // Used as Google logo color.
    public @ColorRes int primaryColorResId;

    // Colors used to draw the color palette in the Chrome color bottom sheet.
    public @ColorInt int highlightColor;

    public Drawable iconDrawable;

    private static final float HIGHLIGHT_COLOR_ALPHA = 0.3f;
    private static final float BACKGROUND_COLOR_ALPHA = 0.15f;

    /**
     * Constructor for predefined colors.
     *
     * @param context The Activity context.
     * @param id The ID of the theme color.
     */
    public NtpThemeColorInfo(Context context, @NtpThemeColorId int id) {
        this.id = id;
        this.primaryColorResId = NtpThemeColorUtils.getNtpThemePrimaryColorResId(id);

        @ColorInt int primaryColor = context.getColor(primaryColorResId);
        highlightColor = calculateHighlightColorForColorPalette(primaryColor);
        @ColorInt int backgroundColor = calculateBackgroundColorForColorPalette(primaryColor);

        iconDrawable =
                NtpThemeColorUtils.createColoredCircle(
                        context, backgroundColor, primaryColor, highlightColor);
    }

    /**
     * Constructor for custom colors.
     *
     * @param context The Activity context.
     * @param backgroundColor The background color.
     * @param primaryColor The primary color.
     */
    public NtpThemeColorInfo(
            Context context, @ColorInt int backgroundColor, @ColorInt int primaryColor) {
        highlightColor = calculateHighlightColorForColorPalette(primaryColor);

        iconDrawable =
                NtpThemeColorUtils.createColoredCircle(
                        context, backgroundColor, primaryColor, highlightColor);
    }

    /**
     * Calculates the highlight color from the given primary color for constructing the color
     * palette.
     *
     * @param primaryColor The base color.
     */
    @VisibleForTesting
    public static @ColorInt int calculateHighlightColorForColorPalette(@ColorInt int primaryColor) {
        return ColorUtils.setAlphaComponentWithFloat(primaryColor, HIGHLIGHT_COLOR_ALPHA);
    }

    /**
     * Calculates the background color from the given primary color for constructing the color
     * palette.
     *
     * @param primaryColor The base color.
     */
    @VisibleForTesting
    public static @ColorInt int calculateBackgroundColorForColorPalette(
            @ColorInt int primaryColor) {
        return ColorUtils.setAlphaComponentWithFloat(primaryColor, BACKGROUND_COLOR_ALPHA);
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (obj == null) return false;

        if (obj instanceof NtpThemeColorInfo info) {
            return id == info.id && primaryColorResId == info.primaryColorResId;
        }

        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(this.id, this.primaryColorResId);
    }
}
