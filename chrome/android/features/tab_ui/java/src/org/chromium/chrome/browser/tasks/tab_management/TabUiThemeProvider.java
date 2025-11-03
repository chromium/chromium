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

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab_ui.TabCardThemeUtil;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.CreationMode;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.tab_groups.TabGroupColorId;

/** Utility class that provides theme related attributes for Tab UI. */
@NullMarked
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

    public static @ColorInt int getGridTabSwitcherBackgroundColor(
            Context context, boolean isIncognito) {
        return isIncognito
                ? ContextCompat.getColor(context, R.color.default_bg_color_dark)
                : SemanticColorUtils.getDefaultBgColor(context);
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
        return ColorStateList.valueOf(MaterialColors.getColor(context, R.attr.colorPrimary, TAG));
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
        return isIncognito
                ? context.getColor(R.color.favicon_background_color_incognito)
                : SemanticColorUtils.getColorSurfaceBright(context);
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
                int baseColor = MaterialColors.getColor(context, R.attr.colorPrimary, TAG);
                int alpha =
                        context.getResources()
                                .getInteger(
                                        R.integer
                                                .tab_grid_hovered_card_background_selected_color_alpha);
                return ColorStateList.valueOf(
                        MaterialColors.compositeARGBWithAlpha(baseColor, alpha));
            } else {

                @ColorInt int baseColor = SemanticColorUtils.getColorSurfaceContainerHigh(context);
                int alpha =
                        context.getResources()
                                .getInteger(R.integer.tab_grid_hovered_card_background_color_alpha);
                return ColorStateList.valueOf(
                        MaterialColors.compositeARGBWithAlpha(baseColor, alpha));
            }
        }
    }

    /**
     * Returns the color used for tab selector list background based on the incognito mode and
     * creation mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param creationMode The mode of creation of the tab selector list.
     * @return The background color.
     */
    public static @ColorInt int getTabGridDialogBackgroundColor(
            Context context, boolean isIncognito, @CreationMode int creationMode) {
        if (creationMode == CreationMode.DIALOG) {
            return getTabGridDialogBackgroundColor(context, isIncognito);
        } else {
            return getGridTabSwitcherBackgroundColor(context, isIncognito);
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
            return context.getColor(R.color.gm3_baseline_surface_container_dark);
        } else {
            return ContextCompat.getColor(context, R.color.tab_grid_dialog_bg_color);
        }
    }

    private static @ColorInt int getTabGridDialogUngroupBarBackgroundColor(
            Context context, boolean isIncognito, boolean isTabHovered) {
        if (isTabHovered) {
            return isIncognito
                    ? context.getColor(
                            R.color.incognito_tab_grid_dialog_ungroup_bar_bg_hovered_color)
                    : SemanticColorUtils.getColorPrimary(context);
        }
        return getTabGridDialogBackgroundColor(context, isIncognito);
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
                        ? ContextCompat.getColor(context, R.color.incognito_tab_hover_card_bg_color)
                        : ContextCompat.getColor(context, R.color.tab_hover_card_bg_color);
        return ColorStateList.valueOf(backgroundTint);
    }

    /**
     * Returns the {@link ColorStateList} to use for tab card highlighting based on the incognito
     * mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The {@link ColorStateList} for the tab card highlight.
     */
    public static ColorStateList getTabCardHighlightBackgroundTintList(
            Context context, boolean isIncognito) {
        int backgroundTint =
                isIncognito
                        ? ContextCompat.getColor(
                                context, R.color.incognito_tab_highlight_card_bg_color)
                        : ContextCompat.getColor(context, R.color.tab_highlight_card_bg_color);
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
     * @param creationMode The mode of creation of the tab selector list.
     * @return The background color for the toolbar when tab switcher is in selection edit mode.
     */
    public static @ColorInt int getTabSelectionToolbarBackground(
            Context context, boolean isIncognito, @CreationMode int creationMode) {
        if (creationMode == CreationMode.DIALOG) {
            return getTabGridDialogBackgroundColor(context, isIncognito);
        }
        return getGridTabSwitcherBackgroundColor(context, isIncognito);
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
        return isIncognito
                ? R.drawable.card_background_corners_16dp_baseline_dark
                : R.drawable.card_background_corners_16dp;
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
                : R.style.TextAppearance_ClickableButtonInverse;
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

    /**
     * Returns the color used for the shared tab notification bubble.
     *
     * @param context {@link Context} used to retrieve color.
     * @return The color for the tab notification bubble.
     */
    public static @ColorInt int getTabBubbleFillColor(Context context) {
        return MaterialColors.getColor(context, R.attr.colorPrimary, TAG);
    }

    /**
     * Get the background fill color used for the tab group cluster quarter.
     *
     * @param context {@link Context} used to retrieve color.
     * @param showFavicon Whether the quarter is showing a favicon.
     * @param enableContainment Whether the tile is shown in the containment list.
     * @return The color for the tab group favicon quarter.
     */
    public static @ColorInt int getTabGroupFaviconQuarterFillColor(
            Context context, boolean showFavicon, boolean enableContainment) {
        if (enableContainment) {
            return showFavicon
                    ? SemanticColorUtils.getColorSurfaceContainer(context)
                    : SemanticColorUtils.getColorSurfaceContainerLow(context);
        }
        return showFavicon
                ? SemanticColorUtils.getColorSurfaceBright(context)
                : ContextCompat.getColor(
                        context, R.color.tab_group_favicon_quater_empty_fill_color);
    }

    /**
     * Get the background color tint for the tab group cluster background.
     *
     * @param context {@link Context} used to retrieve color.
     * @param enableContainment Whether the cluster row is shown in the containment list.
     * @return The color for the tab group favicon quarter.
     */
    public static @ColorInt int getTabGroupClusterBackgroundTint(
            Context context, boolean enableContainment) {
        return enableContainment
                ? SemanticColorUtils.getColorSurfaceBright(context)
                : SemanticColorUtils.getColorSurfaceContainer(context);
    }

    /**
     * Returns the color used by dialogs as background.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The color for the dialog background.
     */
    public static @ColorInt int getColorPickerDialogBackgroundColor(
            Context context, boolean isIncognito) {
        return isIncognito
                ? ContextCompat.getColor(
                        context, R.color.tab_group_color_picker_selection_bg_incognito)
                : SemanticColorUtils.getDialogBgColor(context);
    }

    /**
     * Returns the color used for an empty thumbnail.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param isSelected Whether the tab is currently selected.
     * @return The color for the empty thumbnail.
     */
    public static @ColorInt int getEmptyThumbnailColor(
            Context context,
            boolean isIncognito,
            boolean isSelected,
            @Nullable @TabGroupColorId Integer colorId) {
        return TabCardThemeUtil.getCardViewBackgroundColor(
                context, isIncognito, isSelected, colorId);
    }

    /**
     * Returns the color used for the tab switcher pane hairline.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The color for the pane hairline.
     */
    public static @ColorInt int getPaneHairlineColor(Context context, boolean isIncognito) {
        return isIncognito
                ? ContextCompat.getColor(
                        context, org.chromium.chrome.tab_ui.R.color.divider_line_bg_color_light)
                : SemanticColorUtils.getDividerLineBgColor(context);
    }
}
