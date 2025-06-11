// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import android.content.Context;
import android.content.res.ColorStateList;

import androidx.annotation.ColorInt;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.ContextCompat;

import com.google.android.material.color.MaterialColors;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.theme.SurfaceColorUpdateUtils;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.tab_groups.TabGroupColorId;

/**
 * Utility methods for providing colors and styles specifically for tab cards in the grid tab
 * switcher. For any new, general-purpose tab UI theming utilities, please add them to
 * TabUiThemeUtil.java or TabUiThemeProvider.java instead.
 */
@NullMarked
public class TabCardThemeUtil {
    private static final String TAG = "TabCardThemeUtil";

    /**
     * Returns the tint color for Chrome owned favicon based on the incognito mode or selected.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param isSelected Whether the tab is currently selected.
     * @return The tint color for Chrome owned favicon.
     */
    public static @ColorInt int getChromeOwnedFaviconTintColor(
            Context context, boolean isIncognito, boolean isSelected) {
        return getTitleTextColor(context, isIncognito, isSelected, null);
    }

    /**
     * Returns the title text appearance for the tab grid card based on the incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the text appearance is used for incognito mode.
     * @param isSelected Whether the tab is currently selected.
     * @param colorId Color chosen by user for the TabGroup, null if not a tab group.
     * @return The text appearance for the tab grid card title.
     */
    public static @ColorInt int getTitleTextColor(
            Context context,
            boolean isIncognito,
            boolean isSelected,
            @Nullable @TabGroupColorId Integer colorId) {
        if (isSelected) {
            return isIncognito
                    ? context.getColor(R.color.incognito_tab_title_selected_color)
                    : MaterialColors.getColor(context, R.attr.colorOnPrimary, TAG);
        } else {
            return SurfaceColorUpdateUtils.getCardViewTextColor(context, isIncognito, colorId);
        }
    }

    /**
     * Returns the mini-thumbnail placeholder color for the multi-thumbnail tab grid card based on
     * the incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param isSelected Whether the tab is currently selected.
     * @param colorId Color chosen by user for the TabGroup, null if not a tab group.
     * @return The mini-thumbnail placeholder color.
     */
    public static @ColorInt int getMiniThumbnailPlaceholderColor(
            Context context,
            boolean isIncognito,
            boolean isSelected,
            @Nullable @TabGroupColorId Integer colorId) {
        if (isSelected) {
            if (isIncognito) {
                return context.getColor(R.color.incognito_tab_thumbnail_placeholder_selected_color);
            }
            int alpha =
                    context.getResources()
                            .getInteger(R.integer.tab_thumbnail_placeholder_selected_color_alpha);
            @ColorInt int baseColor = SemanticColorUtils.getColorOnPrimary(context);
            return MaterialColors.compositeARGBWithAlpha(baseColor, alpha);
        }
        return SurfaceColorUpdateUtils.getCardViewMiniThumbnailPlaceholderColor(
                context, isIncognito, colorId);
    }

    /**
     * Returns the color to use for the tab grid card view background based on incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param isSelected Whether the tab is currently selected.
     * @param colorId Color chosen by user for the TabGroup, null if not a tab group.
     * @return The {@link ColorInt} for tab grid card view background.
     */
    public static @ColorInt int getCardViewBackgroundColor(
            Context context,
            boolean isIncognito,
            boolean isSelected,
            @Nullable @TabGroupColorId Integer colorId) {
        if (isSelected) {
            // Incognito does not use dynamic colors, so it can use colors from resources.
            return isIncognito
                    ? ContextCompat.getColor(context, R.color.incognito_tab_bg_selected_color)
                    : MaterialColors.getColor(context, R.attr.colorPrimary, TAG);
        } else {
            return SurfaceColorUpdateUtils.getCardViewBackgroundColor(
                    context, isIncognito, colorId);
        }
    }

    /**
     * Returns the text color for the number used on the tab group cards based on the incognito
     * mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param isSelected Whether the tab is currently selected.
     * @param colorId Color chosen by user for the TabGroup, null if not a tab group.
     * @return The text color for the number used on the tab group cards.
     */
    public static @ColorInt int getTabGroupNumberTextColor(
            Context context,
            boolean isIncognito,
            boolean isSelected,
            @Nullable @TabGroupColorId Integer colorId) {
        if (isSelected) {
            return isIncognito
                    ? context.getColor(R.color.incognito_tab_tile_number_selected_color)
                    : MaterialColors.getColor(context, R.attr.colorOnPrimary, TAG);
        }
        return SurfaceColorUpdateUtils.getCardViewGroupNumberTextColor(
                context, isIncognito, colorId);
    }

    /**
     * Returns the {@link ColorStateList} to use for the tab grid card action button based on
     * incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param isSelected Whether the tab is currently selected.
     * @return The {@link ColorStateList} for tab grid card action button.
     */
    public static ColorStateList getActionButtonTintList(
            Context context,
            boolean isIncognito,
            boolean isSelected,
            @Nullable @TabGroupColorId Integer colorId) {
        if (isSelected) {
            return isIncognito
                    ? AppCompatResources.getColorStateList(
                            context, R.color.incognito_tab_action_button_selected_color)
                    : ColorStateList.valueOf(
                            MaterialColors.getColor(context, R.attr.colorOnPrimary, TAG));
        }
        return SurfaceColorUpdateUtils.getCardViewActionButtonColor(context, isIncognito, colorId);
    }

    /**
     * Returns the {@link ColorStateList} to use for the selectable tab grid card toggle button
     * based on incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param isSelected Whether the tab is currently selected.
     * @return The {@link ColorStateList} for selectable tab grid card toggle button.
     */
    public static ColorStateList getToggleActionButtonBackgroundTintList(
            Context context, boolean isIncognito, boolean isSelected) {
        return getActionButtonTintList(context, isIncognito, isSelected, /* colorId */ null);
    }
}
