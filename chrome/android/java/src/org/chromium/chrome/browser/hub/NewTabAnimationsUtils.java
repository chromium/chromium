// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.hub;

import android.content.Context;

import androidx.annotation.ColorInt;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.styles.ChromeColors;

/** Utilities related to new tab animations. */
public class NewTabAnimationsUtils {

    /**
     * Returns the color for new tab animation background.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The {@link ColorInt} for the new tab animation background.
     */
    public static @ColorInt int getBackgroundColor(Context context, boolean isIncognito) {
        // See crbug.com/1507124 for Surface Background Color.
        return isIncognito
                ? ChromeColors.getPrimaryBackgroundColor(context, isIncognito)
                : ChromeColors.getSurfaceColor(
                        context, R.dimen.home_surface_background_color_elevation);
    }
}
