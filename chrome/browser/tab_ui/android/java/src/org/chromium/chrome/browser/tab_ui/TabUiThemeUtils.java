// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import android.content.Context;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;

import com.google.android.material.color.MaterialColors;

public class TabUiThemeUtils {
    private static final String TAG = "TabUiThemeUtils";

    /**
     * Returns the tint color for Chrome owned favicon based on the incognito mode or selected.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param isTabSelected Whether the tab is currently selected.
     * @return The tint color for Chrome owned favicon.
     */
    public static @ColorInt int getChromeOwnedFaviconTintColor(
            Context context, boolean isIncognito, boolean isTabSelected) {
        return getTitleTextColor(context, isIncognito, isTabSelected);
    }

    /**
     * Returns the title text appearance for the tab grid card based on the incognito mode.
     *
     * @param isIncognito Whether the text appearance is used for incognito mode.
     * @param isSelected Whether the tab is currently selected.
     * @return The text appearance for the tab grid card title.
     */
    public static @ColorInt int getTitleTextColor(
            Context context, boolean isIncognito, boolean isSelected) {
        if (isIncognito) {
            @ColorRes
            int colorRes =
                    isSelected
                            ? R.color.incognito_tab_title_selected_color
                            : R.color.incognito_tab_title_color;
            return context.getColor(colorRes);
        } else {
            return isSelected
                    ? MaterialColors.getColor(context, R.attr.colorOnPrimary, TAG)
                    : MaterialColors.getColor(context, R.attr.colorOnSurface, TAG);
        }
    }
}
