// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.TypedArray;
import android.graphics.Color;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.StyleRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.ContextCompat;

import com.google.android.material.color.MaterialColors;
import com.google.android.material.elevation.ElevationOverlayProvider;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.tab_ui.R;

/**
 * Utility class that provides theme related attributes for Tab UI.
 */
public class TabUiThemeProvider {
    private static final String TAG = "TabUiThemeProvider";
    /**
     * Returns the {@link ColorStateList} to use for the tab grid card view background based on
     * incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param isSelected Whether the tab is currently selected.
     * @return The {@link ColorStateList} for tab grid card view background.
     */
    public static ColorStateList getCardViewTintList(
            Context context, boolean isIncognito, boolean isSelected) {
        if (!themeRefactorEnabled()) {
            return AppCompatResources.getColorStateList(context,
                    isIncognito ? R.color.tab_grid_card_view_tint_color_incognito
                                : R.color.tab_grid_card_view_tint_color);
        }

        if (isIncognito) {
            // Incognito does not use dynamic colors, so it can use colors from resources.
            @ColorRes
            int colorRes = isSelected ? R.color.incognito_tab_bg_selected_color
                                      : R.color.incognito_tab_bg_color;
            return AppCompatResources.getColorStateList(context, colorRes);
        } else {
            float tabElevation = context.getResources().getDimension(R.dimen.tab_bg_elevation);
            @ColorInt
            int colorInt = isSelected
                    ? MaterialColors.getColor(context, R.attr.colorPrimary, TAG)
                    : new ElevationOverlayProvider(context)
                              .compositeOverlayWithThemeSurfaceColorIfNeeded(tabElevation);
            return ColorStateList.valueOf(colorInt);
        }
    }

    /**
     * Returns the text color for the number used on the tab group cards based on the incognito
     * mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param isSelected Whether the tab is currently selected.
     * @return The text color for the number used on the tab group cards.
     */
    @ColorInt
    public static int getTabGroupNumberTextColor(
            Context context, boolean isIncognito, boolean isSelected) {
        if (!themeRefactorEnabled()) {
            return ApiCompatibilityUtils.getColor(context.getResources(),
                    isIncognito ? R.color.tab_group_number_text_color_incognito
                                : R.color.tab_group_number_text_color);
        }
        if (isIncognito) {
            @ColorRes
            int colorRes = isSelected ? R.color.incognito_tab_tile_number_selected_color
                                      : R.color.incognito_tab_tile_number_color;
            return ApiCompatibilityUtils.getColor(context.getResources(), colorRes);
        } else {
            return isSelected
                    ? MaterialColors.getColor(context, R.attr.colorOnPrimaryContainer, TAG)
                    : MaterialColors.getColor(context, R.attr.colorOnSurface, TAG);
        }
    }

    /**
     * Returns the title text appearance for the tab grid card based on the incognito mode.
     *
     * @param isIncognito Whether the text appearance is used for incognito mode.
     * @param isSelected Whether the tab is currently selected.
     * @return The text appearance for the tab grid card title.
     */
    @ColorInt
    public static int getTitleTextColor(Context context, boolean isIncognito, boolean isSelected) {
        if (!themeRefactorEnabled()) {
            return ApiCompatibilityUtils.getColor(context.getResources(),
                    isIncognito ? R.color.tab_grid_card_title_text_color_incognito
                                : R.color.tab_grid_card_title_text_color);
        }

        if (isIncognito) {
            @ColorRes
            int colorRes = isSelected ? R.color.incognito_tab_title_selected_color
                                      : R.color.incognito_tab_title_color;
            return ApiCompatibilityUtils.getColor(context.getResources(), colorRes);
        } else {
            return isSelected ? MaterialColors.getColor(context, R.attr.colorOnPrimary, TAG)
                              : MaterialColors.getColor(context, R.attr.colorOnSurface, TAG);
        }
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
            Context context, boolean isIncognito, boolean isSelected) {
        if (!themeRefactorEnabled()) {
            return AppCompatResources.getColorStateList(context,
                    isIncognito ? R.color.tab_grid_card_action_button_tint_color_incognito
                                : R.color.tab_grid_card_action_button_tint_color);
        }

        if (isIncognito) {
            @ColorRes
            int colorRes = isSelected ? R.color.incognito_tab_action_button_selected_color
                                      : R.color.incognito_tab_action_button_color;
            return AppCompatResources.getColorStateList(context, colorRes);
        } else {
            @ColorInt
            int colorInt = isSelected
                    ? MaterialColors.getColor(context, R.attr.colorOnPrimary, TAG)
                    : MaterialColors.getColor(context, R.attr.colorOnSurfaceVariant, TAG);
            return ColorStateList.valueOf(colorInt);
        }
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
        if (!themeRefactorEnabled()) {
            @ColorRes
            int colorRes;
            if (isSelected) {
                colorRes = isIncognito ? R.color.tab_grid_card_selected_color_incognito
                                       : R.color.tab_grid_card_selected_color;
            } else {
                colorRes =
                        isIncognito ? R.color.default_icon_color_light : R.color.default_icon_color;
            }
            return AppCompatResources.getColorStateList(context, colorRes);
        }
        return getActionButtonTintList(context, isIncognito, isSelected);
    }

    /**
     * Returns the {@link ColorStateList} to use for the "check" drawable on selectable tab grid
     * card toggle button based on incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The {@link ColorStateList} for "check" drawable.
     */
    public static ColorStateList getToggleActionButtonCheckedDrawableTintList(
            Context context, boolean isIncognito) {
        if (!themeRefactorEnabled()) {
            return AppCompatResources.getColorStateList(context,
                    isIncognito ? R.color.default_icon_color_dark
                                : R.color.default_icon_color_inverse);
        }
        if (isIncognito) {
            return AppCompatResources.getColorStateList(
                    context, R.color.incognito_tab_bg_selected_color);
        }
        return ColorStateList.valueOf(MaterialColors.getColor(context, R.attr.colorPrimary, TAG));
    }

    /**
     * Returns the {@link ColorStateList} to use for the plus sign in new tab tile based on the
     * incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The {@link ColorStateList} for new tab tile plus sign color.
     */
    public static ColorStateList getNewTabTilePlusTintList(Context context, boolean isIncognito) {
        return AppCompatResources.getColorStateList(context,
                isIncognito ? R.color.new_tab_tile_plus_color_incognito
                            : R.color.new_tab_tile_plus_color);
    }

    /**
     * Returns the divider color for tab grid card based on the incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The divider color for tab grid card.
     */
    @ColorInt
    public static int getDividerColor(Context context, boolean isIncognito) {
        return ApiCompatibilityUtils.getColor(context.getResources(),
                isIncognito ? R.color.tab_grid_card_divider_tint_color_incognito
                            : R.color.tab_grid_card_divider_tint_color);
    }

    /**
     * Returns the thumbnail placeholder color resource id based on the incognito mode.
     *
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The thumbnail placeholder color resource id.
     */
    public static int getThumbnailPlaceHolderColorResource(boolean isIncognito) {
        return isIncognito ? R.color.tab_grid_card_thumbnail_placeholder_color_incognito
                           : R.color.tab_grid_card_thumbnail_placeholder_color;
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
    @ColorInt
    public static int getMiniThumbnailPlaceHolderColor(
            Context context, boolean isIncognito, boolean isSelected) {
        if (!themeRefactorEnabled()) {
            return ApiCompatibilityUtils.getColor(context.getResources(),
                    isIncognito ? R.color.tab_list_mini_card_default_background_color_incognito
                                : R.color.tab_list_mini_card_default_background_color);
        }

        if (isIncognito) {
            @ColorRes
            int colorRes = isSelected ? R.color.incognito_tab_thumbnail_placeholder_selected_color
                                      : R.color.incognito_tab_thumbnail_placeholder_color;
            return ApiCompatibilityUtils.getColor(context.getResources(), colorRes);
        } else {
            int alpha = context.getResources().getInteger(isSelected
                            ? R.integer.tab_thumbnail_placeholder_selected_color_alpha
                            : R.integer.tab_thumbnail_placeholder_color_alpha);

            @StyleRes
            int styleRes = isSelected ? R.style.TabThumbnailPlaceHolderStyle_Selected
                                      : R.style.TabThumbnailPlaceHolderStyle;
            TypedArray ta =
                    context.obtainStyledAttributes(styleRes, R.styleable.TabThumbnailPlaceHolder);

            @ColorInt
            int baseColor = ta.getColor(
                    R.styleable.TabThumbnailPlaceHolder_colorTileBase, Color.TRANSPARENT);
            float tileSurfaceElevation =
                    ta.getDimension(R.styleable.TabThumbnailPlaceHolder_elevationTileBase, 0);

            ta.recycle();
            if (tileSurfaceElevation != 0) {
                ElevationOverlayProvider eop = new ElevationOverlayProvider(context);
                baseColor = eop.compositeOverlay(baseColor, tileSurfaceElevation);
            }

            return MaterialColors.compositeARGBWithAlpha(baseColor, alpha);
        }
    }

    /**
     * Returns the mini-thumbnail frame color for the multi-thumbnail tab grid card based on the
     * incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The mini-thumbnail frame color.
     */
    @ColorInt
    public static int getMiniThumbnailFrameColor(Context context, boolean isIncognito) {
        return ApiCompatibilityUtils.getColor(context.getResources(),
                isIncognito ? R.color.tab_grid_card_divider_tint_color_incognito
                            : R.color.tab_grid_card_divider_tint_color);
    }

    /**
     * Returns the favicon background color based on the incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The favicon background color.
     */
    @ColorInt
    public static int getFaviconBackgroundColor(Context context, boolean isIncognito) {
        return ApiCompatibilityUtils.getColor(context.getResources(),
                isIncognito ? R.color.favicon_background_color_incognito
                            : R.color.favicon_background_color);
    }

    /**
     * Returns the {@link ColorStateList} for background view when a tab grid card is hovered by
     * another card based on the incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param isSelected Whether the tab is currently selected.
     * @return The {@link ColorStateList} for hovered tab grid card background.
     */
    public static ColorStateList getHoveredCardBackgroundTintList(
            Context context, boolean isIncognito, boolean isSelected) {
        if (!themeRefactorEnabled()) {
            return AppCompatResources.getColorStateList(context,
                    isIncognito ? R.color.hovered_tab_grid_card_background_color_incognito
                                : R.color.hovered_tab_grid_card_background_color);
        }

        if (isIncognito) {
            @ColorRes
            int colorRes = isSelected ? R.color.incognito_tab_group_hovered_bg_selected_color
                                      : R.color.incognito_tab_group_hovered_bg_color;
            return AppCompatResources.getColorStateList(context, colorRes);
        } else {
            if (isSelected) {
                @ColorInt
                int baseColor = MaterialColors.getColor(context, R.attr.colorPrimary, TAG);
                int alpha = context.getResources().getInteger(
                        R.integer.tab_grid_hovered_card_background_selected_color_alpha);
                return ColorStateList.valueOf(
                        MaterialColors.compositeARGBWithAlpha(baseColor, alpha));
            } else {
                float backgroundElevation =
                        context.getResources().getDimension(R.dimen.default_elevation_4);
                @ColorInt
                int baseColor =
                        new ElevationOverlayProvider(context)
                                .compositeOverlayWithThemeSurfaceColorIfNeeded(backgroundElevation);
                int alpha = context.getResources().getInteger(
                        R.integer.tab_grid_hovered_card_background_color_alpha);
                return ColorStateList.valueOf(
                        MaterialColors.compositeARGBWithAlpha(baseColor, alpha));
            }
        }
    }

    /**
     * Returns the color used for tab grid dialog background based on the incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The background color for tab grid dialog.
     */
    @ColorInt
    public static int getTabGridDialogBackgroundColor(Context context, boolean isIncognito) {
        if (!themeRefactorEnabled()) {
            return ContextCompat.getColor(context,
                    isIncognito ? R.color.tab_grid_dialog_background_color_incognito
                                : R.color.tab_grid_dialog_background_color);
        }

        if (isIncognito) {
            return ApiCompatibilityUtils.getColor(
                    context.getResources(), R.color.incognito_tab_grid_dialog_background_color);
        } else {
            return MaterialColors.getColor(context, R.attr.colorSurface, TAG);
        }
    }

    @ColorInt
    private static int getTabGridDialogUngroupBarBackgroundColor(
            Context context, boolean isIncognito, boolean isTabHovered) {
        if (!themeRefactorEnabled()) {
            @ColorRes
            int colorRes;
            if (isTabHovered) {
                colorRes = isIncognito ? R.color.tab_grid_card_selected_color_incognito
                                       : R.color.tab_grid_card_selected_color;
            } else {
                colorRes = isIncognito ? R.color.tab_grid_dialog_background_color_incognito
                                       : R.color.tab_grid_dialog_background_color;
            }
            return ApiCompatibilityUtils.getColor(context.getResources(), colorRes);
        }

        if (isIncognito) {
            return ApiCompatibilityUtils.getColor(context.getResources(),
                    isTabHovered ? R.color.incognito_tab_grid_dialog_ungroup_bar_bg_hovered_color
                                 : R.color.incognito_tab_grid_dialog_background_color);
        } else {
            return MaterialColors.getColor(
                    context, isTabHovered ? R.attr.colorPrimary : R.attr.colorSurface, TAG);
        }
    }

    @ColorInt
    private static int getTabGridDialogUngroupBarTextColor(
            Context context, boolean isIncognito, boolean isTabHovered) {
        if (!themeRefactorEnabled()) {
            @ColorRes
            int colorRes;
            if (isTabHovered) {
                colorRes = R.color.tab_grid_dialog_ungroup_button_text_color_hovered;
            } else {
                colorRes = isIncognito ? R.color.tab_grid_dialog_ungroup_button_text_color_incognito
                                       : R.color.tab_grid_dialog_ungroup_button_text_color;
            }
            return ApiCompatibilityUtils.getColor(context.getResources(), colorRes);
        }

        if (isIncognito) {
            return ApiCompatibilityUtils.getColor(context.getResources(),
                    isTabHovered ? R.color.incognito_tab_grid_dialog_ungroup_bar_text_hovered_color
                                 : R.color.incognito_tab_grid_dialog_ungroup_bar_text_color);
        } else {
            return MaterialColors.getColor(
                    context, isTabHovered ? R.attr.colorOnPrimary : R.attr.colorPrimary, TAG);
        }
    }

    /**
     * Returns the background color used for the ungroup bar in tab grid dialog.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The background color for the ungroup bar in tab grid dialog.
     */
    @ColorInt
    public static int getTabGridDialogUngroupBarTextColor(Context context, boolean isIncognito) {
        return getTabGridDialogUngroupBarTextColor(context, isIncognito, false);
    }

    /**
     * Returns the background color used for the ungroup bar in tab grid dialog when a tab is
     * hovered.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The background color for the ungroup bar in tab grid dialog.
     */
    @ColorInt
    public static int getTabGridDialogUngroupBarHoveredTextColor(
            Context context, boolean isIncognito) {
        return getTabGridDialogUngroupBarTextColor(context, isIncognito, true);
    }

    /**
     * Returns the color used for the ungroup bar text in tab grid dialog.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The color for the ungroup bar text in tab grid dialog.
     */
    @ColorInt
    public static int getTabGridDialogUngroupBarBackgroundColor(
            Context context, boolean isIncognito) {
        return getTabGridDialogUngroupBarBackgroundColor(context, isIncognito, false);
    }

    /**
     * Returns the color used for the ungroup bar text in tab grid dialog when a tab is hovered.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The color for the ungroup bar text in tab grid dialog.
     */
    @ColorInt
    public static int getTabGridDialogUngroupBarHoveredBackgroundColor(
            Context context, boolean isIncognito) {
        return getTabGridDialogUngroupBarBackgroundColor(context, isIncognito, true);
    }

    /**
     * Returns the message card background resource id based on the incognito mode.
     *
     * @param isIncognito Whether the resource is used for incognito mode.
     * @return The background resource id for message card view.
     */
    public static int getMessageCardBackgroundResourceId(boolean isIncognito) {
        return isIncognito ? R.drawable.message_card_background_with_inset_incognito
                           : R.drawable.message_card_background_with_inset;
    }

    /**
     * Returns the text appearance for the message card description based on the incognito mode.
     *
     * @param isIncognito Whether the text appearance is used for incognito mode.
     * @return The text appearance for the message card description.
     */
    public static int getMessageCardDescriptionTextAppearance(boolean isIncognito) {
        return isIncognito ? R.style.TextAppearance_TextMedium_Primary_Light
                           : R.style.TextAppearance_TextMedium_Primary;
    }

    /**
     * Returns the text appearance for the message card action button based on the incognito mode.
     *
     * @param isIncognito Whether the text appearance is used for incognito mode.
     * @return The text appearance for the message card action button.
     */
    public static int getMessageCardActionButtonTextAppearance(boolean isIncognito) {
        return isIncognito ? R.style.TextAppearance_Button_Text_Blue_Dark
                           : R.style.TextAppearance_Button_Text_Blue;
    }

    /**
     * Returns the {@link ColorStateList} to use for the message card close button based on
     * incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The {@link ColorStateList} for message card close button.
     */
    public static ColorStateList getMessageCardCloseButtonTintList(
            Context context, boolean isIncognito) {
        return AppCompatResources.getColorStateList(context,
                isIncognito ? R.color.default_icon_color_light : R.color.default_icon_color);
    }

    /**
     * Return the padding around favicon if it is visible.
     * @param context {@link Context} used to retrieve dimension.
     * @return The padding space around favicon.
     */
    public static float getTabCardTopFaviconPadding(Context context) {
        return context.getResources().getDimension(themeRefactorEnabled()
                        ? R.dimen.tab_grid_card_favicon_padding
                        : R.dimen.tab_list_card_padding);
    }

    /**
     * Return the size represented by dimension for padding between tab cards.
     * @param context {@link Context} to retrieve dimension.
     * @return The padding between tab cards in float number.
     */
    public static float getTabCardPaddingDimension(Context context) {
        return context.getResources().getDimension(themeRefactorEnabled()
                        ? R.dimen.tab_grid_card_thumbnail_margin
                        : R.dimen.tab_list_card_padding);
    }

    /**
     * Return the insect dimension around the selection button for tab grid card.
     * @param context {@link Context} to retrieve dimension.
     *
     * @return The insect dimension around the selection button for tab grid card.
     */
    public static float getTabGridCardSelectButtonInsectDimension(Context context) {
        return context.getResources().getDimension(themeRefactorEnabled()
                        ? R.dimen.tab_grid_card_toggle_button_background_inset
                        : R.dimen.selection_tab_grid_toggle_button_inset);
    }

    /**
     * Returns the style resource Id that requires for Tab UI.
     *
     * @return The resource Id for the theme overlay used for tab UI.
     */
    @StyleRes
    public static int getThemeOverlayStyleResourceId() {
        return themeRefactorEnabled() ? R.style.ThemeRefactorOverlay_Enabled_TabUi
                                      : R.style.ThemeRefactorOverlay_Disabled_TabUi;
    }

    /** Return if theme refactor is enabled. **/
    static boolean themeRefactorEnabled() {
        return CachedFeatureFlags.isEnabled(ChromeFeatureList.THEME_REFACTOR_ANDROID);
    }
}
