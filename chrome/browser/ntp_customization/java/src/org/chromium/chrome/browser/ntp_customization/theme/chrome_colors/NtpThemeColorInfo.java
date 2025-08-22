// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import android.content.Context;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
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

    public @DrawableRes int iconResId;

    private static final float HIGHLIGHT_COLOR_ALPHA = 0.15f;

    public NtpThemeColorInfo(
            Context context,
            @NtpThemeColorId int id,
            @ColorRes int backgroundColorResId,
            @ColorRes int primaryColorResId,
            @DrawableRes int iconResId) {
        this.id = id;
        this.backgroundColor = ContextCompat.getColor(context, backgroundColorResId);
        this.primaryColor = ContextCompat.getColor(context, primaryColorResId);
        this.highlightColor =
                ColorUtils.setAlphaComponentWithFloat(this.primaryColor, HIGHLIGHT_COLOR_ALPHA);
        // TODO(https://crbug.com/440583138): User LayerDrawable and update @fillcolor instead of
        // creating one drawable per svg file for the icon.
        this.iconResId = iconResId;
    }
}
