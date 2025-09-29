// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.IntDef;
import androidx.core.content.ContextCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.util.ColorUtils;

/** The data class for NTP's theme color. */
@NullMarked
public class NtpThemeColorInfo {
    @IntDef({
        NtpThemeColorId.DEFAULT,
        NtpThemeColorId.BLUE,
        NtpThemeColorId.LIGHT_BLUE,
        NtpThemeColorId.NUM_ENTRIES
    })
    public @interface NtpThemeColorId {
        int DEFAULT = 0;
        int BLUE = 1;
        int LIGHT_BLUE = 2;
        int NUM_ENTRIES = 3;
    }

    public @NtpThemeColorId int id;
    // Used as the NTP's background color
    public @ColorInt int backgroundColor;
    // Used as Google logo color.
    public @ColorInt int primaryColor;
    // Used as the omnibox color.
    public @ColorInt int highlightColor;

    public Drawable iconDrawable;

    private static final float HIGHLIGHT_COLOR_ALPHA = 0.15f;

    /**
     * Constructor for predefined colors.
     *
     * @param context The Activity context.
     * @param id The ID of the theme color.
     * @param backgroundColorResId The resource ID of the background color.
     * @param primaryColorResId The resource ID of the primary color.
     */
    public NtpThemeColorInfo(
            Context context,
            @NtpThemeColorId int id,
            @ColorRes int backgroundColorResId,
            @ColorRes int primaryColorResId) {
        this.id = id;
        backgroundColor = ContextCompat.getColor(context, backgroundColorResId);
        primaryColor = ContextCompat.getColor(context, primaryColorResId);
        highlightColor =
                ColorUtils.setAlphaComponentWithFloat(this.primaryColor, HIGHLIGHT_COLOR_ALPHA);
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
        this.backgroundColor = backgroundColor;
        this.primaryColor = primaryColor;
        this.highlightColor =
                ColorUtils.setAlphaComponentWithFloat(primaryColor, HIGHLIGHT_COLOR_ALPHA);
        iconDrawable =
                NtpThemeColorUtils.createColoredCircle(
                        context, backgroundColor, primaryColor, highlightColor);
    }
}
