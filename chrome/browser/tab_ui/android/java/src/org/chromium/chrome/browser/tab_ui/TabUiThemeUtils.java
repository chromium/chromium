// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import android.content.Context;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.core.content.ContextCompat;

import com.google.android.material.color.MaterialColors;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.theme.SurfaceColorUpdateUtils;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

/**
 * Utility methods for providing colors and styles for the tab UI.
 * @deprecated Add new changes to TabUiThemeUtil.java, or TabUiThemeProvider.java.
 */
@NullMarked
@Deprecated
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

    /**
     * Returns the mini-thumbnail placeholder color for the multi-thumbnail tab grid card based on
     * the incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param isSelected Whether the tab is currently selected.
     * @return The mini-thumbnail placeholder color.
     */
    public static @ColorInt int getMiniThumbnailPlaceholderColor(
            Context context, boolean isIncognito, boolean isSelected) {
        if (isIncognito) {
            @ColorRes
            int colorRes =
                    isSelected
                            ? R.color.incognito_tab_thumbnail_placeholder_selected_color
                            : R.color.incognito_tab_thumbnail_placeholder_color;
            return context.getColor(colorRes);
        }

        if (isSelected) {
            int alpha =
                    context.getResources()
                            .getInteger(R.integer.tab_thumbnail_placeholder_selected_color_alpha);
            @ColorInt int baseColor = SemanticColorUtils.getColorOnPrimary(context);
            return MaterialColors.compositeARGBWithAlpha(baseColor, alpha);
        }

        return SemanticColorUtils.getColorSurfaceContainerLow(context);
    }

    /**
     * Returns the color to use for the tab grid card view background based on incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param isSelected Whether the tab is currently selected.
     * @return The {@link ColorInt} for tab grid card view background.
     */
    public static @ColorInt int getCardViewBackgroundColor(
            Context context, boolean isIncognito, boolean isSelected) {
        if (isSelected) {
            // Incognito does not use dynamic colors, so it can use colors from resources.
            return isIncognito
                    ? ContextCompat.getColor(context, R.color.incognito_tab_bg_selected_color)
                    : MaterialColors.getColor(context, R.attr.colorPrimary, TAG);
        } else {
            return SurfaceColorUpdateUtils.getCardViewBackgroundColor(context, isIncognito);
        }
    }
}
