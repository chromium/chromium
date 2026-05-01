// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import android.content.Context;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Objects;

@NullMarked
/** A subclass of NtpThemeColorInfo, manually created by the user from a hex string. */
public class NtpThemeColorFromHexInfo extends NtpThemeColorInfo {
    public final @ColorInt int backgroundColorLight;
    public final @ColorInt int backgroundColorDark;
    public final @ColorInt int primaryColorLight;
    public final @ColorInt int primaryColorDark;

    /**
     * Create a NtpThemeColorFromHexInfo.
     *
     * @param context The Activity context.
     * @param backgroundColor The background color.
     * @param primaryColor The primary color.
     */
    public NtpThemeColorFromHexInfo(
            Context context, @ColorInt int backgroundColor, @ColorInt int primaryColor) {
        this(context, backgroundColor, backgroundColor, primaryColor, primaryColor);
    }

    /**
     * Create a NtpThemeColorFromHexInfo.
     *
     * @param context The Activity context.
     * @param backgroundColorLight The background color in light mode.
     * @param backgroundColorDark The background color in dark mode.
     * @param primaryColorLight The primary color in light mode.
     * @param primaryColorDark The primary color in dark mode.
     */
    public NtpThemeColorFromHexInfo(
            Context context,
            @ColorInt int backgroundColorLight,
            @ColorInt int backgroundColorDark,
            @ColorInt int primaryColorLight,
            @ColorInt int primaryColorDark) {
        super(context, backgroundColorLight, primaryColorLight);
        this.backgroundColorLight = backgroundColorLight;
        this.backgroundColorDark = backgroundColorDark;
        this.primaryColorLight = primaryColorLight;
        this.primaryColorDark = primaryColorDark;
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (obj == null) return false;

        if (obj instanceof NtpThemeColorFromHexInfo other) {
            return primaryColorLight == other.primaryColorLight
                    && primaryColorDark == other.primaryColorDark
                    && backgroundColorLight == other.backgroundColorLight
                    && backgroundColorDark == other.backgroundColorDark;
        }
        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                super.hashCode(),
                primaryColorLight,
                primaryColorDark,
                backgroundColorLight,
                backgroundColorDark);
    }
}
