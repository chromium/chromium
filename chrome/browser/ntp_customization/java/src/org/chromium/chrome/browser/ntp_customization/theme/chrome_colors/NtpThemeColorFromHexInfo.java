// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import android.content.Context;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;

@NullMarked
/** A subclass of NtpThemeColorInfo, manually created by the user from a hex string. */
public class NtpThemeColorFromHexInfo extends NtpThemeColorInfo {
    public final @ColorInt int backgroundColor;
    public final @ColorInt int primaryColor;

    /**
     * Create a NtpThemeColorFromHexInfo.
     *
     * @param context The Activity context.
     * @param backgroundColor The background color.
     * @param primaryColor The primary color.
     */
    public NtpThemeColorFromHexInfo(
            Context context, @ColorInt int backgroundColor, @ColorInt int primaryColor) {
        super(context, backgroundColor, primaryColor);
        this.backgroundColor = backgroundColor;
        this.primaryColor = primaryColor;
    }
}
