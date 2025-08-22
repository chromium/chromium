// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ntp_customization.R;

import java.util.ArrayList;
import java.util.List;

/** Utility class for Chrome NTP's theme colors. */
@NullMarked
public class NtpThemeColorUtils {
    /**
     * Returens a list of predefined theme color info.
     *
     * @param context The Activity context.
     */
    public static List<NtpThemeColorInfo> getThemeColors(Context context) {
        List<NtpThemeColorInfo> colorList = new ArrayList<>();
        // TODO(https://crbug.com/423579377): Updates the colors here and adds the entire list of
        // colors.
        colorList.add(
                new NtpThemeColorInfo(
                        context,
                        NtpThemeColorInfo.NtpThemeColorId.LIGHT_BLUE,
                        R.color.ntp_color_light_blue_background,
                        R.color.ntp_color_light_blue_primary,
                        R.drawable.chrome_color_theme_icon_blue));
        colorList.add(
                new NtpThemeColorInfo(
                        context,
                        NtpThemeColorInfo.NtpThemeColorId.BLUE,
                        R.color.ntp_color_blue_background,
                        R.color.ntp_color_blue_background,
                        R.drawable.chrome_color_theme_icon_light_blue));
        return colorList;
    }
}
