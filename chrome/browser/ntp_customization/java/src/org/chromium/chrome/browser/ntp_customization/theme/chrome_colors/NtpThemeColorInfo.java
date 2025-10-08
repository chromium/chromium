// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.util.ColorUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Objects;

/** The data class for NTP's theme color. */
@NullMarked
public class NtpThemeColorInfo {
    @IntDef({
        NtpThemeColorId.DEFAULT,
        NtpThemeColorId.BLUE,
        NtpThemeColorId.LIGHT_BLUE,
        NtpThemeColorId.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface NtpThemeColorId {
        int DEFAULT = 0;
        int BLUE = 1;
        int LIGHT_BLUE = 2;
        int NUM_ENTRIES = 3;
    }

    public static final int COLOR_NOT_SET = -1;

    public @NtpThemeColorId int id;
    // Used as the NTP's background color
    public @ColorRes int backgroundColorResId;
    // Used as Google logo color.
    public @ColorRes int primaryColorResId;
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
        this.backgroundColorResId = backgroundColorResId;
        this.primaryColorResId = primaryColorResId;
        @ColorInt int primaryColor = context.getColor(primaryColorResId);
        highlightColor = ColorUtils.setAlphaComponentWithFloat(primaryColor, HIGHLIGHT_COLOR_ALPHA);
        iconDrawable =
                NtpThemeColorUtils.createColoredCircle(
                        context,
                        context.getColor(backgroundColorResId),
                        primaryColor,
                        highlightColor);
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
        this.highlightColor =
                ColorUtils.setAlphaComponentWithFloat(primaryColor, HIGHLIGHT_COLOR_ALPHA);
        iconDrawable =
                NtpThemeColorUtils.createColoredCircle(
                        context, backgroundColor, primaryColor, highlightColor);
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (obj == null) return false;

        if (obj instanceof NtpThemeColorInfo info) {
            return id == info.id
                    && primaryColorResId == info.primaryColorResId
                    && backgroundColorResId == info.backgroundColorResId;
        }

        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(this.id, this.primaryColorResId, this.backgroundColorResId);
    }
}
