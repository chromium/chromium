// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;

import androidx.annotation.ColorInt;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.tab_ui.R;

/**
 * Color utility class for a Tab Grid card.
 */
public class TabUiColorProvider {
    /**
     * Returns the {@link ColorStateList} to use for the tab grid card view background based on
     * incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The {@link ColorStateList} for tab grid card view background.
     */
    public static ColorStateList getCardViewTintList(Context context, boolean isIncognito) {
        return AppCompatResources.getColorStateList(context,
                isIncognito ? R.color.tab_grid_card_view_tint_color_incognito
                            : R.color.tab_grid_card_view_tint_color);
    }

    /**
     * Returns the {@link Drawable} for tab grid card background view based on the incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The {@link Drawable} for tab grid card view.
     */
    public static Drawable getCardViewBackgroundDrawable(Context context, boolean isIncognito) {
        return ApiCompatibilityUtils.getDrawable(context.getResources(),
                isIncognito ? R.drawable.popup_bg_dark : R.drawable.popup_bg_tinted);
    }

    /**
     * Returns the title text color for the tab grid card based on the incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The text color for the tab grid card title.
     */
    @ColorInt
    public static int getTitleTextColor(Context context, boolean isIncognito) {
        return ApiCompatibilityUtils.getColor(context.getResources(),
                isIncognito ? R.color.tab_grid_card_title_text_color_incognito
                            : R.color.tab_grid_card_title_text_color);
    }

    /**
     * Returns the title text appearance for the tab grid card based on the incognito mode.
     *
     * @param isIncognito Whether the text appearance is used for incognito mode.
     * @return The text appearance for the tab grid card title.
     */
    public static int getTitleTextAppearance(boolean isIncognito) {
        return isIncognito ? R.style.TextAppearance_TextMediumThick_Primary_Light
                           : R.style.TextAppearance_TextMediumThick_Primary;
    }

    /**
     * Returns the {@link ColorStateList} to use for the tab grid card action button based on
     * incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The {@link ColorStateList} for tab grid card action button.
     */
    public static ColorStateList getActionButtonTintList(Context context, boolean isIncognito) {
        return AppCompatResources.getColorStateList(context,
                isIncognito ? R.color.tab_grid_card_action_button_tint_color_incognito
                            : R.color.tab_grid_card_action_button_tint_color);
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
     * @return The mini-thumbnail placeholder color.
     */
    @ColorInt
    public static int getMiniThumbnailPlaceHolderColor(Context context, boolean isIncognito) {
        return ApiCompatibilityUtils.getColor(context.getResources(),
                isIncognito ? R.color.tab_list_mini_card_default_background_color_incognito
                            : R.color.tab_list_mini_card_default_background_color);
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
     * @return The {@link ColorStateList} for hovered tab grid card background.
     */
    public static ColorStateList getHoveredCardBackgroundTintList(
            Context context, boolean isIncognito) {
        return AppCompatResources.getColorStateList(context,
                isIncognito ? R.color.hovered_tab_grid_card_background_color_incognito
                            : R.color.hovered_tab_grid_card_background_color);
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
}
