// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.ColorStateList;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.ContextCompat;

import com.google.android.material.color.MaterialColors;
import com.google.android.material.elevation.ElevationOverlayProvider;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

/** Utility class that provides theme related attributes for Tab UI. */
public class TabUiThemeProvider {
    private static final String TAG = "TabUiThemeProvider";

    /**
     * Returns the semantic color value that corresponds to colorPrimaryContainer.
     *
     * @param context {@link Context} used to retrieve color.
     */
    public static @ColorInt int getDefaultNtbContainerColor(Context context) {
        return MaterialColors.getColor(context, R.attr.colorPrimaryContainer, TAG);
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
    public static @ColorInt int getTabGroupNumberTextColor(
            Context context, boolean isIncognito, boolean isSelected) {
        if (isIncognito) {
            @ColorRes
            int colorRes =
                    isSelected
                            ? R.color.incognito_tab_tile_number_selected_color
                            : R.color.incognito_tab_tile_number_color;
            return context.getColor(colorRes);
        } else {
            return isSelected
                    ? MaterialColors.getColor(context, R.attr.colorOnPrimary, TAG)
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
        if (isIncognito) {
            @ColorRes
            int colorRes =
                    isSelected
                            ? R.color.incognito_tab_action_button_selected_color
                            : R.color.incognito_tab_action_button_color;
            return AppCompatResources.getColorStateList(context, colorRes);
        } else {
            @ColorInt
            int colorInt =
                    isSelected
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
        if (isIncognito) {
            return AppCompatResources.getColorStateList(
                    context, R.color.incognito_tab_bg_selected_color);
        }
        return ColorStateList.valueOf(
                MaterialColors.getColor(context, org.chromium.chrome.R.attr.colorPrimary, TAG));
    }

    /**
     * Returns the mini-thumbnail frame color for the multi-thumbnail tab grid card based on the
     * incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The mini-thumbnail frame color.
     */
    public static @ColorInt int getMiniThumbnailFrameColor(Context context, boolean isIncognito) {
        return isIncognito
                ? context.getColor(R.color.tab_grid_card_divider_tint_color_incognito)
                : SemanticColorUtils.getTabGridCardDividerTintColor(context);
    }

    /**
     * Returns the favicon background color based on the incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The favicon background color.
     */
    public static @ColorInt int getFaviconBackgroundColor(Context context, boolean isIncognito) {
        return context.getColor(
                isIncognito
                        ? R.color.favicon_background_color_incognito
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
        if (isIncognito) {
            @ColorRes
            int colorRes =
                    isSelected
                            ? R.color.incognito_tab_group_hovered_bg_selected_color
                            : R.color.incognito_tab_group_hovered_bg_color;
            return AppCompatResources.getColorStateList(context, colorRes);
        } else {
            if (isSelected) {
                @ColorInt
                int baseColor =
                        MaterialColors.getColor(
                                context, org.chromium.chrome.R.attr.colorPrimary, TAG);
                int alpha =
                        context.getResources()
                                .getInteger(
                                        R.integer
                                                .tab_grid_hovered_card_background_selected_color_alpha);
                return ColorStateList.valueOf(
                        MaterialColors.compositeARGBWithAlpha(baseColor, alpha));
            } else {
                float backgroundElevation =
                        context.getResources().getDimension(R.dimen.default_elevation_4);
                @ColorInt
                int baseColor =
                        new ElevationOverlayProvider(context)
                                .compositeOverlayWithThemeSurfaceColorIfNeeded(backgroundElevation);
                int alpha =
                        context.getResources()
                                .getInteger(R.integer.tab_grid_hovered_card_background_color_alpha);
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
    public static @ColorInt int getTabGridDialogBackgroundColor(
            Context context, boolean isIncognito) {
        if (isIncognito) {
            return context.getColor(R.color.incognito_tab_grid_dialog_background_color);
        } else {
            return MaterialColors.getColor(context, R.attr.colorSurface, TAG);
        }
    }

    private static @ColorInt int getTabGridDialogUngroupBarBackgroundColor(
            Context context, boolean isIncognito, boolean isTabHovered) {
        if (isIncognito) {
            return context.getColor(
                    isTabHovered
                            ? R.color.incognito_tab_grid_dialog_ungroup_bar_bg_hovered_color
                            : R.color.incognito_tab_grid_dialog_background_color);
        } else {
            return MaterialColors.getColor(
                    context,
                    isTabHovered ? org.chromium.chrome.R.attr.colorPrimary : R.attr.colorSurface,
                    TAG);
        }
    }

    private static @ColorInt int getTabGridDialogUngroupBarTextColor(
            Context context, boolean isIncognito, boolean isTabHovered) {
        if (isIncognito) {
            return context.getColor(
                    isTabHovered
                            ? R.color.incognito_tab_grid_dialog_ungroup_bar_text_hovered_color
                            : R.color.incognito_tab_grid_dialog_ungroup_bar_text_color);
        } else {
            return MaterialColors.getColor(
                    context,
                    isTabHovered ? R.attr.colorOnPrimary : org.chromium.chrome.R.attr.colorPrimary,
                    TAG);
        }
    }

    /**
     * Returns the background color used for the ungroup bar in tab grid dialog.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The background color for the ungroup bar in tab grid dialog.
     */
    public static @ColorInt int getTabGridDialogUngroupBarTextColor(
            Context context, boolean isIncognito) {
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
    public static @ColorInt int getTabGridDialogUngroupBarHoveredTextColor(
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
    public static @ColorInt int getTabGridDialogUngroupBarBackgroundColor(
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
    public static @ColorInt int getTabGridDialogUngroupBarHoveredBackgroundColor(
            Context context, boolean isIncognito) {
        return getTabGridDialogUngroupBarBackgroundColor(context, isIncognito, true);
    }

    /**
     * Returns the {@link ColorStateList} to use for the strip tab hover card based on the incognito
     * mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The {@link ColorStateList} for the strip tab hover card.
     */
    public static ColorStateList getStripTabHoverCardBackgroundTintList(
            Context context, boolean isIncognito) {
        int backgroundTint =
                isIncognito
                        ? ContextCompat.getColor(
                                context, R.color.default_bg_color_dark_elev_5_baseline)
                        : ChromeColors.getSurfaceColor(
                                context, R.dimen.tab_hover_card_bg_color_elev);
        return ColorStateList.valueOf(backgroundTint);
    }

    /**
     * Returns the text color for the strip tab hover card title based on the incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The text color for the strip tab hover card title.
     */
    public static @ColorInt int getStripTabHoverCardTextColorPrimary(
            Context context, boolean isIncognito) {
        return isIncognito
                ? context.getColor(R.color.default_text_color_light)
                : SemanticColorUtils.getDefaultTextColor(context);
    }

    /**
     * Returns the text color for the strip tab hover card URL based on the incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The text color for the strip tab hover card URL.
     */
    public static @ColorInt int getStripTabHoverCardTextColorSecondary(
            Context context, boolean isIncognito) {
        return isIncognito
                ? context.getColor(R.color.default_text_color_secondary_light)
                : SemanticColorUtils.getDefaultTextColorSecondary(context);
    }

    /**
     * Return the background color used for tab UI toolbar in selection edit mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The background color for the toolbar when tab switcher is in selection edit mode.
     */
    public static @ColorInt int getTabSelectionToolbarBackground(
            Context context, boolean isIncognito) {
        if (isIncognito) {
            return context.getColor(R.color.incognito_tab_list_editor_toolbar_bg_color);
        } else {
            return MaterialColors.getColor(context, R.attr.colorSurface, TAG);
        }
    }

    /**
     * Returns the {@link ColorStateList} for icons on the tab UI toolbar in selection edit mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The {@link ColorStateList} for icons on the toolbar when tab switcher is in selection
     *         edit mode.
     */
    public static ColorStateList getTabSelectionToolbarIconTintList(
            Context context, boolean isIncognito) {
        return AppCompatResources.getColorStateList(
                context,
                isIncognito
                        ? R.color.default_text_color_light_list
                        : R.color.default_text_color_list);
    }

    /**
     * Returns the message card background resource id based on the incognito mode.
     *
     * @param isIncognito Whether the resource is used for incognito mode.
     * @return The background resource id for message card view.
     */
    public static int getMessageCardBackgroundResourceId(boolean isIncognito) {
        return isIncognito ? R.drawable.incognito_card_bg : R.drawable.card_with_corners_background;
    }

    /**
     * Returns the text appearance for the message card title based on the incognito mode.
     *
     * @param isIncognito Whether the text appearance is used for incognito mode.
     * @return The text appearance for the message card title.
     */
    public static int getMessageCardTitleTextAppearance(boolean isIncognito) {
        return isIncognito
                ? R.style.TextAppearance_TextLarge_Primary_Baseline_Light
                : R.style.TextAppearance_TextLarge_Primary;
    }

    /**
     * Returns the text appearance for message card description based on the incognito mode.
     *
     * @param isIncognito Whether the text appearance is used for incognito mode.
     * @return The text appearance for the message card description.
     */
    public static int getMessageCardDescriptionTextAppearance(boolean isIncognito) {
        return isIncognito
                ? R.style.TextAppearance_TextMedium_Primary_Baseline_Light
                : R.style.TextAppearance_TextMedium_Primary;
    }

    /**
     * Returns the text appearance for the message card action button based on the incognito mode.
     *
     * @param isIncognito Whether the text appearance is used for incognito mode.
     * @return The text appearance for the message card action button.
     */
    public static int getMessageCardActionButtonTextAppearance(boolean isIncognito) {
        return isIncognito
                ? R.style.TextAppearance_Button_Text_Blue_Dark
                : R.style.TextAppearance_Button_Text_Blue;
    }

    /**
     * Returns the text appearance for the message card title based on the incognito mode.
     *
     * @param isIncognito Whether the text appearance is used for incognito mode.
     * @return The text appearance for the message card title.
     */
    public static int getLargeMessageCardTitleTextAppearance(boolean isIncognito) {
        return isIncognito
                ? R.style.TextAppearance_TextLarge_Primary_Baseline_Light
                : R.style.TextAppearance_TextLarge_Primary;
    }

    /**
     * Returns the text appearance for large message card description based on the incognito mode.
     *
     * @param isIncognito Whether the text appearance is used for incognito mode.
     * @return The text appearance for the message card description.
     */
    public static int getLargeMessageCardDescriptionTextAppearance(boolean isIncognito) {
        return isIncognito
                ? R.style.TextAppearance_TextMedium_Secondary_Baseline_Light
                : R.style.TextAppearance_TextMedium_Secondary;
    }

    /**
     * Returns the text appearance for the large message card action button based on the incognito
     * mode.
     *
     * @param isIncognito Whether the text appearance is used for incognito mode.
     * @return The appearance for the message card action button text.
     */
    public static int getLargeMessageCardActionButtonTextAppearance(boolean isIncognito) {
        return isIncognito
                ? R.style.TextAppearance_Button_Text_Filled_Baseline_Dark
                : R.style.TextAppearance_Button_Text_Filled;
    }

    /**
     * Returns the color for the large message card action button based on the
     * incognito mode.
     *
     * @param context The {@link Context} to use to fetch the resources.
     * @param isIncognito Whether the color is used for incognito mode.
     *
     * @return The {@link ColorInt} to set for the large message card action button.
     */
    public static @ColorInt int getLargeMessageCardActionButtonColor(
            Context context, boolean isIncognito) {
        return isIncognito
                ? context.getColor(R.color.filled_button_bg_color_light)
                : context.getColor(R.color.filled_button_bg_color);
    }

    /**
     * Returns the text color for the message card secondary action button based on the
     * incognito mode.
     *
     * @param context The {@link Context} to use to fetch the resources.
     * @param isIncognito Whether the text appearance is used for incognito mode.
     *
     * @return The {@link ColorInt} to set for the message card secondary action button.
     */
    public static @ColorInt int getMessageCardSecondaryActionButtonColor(
            Context context, boolean isIncognito) {
        return isIncognito
                ? context.getColor(R.color.default_text_color_link_light)
                : SemanticColorUtils.getDefaultTextColorLink(context);
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
        return AppCompatResources.getColorStateList(
                context,
                isIncognito
                        ? R.color.default_icon_color_light
                        : R.color.default_icon_color_tint_list);
    }

    /**
     * Return the size represented by dimension for padding between tab cards.
     *
     * @param context {@link Context} to retrieve dimension.
     * @return The padding between tab cards in float number.
     */
    public static float getTabCardPaddingDimension(Context context) {
        return context.getResources().getDimension(R.dimen.tab_grid_card_between_card_padding);
    }

    /**
     * Return the space represented by dimension for spaces between mini thumbnails in a group tab.
     * @param context {@link Context} to retrieve dimension.
     * @return The padding between between mini thumbnails in float number.
     */
    public static float getTabMiniThumbnailPaddingDimension(Context context) {
        return context.getResources().getDimension(R.dimen.tab_grid_card_thumbnail_margin);
    }

    /**
     * Get the margin space from tab grid cards outline to its outbound represented by dimension.
     * This space is used to calculate the starting point for the tab grid dialog.
     *
     * @param context {@link Context} to retrieve dimension.
     * @return The margin between tab cards in float number.
     */
    public static float getTabGridCardMargin(Context context) {
        return context.getResources().getDimension(R.dimen.tab_grid_card_margin);
    }

    /**
     * Return the size represented by dimension for margin around message cards.
     * @param context {@link Context} to retrieve dimension.
     * @return The margin around message cards in float number.
     */
    public static float getMessageCardMarginDimension(Context context) {
        return context.getResources().getDimension(R.dimen.tab_list_selected_inset);
    }
}
